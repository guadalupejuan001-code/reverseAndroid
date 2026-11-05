// Included after MinimalGuest's declaration, inside reverse_pc::guest.
inline void MinimalGuest::validate_system_configuration() const {
  if(machine_.pci.enabled){
    for(const auto& d:machine_.pci.devices)for(const auto& q:d.queues)if(q.enabled){require(q.size&&q.size<=256&&!(q.size&(q.size-1))&&!(q.desc&15)&&!(q.avail&1)&&!(q.used&3),"Invalid restored virtqueue alignment/size");range(q.desc,q.size*16);range(q.avail,6+q.size*2);range(q.used,6+q.size*8);}
    for(const auto& r:machine_.pci.resources)for(const auto& e:r.backing)range(e.address,e.length);
  }
  require(!(machine_.sctlr&((UINT64_C(1)<<25)|(UINT64_C(1)<<24))),"Big-endian system memory is not implemented");
  if(machine_.sctlr&1){
    const auto& m=machine_;
    require(((m.tcr>>14)&3)==0 && ((m.tcr>>30)&3)==2,"System MMU supports only 4 KiB translation granules");
    require((m.tcr&63)>=16 && (m.tcr&63)<=39 && ((m.tcr>>16)&63)>=16 && ((m.tcr>>16)&63)<=39,
            "System MMU supports 25..48-bit virtual addresses");
    require(!(m.tcr&(UINT64_C(3)<<39)),"Hardware access/dirty flag updates are not implemented");
  }
}
inline std::uint64_t MinimalGuest::translate(std::uint64_t address,bool writing,bool executing,bool unprivileged) const {
  if(!machine_.enabled || !(machine_.sctlr&1))return address;
  const auto& m=machine_;
  const bool permission_el0=unprivileged||!m.el;
  // EC always reflects the actual current EL, even for LDTR/STTR.
  const auto ec=executing?(m.el?0x21u:0x20u):(m.el?0x25u:0x24u);
  auto fault=[&](unsigned status){throw MachineFault{(std::uint64_t(ec)<<26)|(1u<<25)|(!executing&&writing?64u:0u)|status,address};};
#ifndef REVERSE_PC_TRANSLATION_REFERENCE
  const auto page=address>>12;
  const unsigned mode=unsigned(writing)|(unsigned(executing)<<1)|
                      (unsigned(unprivileged)<<2)|(m.el<<3);
  auto& hint=translation_hints_[(page^(page>>12)^mode)&63];
  if(hint.generation==translation_generation_&&hint.page==page&&hint.mode==mode&&
     hint.ttbr0==m.ttbr0&&hint.ttbr1==m.ttbr1&&hint.tcr==m.tcr&&hint.sctlr==m.sctlr)
    return hint.physical|(address&4095);
#endif
  const bool upper=(address>>55)&1;
  auto canonical=address;
  if(m.tcr&(UINT64_C(1)<<(upper?38:37)))canonical=upper?address|UINT64_C(0xff00000000000000):address&UINT64_C(0x00ffffffffffffff);
  const unsigned size=64-unsigned((m.tcr>>(upper?16:0))&63);
  const auto va_mask=(UINT64_C(1)<<size)-1;
  if((upper?(canonical|va_mask)!=UINT64_MAX:(canonical&~va_mask)!=0))fault(0);
  const unsigned start=4-(size-12+8)/9;
  if(m.tcr&(UINT64_C(1)<<(upper?23:7)))fault(4+start);
  auto table=(upper?m.ttbr1:m.ttbr0)&UINT64_C(0x0000fffffffff000);
  bool deny_user=false,read_only=false,pxn=false,uxn=false;
  for(unsigned level=start;level<4;++level){
    const unsigned shift=12+(3-level)*9;
    const auto slot=table+((canonical>>shift)&511)*8;
    if(slot<MachineState::ram_base || slot-MachineState::ram_base>ram_ || ram_-(slot-MachineState::ram_base)<8)fault(0x14+level);
    note_translation_table_page(slot);
    const auto descriptor=read(slot,8);
    if(!(descriptor&1))fault(4+level);
    if(level<3 && (descriptor&2)){
      table=descriptor&UINT64_C(0x0000fffffffff000);
      deny_user|=bool(descriptor&(UINT64_C(1)<<61));read_only|=bool(descriptor&(UINT64_C(1)<<62));
      pxn|=bool(descriptor&(UINT64_C(1)<<59));uxn|=bool(descriptor&(UINT64_C(1)<<60));continue;
    }
    if(level==0 || (level==3 && !(descriptor&2)))fault(4+level);
    const auto offset_mask=(UINT64_C(1)<<shift)-1;
    if(descriptor&((offset_mask&~UINT64_C(4095))))fault(4+level);
    if(!(descriptor&(UINT64_C(1)<<10)))fault(8+level);
    const bool user=(descriptor&(1u<<6))&&!deny_user;
    const bool readonly=(descriptor&(1u<<7))||read_only;
    if((permission_el0&&!user)||(writing&&readonly))fault(12+level);
    // AArch64 always forbids privileged execution from a region writable by
    // EL0, independently of PXN and SCTLR.WXN (Arm VMSA permission rules).
    if(executing&&m.el&&user&&!readonly)fault(12+level);
    if(executing && ((m.el?(pxn||bool(descriptor&(UINT64_C(1)<<53))):(uxn||bool(descriptor&(UINT64_C(1)<<54)))) ||
                    ((m.sctlr&(1u<<19))&&!readonly)))fault(12+level);
    const auto physical=(descriptor&UINT64_C(0x0000fffffffff000)&~offset_mask)|(canonical&offset_mask);
#ifndef REVERSE_PC_TRANSLATION_REFERENCE
    hint={translation_generation_,page,physical&~UINT64_C(4095),
          m.ttbr0,m.ttbr1,m.tcr,m.sctlr,mode};
#endif
    return physical;
  }
  fault(4);return 0;
}
inline bool MinimalGuest::device_address(std::uint64_t address) const {
  return (machine_.pci.enabled&&((address>=GuestPciState::ecam&&address<GuestPciState::ecam+GuestPciState::ecam_size)||(address>=GuestPciState::mmio&&address<GuestPciState::mmio+GuestPciState::mmio_size)))||
         (address>=MachineState::uart_base&&address<MachineState::uart_base+4096)||
         (address>=MachineState::gic_dist&&address<MachineState::gic_dist+4096)||
         (address>=MachineState::gic_cpu&&address<MachineState::gic_cpu+4096);
}
inline void MinimalGuest::check_access(std::uint64_t address,unsigned size,bool writing,bool executing,bool unprivileged) const {
  (void)resolve_access(address,size,writing,executing,unprivileged);
}
inline std::array<std::uint64_t,16> MinimalGuest::resolve_access(std::uint64_t address,unsigned size,bool writing,bool executing,bool unprivileged) const {
  require(size&&size<=16 && address<=UINT64_MAX-(size-1),"Invalid CPU access extent");
  std::array<std::uint64_t,16> resolved{};
  if(!machine_.enabled){range(address,size);for(unsigned i=0;i<size;++i)resolved[i]=address+i;return resolved;}
  if((machine_.sctlr&2)&&(address&(size-1))){
    const auto ec=executing?(machine_.el?0x21:0x20):(machine_.el?0x25:0x24);
    throw MachineFault{(std::uint64_t(ec)<<26)|(1u<<25)|(writing?64u:0u)|0x21,address};
  }
  for(unsigned i=0;i<size;++i){
    // A supported 4 KiB stage-1 mapping is linear within its smallest page.
    // Reuse only during this one access, never across instructions or stores.
#ifdef REVERSE_PC_BYTEWISE_ACCESS_REFERENCE
    const auto physical=translate(address+i,writing,executing,unprivileged);
#else
    const auto physical=i&&((address+i)&4095)?resolved[i-1]+1:translate(address+i,writing,executing,unprivileged);
#endif
    resolved[i]=physical;
    const bool ram=physical>=MachineState::ram_base&&physical-MachineState::ram_base<ram_;
    if(!ram && (!device_address(physical)||executing)){
      const auto ec=executing?(machine_.el?0x21:0x20):(machine_.el?0x25:0x24);
      throw MachineFault{(std::uint64_t(ec)<<26)|(1u<<25)|(writing&&!executing?64u:0u)|0x10,address+i};
    }
    if(!ram){
      const auto first=resolved[0];
      const bool pci=machine_.pci.enabled&&((first>=GuestPciState::ecam&&first<GuestPciState::ecam+GuestPciState::ecam_size)||(first>=GuestPciState::mmio&&first<GuestPciState::mmio+GuestPciState::mmio_size));
      require(size<=(pci?8u:4u) && !(address&(size-1)) && physical==first+i,"Unsupported split or unaligned device access");
    }
  }
  return resolved;
}
inline std::uint64_t MinimalGuest::cpu_read(std::uint64_t address,unsigned size,RunResult&,bool executing,bool unprivileged) {
  const auto physical=resolve_access(address,size,false,executing,unprivileged);
  if(!machine_.enabled)return read(address,size);
  const auto first=physical[0];
  if(device_address(first))return device_read(first,size)&low_mask(size*8);
  std::uint64_t value=0;for(unsigned i=0;i<size;++i)value|=std::uint64_t(read8(physical[i]))<<(8*i);return value;
}
inline void MinimalGuest::cpu_write(std::uint64_t address,std::uint64_t value,unsigned size,RunResult& result,bool unprivileged) {
  const auto physical=resolve_access(address,size,true,false,unprivileged);machine_.exclusive_address=UINT64_MAX;machine_.exclusive_size=0;
  // Include table pages used by validation itself: the store can replace
  // the descriptor that resolved its own destination, including its tail.
  invalidate_cpu_store_hints(physical,size);invalidate_code_physical(physical,size);invalidate_data_physical(physical,size);
  if(!machine_.enabled){write(address,value,size);return;}
  const auto first=physical[0];
  if(device_address(first)){device_write(first,value&low_mask(size*8),size,result);return;}
  auto store=[&](mundo_x::QuantumRuntime::ByteAccess& bytes){
    for(unsigned i=0;i<size;++i){const auto delta=std::uint8_t(value>>(8*i))^backing(physical[i]);
      store_inverse_byte(bytes,physical[i],delta);}
  };
  if(byte_transaction_)store(*byte_transaction_);else runtime_.transact_bytes(store);
}
inline std::uint64_t MinimalGuest::device_read(std::uint64_t address,unsigned size) {
  auto& m=machine_;
  if(m.pci.enabled&&((address>=GuestPciState::ecam&&address<GuestPciState::ecam+GuestPciState::ecam_size)||(address>=GuestPciState::mmio&&address<GuestPciState::mmio+GuestPciState::mmio_size)))return pci_read(address,size);
  if(address>=MachineState::uart_base&&address<MachineState::uart_base+4096){
    const auto offset=unsigned(address-MachineState::uart_base);
    if(offset==0){if(m.receive.empty())return 0;const auto value=std::uint8_t(m.receive.front());m.receive.erase(0,1);return value;}
    if(offset==0x18)return 0x80|(m.receive.empty()?16u:0u);
    if(offset==0x24)return m.uart_ibrd;if(offset==0x28)return m.uart_fbrd;if(offset==0x2c)return m.uart_lcr;
    if(offset==0x30)return m.uart_cr;if(offset==0x38)return m.uart_imsc;
    if(offset==0x3c)return m.receive.empty()?0:16;
    if(offset==0x40)return m.receive.empty()?0:m.uart_imsc&16;
    const std::array<unsigned,8> id{0x11,0x10,0x14,0,0x0d,0xf0,0x05,0xb1};
    if(offset>=0xfe0 && !(offset&3))return id[(offset-0xfe0)/4];
    if(offset==0x34)return m.uart_ifls;
    if(offset==4||offset==0x48)return 0;
    throw std::runtime_error("Unsupported PL011 register read "+hex(offset));
  }
  if(address>=MachineState::gic_cpu&&address<MachineState::gic_cpu+4096){
    require(size==4,"GIC CPU registers require 32-bit access");
    const auto offset=unsigned(address-MachineState::gic_cpu);
    if(offset==0)return m.cpu_ctl;if(offset==4)return m.pmr;if(offset==8)return m.bpr;
    if(offset==0xc){const auto irq=m.next_irq();if(irq<64){m.pending_irqs[irq/32]&=~(1u<<(irq%32));m.active_irqs[irq/32]|=1u<<(irq%32);}return irq;}
    if(offset==0x18)return m.next_irq();if(offset==0xfc)return 0x0202043b;
    if(offset==0x14){unsigned priority=255;for(unsigned i=0;i<64;++i)if(m.active_irqs[i/32]&(1u<<(i%32)))priority=std::min(priority,unsigned(m.priority[i]));return priority;}
    if(offset>=0xd0&&offset<=0xdc){require(!(m.active_irqs[0]|m.active_irqs[1]),"GIC APR reads with active IRQs are not implemented");return 0;}
    throw std::runtime_error("Unsupported GIC CPU register read "+hex(offset));
  }
  const auto offset=unsigned(address-MachineState::gic_dist);
  require(size==4||(offset>=0x400&&offset+size<=0x440)||(offset>=0x800&&offset+size<=0x840),"GIC distributor control registers require 32-bit access");
  if(offset==0)return m.dist_ctl;if(offset==4)return 1; // 64 interrupt IDs, 1 CPU
  if(offset==8)return 0x0102043b;
  if(offset>=0x80&&offset<0x88)return UINT32_MAX;
  if((offset>=0x100&&offset<0x108)||(offset>=0x180&&offset<0x188))return m.enabled_irqs[(offset&127)/4];
  if((offset>=0x200&&offset<0x208)||(offset>=0x280&&offset<0x288)){const auto word=(offset&127)/4;return m.pending_irqs[word]|m.level_irqs(word);}
  if((offset>=0x300&&offset<0x308)||(offset>=0x380&&offset<0x388))return m.active_irqs[(offset&127)/4];
  if((offset>=0x400&&offset+size<=0x440)||(offset>=0x800&&offset+size<=0x840)){
    const auto& values=offset>=0x800?m.targets:m.priority;std::uint64_t value=0;
    for(unsigned i=0;i<size;++i)value|=std::uint64_t(values[(offset&255)+i])<<(i*8);return value;
  }
  if(offset>=0xc00&&offset<0xc10&&size==4)return m.irq_config[(offset-0xc00)/4];
  if(offset==0xfe8)return 0x20;
  throw std::runtime_error("Unsupported GIC distributor register read "+hex(offset));
}
inline void MinimalGuest::device_write(std::uint64_t address,std::uint64_t value,unsigned size,RunResult& result) {
  auto& m=machine_;
  if(m.pci.enabled&&((address>=GuestPciState::ecam&&address<GuestPciState::ecam+GuestPciState::ecam_size)||(address>=GuestPciState::mmio&&address<GuestPciState::mmio+GuestPciState::mmio_size))){pci_write(address,value,size);return;}
  if(address>=MachineState::uart_base&&address<MachineState::uart_base+4096){
    const auto offset=unsigned(address-MachineState::uart_base);
    if(offset==0){require(result.output.size()+result.error_output.size()<max_output,"Serial output budget exceeded");result.output.push_back(char(value&255));return;}
    if(offset==0x24){m.uart_ibrd=unsigned(value)&65535;return;}if(offset==0x28){m.uart_fbrd=unsigned(value)&63;return;}
    if(offset==0x2c){m.uart_lcr=unsigned(value)&255;return;}if(offset==0x30){m.uart_cr=unsigned(value)&65535;return;}
    if(offset==0x38){m.uart_imsc=unsigned(value)&0x7ff;return;}
    if(offset==4||offset==0x44)return;
    if(offset==0x34){
      // Linux uses STRH for IFLS: accept byte, halfword and word MMIO.
      // Only the legal PL011 TX/RX trigger fractions (0..4) are supported.
      // Persist and report configuration; the bounded serial transport still
      // drains TX synchronously and does not model per-byte physical latency.
      require((size==1 || size==2 || size==4) && !(value&~UINT64_C(63)) && (value&7)<=4 && ((value>>3)&7)<=4,
              "Invalid PL011 FIFO interrupt thresholds");
      m.uart_ifls=unsigned(value);return;
    }
    throw std::runtime_error("Unsupported PL011 register write "+hex(offset));
  }
  if(address>=MachineState::gic_cpu&&address<MachineState::gic_cpu+4096){
    require(size==4,"GIC CPU registers require 32-bit access");const auto offset=unsigned(address-MachineState::gic_cpu);
    if(offset==0){require((value&~UINT64_C(1))==0,"Only GICv2 single-group mode is implemented");m.cpu_ctl=unsigned(value);return;}
    if(offset==4){m.pmr=unsigned(value)&255;return;}if(offset==8){m.bpr=unsigned(value)&7;return;}
    if(offset==0x10){const auto irq=unsigned(value)&1023;require(irq<64,"Invalid GIC end-of-interrupt ID");m.active_irqs[irq/32]&=~(1u<<(irq%32));return;}
    if(offset>=0xd0&&offset<=0xdc){
      require(value==0&&!(m.active_irqs[0]|m.active_irqs[1]),"Only GIC APR zero initialization before active IRQs is implemented");return;
    }
    throw std::runtime_error("Unsupported GIC CPU register write "+hex(offset));
  }
  const auto offset=unsigned(address-MachineState::gic_dist);
  require(size==4||(offset>=0x400&&offset+size<=0x440)||(offset>=0x800&&offset+size<=0x840),"GIC distributor control registers require 32-bit access");
  if(offset==0){require((value&~UINT64_C(1))==0,"Only GICv2 single-group mode is implemented");m.dist_ctl=unsigned(value);return;}
  if(offset>=0x80&&offset<0x88)return; // Security extensions are not exposed.
  if(offset>=0x100&&offset<0x108&&size==4){m.enabled_irqs[(offset-0x100)/4]|=std::uint32_t(value);return;}
  if(offset>=0x180&&offset<0x188&&size==4){m.enabled_irqs[(offset-0x180)/4]&=~std::uint32_t(value);return;}
  if(offset>=0x200&&offset<0x208&&size==4){m.pending_irqs[(offset-0x200)/4]|=std::uint32_t(value);return;}
  if(offset>=0x280&&offset<0x288&&size==4){m.pending_irqs[(offset-0x280)/4]&=~std::uint32_t(value);return;}
  if(offset>=0x300&&offset<0x308&&size==4){m.active_irqs[(offset-0x300)/4]|=std::uint32_t(value);return;}
  if(offset>=0x380&&offset<0x388&&size==4){m.active_irqs[(offset-0x380)/4]&=~std::uint32_t(value);return;}
  if((offset>=0x400&&offset+size<=0x440)||(offset>=0x800&&offset+size<=0x840)){
    auto& values=offset>=0x800?m.targets:m.priority;for(unsigned i=0;i<size;++i)values[(offset&255)+i]=std::uint8_t(value>>(8*i));return;
  }
  if(offset>=0xc00&&offset<0xc10&&size==4){m.irq_config[(offset-0xc00)/4]=std::uint32_t(value)&0xaaaaaaaa;return;}
  if(offset==0xf00){const auto irq=unsigned(value)&15;if((value&(1u<<17))||((value>>16)&1))m.pending_irqs[0]|=1u<<irq;return;}
  throw std::runtime_error("Unsupported GIC distributor register write "+hex(offset));
}
inline unsigned guest_sysreg(unsigned op0,unsigned op1,unsigned crn,unsigned crm,unsigned op2){return (op0<<14)|(op1<<11)|(crn<<7)|(crm<<3)|op2;}
inline std::uint64_t MinimalGuest::system_read(unsigned id,unsigned rt) {
  auto& m=machine_;
  if(id==guest_sysreg(3,3,4,4,0)||id==guest_sysreg(3,3,4,4,1)){fp_access();return id==guest_sysreg(3,3,4,4,0)?floating_.fpcr:floating_.fpsr;}
  if(id==guest_sysreg(3,3,4,2,0))return std::uint64_t(registers_.nzcv)<<28;
  if(!m.enabled)throw std::runtime_error("System registers require the ARM64 compatibility system mode");
  // DCZID is readable at EL0. DZP reports the guest kernel's DZE policy;
  // BS=4 retains this machine's original 64-byte DC ZVA block size.
  if(id==guest_sysreg(3,3,0,0,7))return 4u|(!m.el&&!(m.sctlr&(UINT64_C(1)<<14))?16u:0u);
  if(!m.el&&id==guest_sysreg(3,3,0,0,1)){
    if(!(m.sctlr&(UINT64_C(1)<<15)))throw MachineFault{guest_sys64_syndrome(id,rt,true),0};
    return 0x8444c004;
  }
  if(!m.el && id!=guest_sysreg(3,3,13,0,2)&&id!=guest_sysreg(3,3,13,0,3)&&id!=guest_sysreg(3,3,4,2,0)&&
     !(id==guest_sysreg(3,3,14,0,1)&&(m.cntkctl&1))&&!(id==guest_sysreg(3,3,14,0,2)&&(m.cntkctl&2)))throw MachineFault{1u<<25,0};
  if(id==guest_sysreg(3,0,0,0,0))return 0x410fd034; // MIDR, Cortex-A53 model, one core.
  if(id==guest_sysreg(3,0,0,0,5))return UINT64_C(1)<<31; // MPIDR
  if(id==guest_sysreg(3,0,0,0,6))return 0; // REVIDR: this software model has no implementation revision.
  if(id==guest_sysreg(3,0,0,4,0))return 0x11; // A64 EL0/EL1 and baseline FP/SIMD; unsupported opcodes fail.
  // ID_AA64ISAR3_EL1 (S3_0_C0_C6_3): no CPA/FAMINMAX/TLBIW/PACM
  // or newer ISAR3 extensions in this baseline CPU. Keep EL0 access rejected
  // by the privilege check above; the read does not add instruction support.
  if(id==guest_sysreg(3,0,0,6,3))return 0;
  // Exact additional reads in the same genuine Android CPU-info routine:
  // PFR2: no FPMR/MTE extensions; FPFR0: no FP8 extensions. MMFR4 is
  // baseline RES0 here; PFR0 already reports that EL2 is absent, so its
  // E2H0 encoding cannot advertise an unimplemented hypervisor mode.
  if(id==guest_sysreg(3,0,0,4,2)||id==guest_sysreg(3,0,0,4,7)||
     id==guest_sysreg(3,0,0,7,4))return 0;
  if(id==guest_sysreg(3,0,0,4,1)||id==guest_sysreg(3,0,0,4,4)||id==guest_sysreg(3,0,0,4,5)|| // PFR1/ZFR0/SMFR0: extensions absent.
     id==guest_sysreg(3,0,0,5,0)||id==guest_sysreg(3,0,0,5,1)||
     id==guest_sysreg(3,0,0,6,0)||id==guest_sysreg(3,0,0,6,1)||id==guest_sysreg(3,0,0,6,2)||
     id==guest_sysreg(3,0,0,7,1)||id==guest_sysreg(3,0,0,7,2)||
     id==guest_sysreg(3,0,0,7,3))return 0; // ID_AA64MMFR3_EL1: advanced memory features absent.
  if(id==guest_sysreg(3,0,0,7,0))return 0x0ff00005; // 48-bit PA, 4 KiB only.
  if(id==guest_sysreg(3,3,0,0,1))return 0x8444c004; // CTR: 64-byte coherent cache lines.
  if(id==guest_sysreg(3,1,0,0,1))return 0; // CLIDR: no modeled caches.
  if(id==guest_sysreg(3,0,4,2,2))return m.el<<2;
  if(id==guest_sysreg(3,0,4,2,0))return m.sp_select;
  if(id==guest_sysreg(3,3,4,2,0))return std::uint64_t(registers_.nzcv)<<28;
  if(id==guest_sysreg(3,3,4,2,1))return m.daif;
  if(id==guest_sysreg(3,0,4,1,0))return m.el?m.sp0:registers_.sp;
  if(id==guest_sysreg(3,0,1,0,0))return m.sctlr;if(id==guest_sysreg(3,0,1,0,2))return m.cpacr;
  if(id==guest_sysreg(3,0,2,0,0))return m.ttbr0;if(id==guest_sysreg(3,0,2,0,1))return m.ttbr1;
  if(id==guest_sysreg(3,0,2,0,2))return m.tcr;if(id==guest_sysreg(3,0,10,2,0))return m.mair;
  if(id==guest_sysreg(3,0,12,0,0))return m.vbar;if(id==guest_sysreg(3,0,4,0,1))return m.elr;
  if(id==guest_sysreg(3,0,4,0,0))return m.spsr;if(id==guest_sysreg(3,0,5,2,0))return m.esr;
  if(id==guest_sysreg(3,0,6,0,0))return m.far;if(id==guest_sysreg(3,0,7,4,0))return m.par;
  if(id==guest_sysreg(3,0,13,0,1))return m.contextidr;if(id==guest_sysreg(3,0,13,0,4))return m.tpidr1;
  if(id==guest_sysreg(3,3,13,0,2))return m.tpidr0;if(id==guest_sysreg(3,3,13,0,3))return m.tpidrro;
  if(id==guest_sysreg(3,0,14,1,0))return m.cntkctl;
  if(id==guest_sysreg(3,3,14,0,0))return 1000000;
  if(id==guest_sysreg(3,3,14,0,1)||id==guest_sysreg(3,3,14,0,2)){
    if(m.counter_latency.enabled){const auto delay=m.counter_latency.next();
      require(delay<=UINT64_MAX-m.ticks,"Guest virtual timer overflow");m.ticks+=delay;
    }
    return m.ticks;
  }
  if(id==guest_sysreg(3,3,14,2,0))return std::uint32_t(m.cntp_cval-m.ticks);
  if(id==guest_sysreg(3,3,14,3,0))return std::uint32_t(m.cntv_cval-m.ticks);
  if(id==guest_sysreg(3,3,14,2,1))return m.timer_control(true);if(id==guest_sysreg(3,3,14,3,1))return m.timer_control(false);
  if(id==guest_sysreg(3,3,14,2,2))return m.cntp_cval;if(id==guest_sysreg(3,3,14,3,2))return m.cntv_cval;
  if(id==guest_sysreg(2,0,0,2,2))return m.mdscr;
  if(id==guest_sysreg(2,0,0,2,0)||id==guest_sysreg(3,3,9,14,0))return 0;
  // DFR0's zero count fields encode one breakpoint/watchpoint slot. The
  // inactive slot is readable, but enabling hardware debug remains rejected.
  if(id>=guest_sysreg(2,0,0,0,4)&&id<=guest_sysreg(2,0,0,0,7))return 0;
  throw std::runtime_error("Unsupported AArch64 system register read "+hex(id));
}
inline void MinimalGuest::select_stack(unsigned el,unsigned selected) {
  auto& m=machine_;if(m.el&&m.sp_select)m.sp1=registers_.sp;else m.sp0=registers_.sp;
  m.el=el;m.sp_select=selected;registers_.sp=el&&selected?m.sp1:m.sp0;
}
inline void MinimalGuest::system_write(unsigned id,std::uint64_t value) {
  auto& m=machine_;
  if(id==guest_sysreg(3,3,4,4,0)||id==guest_sysreg(3,3,4,4,1)){
    fp_access();if(id==guest_sysreg(3,3,4,4,0)){require(!(value&(1u<<26)),"Alternative FP16 format is not implemented");floating_.fpcr=std::uint32_t(value)&0x03c00000;}
    else floating_.fpsr=std::uint32_t(value)&0x0800009f;return;
  }
  if(id==guest_sysreg(3,3,4,2,0)){registers_.nzcv=unsigned(value>>28)&15;return;}
  require(m.enabled,"System registers require ARM64 system mode");
  if(id==guest_sysreg(3,3,13,0,2)){m.tpidr0=value;return;}
  if(id==guest_sysreg(3,3,4,2,0)){registers_.nzcv=unsigned(value>>28)&15;return;}
  if(!m.el)throw MachineFault{1u<<25,0};
  if(id==guest_sysreg(3,0,4,2,0)){select_stack(m.el,unsigned(value)&1);return;}
  if(id==guest_sysreg(3,3,4,2,1)){m.daif=unsigned(value)&0x3c0;return;}
  if(id==guest_sysreg(3,0,4,1,0)){m.sp0=value;if(!m.sp_select)registers_.sp=value;return;}
  if(id==guest_sysreg(3,0,1,0,0)){auto previous=m.sctlr;m.sctlr=value;try{validate_system_configuration();}catch(...){m.sctlr=previous;throw;}return;}
  if(id==guest_sysreg(3,0,2,0,2)){auto previous=m.tcr;m.tcr=value;try{validate_system_configuration();}catch(...){m.tcr=previous;throw;}return;}
  if(id==guest_sysreg(3,0,2,0,0)){m.ttbr0=value;return;}if(id==guest_sysreg(3,0,2,0,1)){m.ttbr1=value;return;}
  if(id==guest_sysreg(3,0,10,2,0)){m.mair=value;return;}if(id==guest_sysreg(3,0,1,0,2)){m.cpacr=value;return;}
  if(id==guest_sysreg(3,0,12,0,0)){m.vbar=value&~UINT64_C(2047);return;}if(id==guest_sysreg(3,0,4,0,1)){m.elr=value;return;}
  if(id==guest_sysreg(3,0,4,0,0)){m.spsr=value;return;}if(id==guest_sysreg(3,0,5,2,0)){m.esr=value;return;}
  if(id==guest_sysreg(3,0,6,0,0)){m.far=value;return;}if(id==guest_sysreg(3,0,7,4,0)){m.par=value;return;}
  if(id==guest_sysreg(3,0,13,0,1)){m.contextidr=value;return;}if(id==guest_sysreg(3,0,13,0,4)){m.tpidr1=value;return;}
  if(id==guest_sysreg(3,3,13,0,3)){m.tpidrro=value;return;}if(id==guest_sysreg(3,0,14,1,0)){m.cntkctl=value;return;}
  if(id==guest_sysreg(3,3,14,2,0)){m.cntp_cval=m.ticks+std::uint64_t(signed_field(value&0xffffffff,32));return;}
  if(id==guest_sysreg(3,3,14,3,0)){m.cntv_cval=m.ticks+std::uint64_t(signed_field(value&0xffffffff,32));return;}
  if(id==guest_sysreg(3,3,14,2,1)){m.cntp_ctl=unsigned(value)&3;return;}if(id==guest_sysreg(3,3,14,3,1)){m.cntv_ctl=unsigned(value)&3;return;}
  if(id==guest_sysreg(3,3,14,2,2)){m.cntp_cval=value;return;}if(id==guest_sysreg(3,3,14,3,2)){m.cntv_cval=value;return;}
  if(id==guest_sysreg(3,2,0,0,0))return; // CSSELR: no data caches modeled.
  if(id==guest_sysreg(2,0,0,2,2)){
    require(!(value&~UINT64_C(0x1000)),"Hardware debug/stepping is not implemented");m.mdscr=value;return;
  }
  if(id>=guest_sysreg(2,0,0,0,4)&&id<=guest_sysreg(2,0,0,0,7)){
    // Linux hw_breakpoint_reset clears BCR0/BVR0/WCR0/WVR0 on cold boot.
    // Preserve only this defined inactive state; never silently enable a
    // breakpoint, watchpoint or unimplemented additional slot.
    require(value==0,"Hardware breakpoint/watchpoint programming is not implemented");return;
  }
  if(id==guest_sysreg(2,0,0,2,0)||id==guest_sysreg(2,0,1,0,4)||
     id==guest_sysreg(2,0,1,3,4)||id==guest_sysreg(3,3,9,14,0)){
    // No debug/PMU feature is advertised. Clearing its inactive control state
    // is defined; attempts to enable an unimplemented feature fail explicitly.
    require(value==0,"Debug/performance monitoring is not implemented");return;
  }
  throw std::runtime_error("Unsupported AArch64 system register write "+hex(id));
}
inline void MinimalGuest::exception(std::uint64_t syndrome,std::uint64_t address,std::uint64_t return_pc,bool irq) {
  auto& m=machine_;require(m.enabled,"Architectural exception outside system mode");
  const auto offset=m.el?(m.sp_select?0x200u:0u):0x400u;
  m.spsr=(std::uint64_t(registers_.nzcv)<<28)|m.daif|(m.el?(4u|m.sp_select):0u);
  m.elr=return_pc;if(!irq){m.esr=syndrome;m.far=address;}
  select_stack(1,1);m.daif=0x3c0;m.waiting=false;m.exclusive_address=UINT64_MAX;m.exclusive_size=0;
  registers_.pc=m.vbar+offset+(irq?0x80:0);require(m.exceptions!=UINT64_MAX,"Exception counter overflow");++m.exceptions;
}
