#pragma once
// Architectural machine state. These are finite registers/device rules, not a
// second RAM store. Physical bytes remain in MinimalGuest's XI/Ether authority.
#include <array>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>
#include "minimal_guest_pci_state.hpp"
#include "minimal_guest_counter.hpp"

namespace reverse_pc::guest {
// ESR_EL1 EC=0x18, IL=1, ISS for an AArch64 trapped MRS/MSR/SYS.
// Register identity and Rt are carried to the guest's actual sysinstr handler.
inline std::uint64_t guest_sys64_syndrome(unsigned id,unsigned rt,bool reading){
  return (UINT64_C(0x18)<<26)|(UINT64_C(1)<<25)|
    (std::uint64_t((id>>14)&3)<<20)|(std::uint64_t(id&7)<<17)|
    (std::uint64_t((id>>11)&7)<<14)|(std::uint64_t((id>>7)&15)<<10)|
    (std::uint64_t(rt&31)<<5)|(std::uint64_t((id>>3)&15)<<1)|(reading?1u:0u);
}
struct MachineFault {
  std::uint64_t syndrome=0, address=0;
};

struct MachineState {
  static constexpr std::uint64_t ram_base=0x40000000, uart_base=0x09000000;
  static constexpr std::uint64_t gic_dist=0x08000000, gic_cpu=0x08010000;
  bool enabled=false, waiting=false, event=false;
  unsigned el=1, sp_select=1, daif=0x3c0, image_kind=0;
  std::uint64_t sp0=0,sp1=0,sctlr=0x30d00800,tcr=0,ttbr0=0,ttbr1=0,mair=0;
  std::uint64_t vbar=0,elr=0,spsr=0,esr=0,far=0,par=0,cpacr=0,contextidr=0;
  std::uint64_t tpidr0=0,tpidrro=0,tpidr1=0,cntkctl=0,ticks=0,exceptions=0;
  std::uint64_t cntp_cval=0,cntv_cval=0;
  std::uint64_t exclusive_address=UINT64_MAX;
  unsigned exclusive_size=0;
  unsigned cntp_ctl=0,cntv_ctl=0;
  unsigned dist_ctl=0,cpu_ctl=0,pmr=255,bpr=0;
  std::array<std::uint32_t,2> enabled_irqs{},pending_irqs{},active_irqs{};
  std::array<std::uint8_t,64> priority{},targets{};
  std::array<std::uint32_t,4> irq_config{};
  unsigned uart_cr=0x301,uart_ibrd=0,uart_fbrd=0,uart_lcr=0,uart_imsc=0,uart_ifls=0;
  std::uint64_t mdscr=0;
  std::string receive;
  GuestPciState pci;
  GuestCounterLatency counter_latency;

  MachineState() { priority.fill(0xa0);targets.fill(1); }
  bool timer_pending(bool physical) const {
    const auto control=physical?cntp_ctl:cntv_ctl;
    return (control&1) && ticks>=(physical?cntp_cval:cntv_cval);
  }
  unsigned timer_control(bool physical) const {
    return (physical?cntp_ctl:cntv_ctl)|(timer_pending(physical)?4u:0u);
  }
  std::uint32_t level_irqs(unsigned word) const {
    if(word==1)return (!receive.empty()&&(uart_imsc&16)?2u:0u)|
      (pci.irq(0)?1u<<16:0u)|(pci.irq(1)?1u<<17:0u); // SPI 33,48,49
    return (timer_pending(true)&&!(cntp_ctl&2)?1u<<30:0u)|
           (timer_pending(false)&&!(cntv_ctl&2)?1u<<27:0u);
  }
  unsigned next_irq_reference() const {
    if(!(dist_ctl&1)||!(cpu_ctl&1))return 1023;
    unsigned selected=1023, threshold=pmr;
    // Single-core GICv2: do not preempt an equal/higher active priority.
    for(unsigned i=0;i<64;++i)if(active_irqs[i/32]&(1u<<(i%32)))
      if(priority[i]<threshold)threshold=priority[i];
    for(unsigned i=0;i<64;++i)if((enabled_irqs[i/32]&(1u<<(i%32)))&&
      ((pending_irqs[i/32]|level_irqs(i/32))&(1u<<(i%32)))&&
      !(active_irqs[i/32]&(1u<<(i%32)))&&(i<32||(targets[i]&1))&&priority[i]<threshold){
      threshold=priority[i];selected=i;
    }
    return selected;
  }

  unsigned next_irq() const {
#ifdef REVERSE_PC_GIC_SCAN_REFERENCE
    return next_irq_reference();
#else
    if(!(dist_ctl&1)||!(cpu_ctl&1))return 1023;
    unsigned selected=1023,threshold=pmr;
    // Same ascending ID order and strict priority comparison as the original.
    // No result is cached: timer/device levels are reevaluated on every call.
    for(unsigned word=0;word<2;++word){
      auto bits=active_irqs[word];
      while(bits){
        const auto i=32*word+unsigned(__builtin_ctz(bits));bits&=bits-1;
        if(priority[i]<threshold)threshold=priority[i];
      }
    }
    for(unsigned word=0;word<2;++word){
      if(!enabled_irqs[word])continue;
      auto bits=enabled_irqs[word]&(pending_irqs[word]|level_irqs(word))&~active_irqs[word];
      while(bits){
        const auto i=32*word+unsigned(__builtin_ctz(bits));bits&=bits-1;
        if((i<32||(targets[i]&1))&&priority[i]<threshold){threshold=priority[i];selected=i;}
      }
    }
    return selected;
#endif
  }

  std::vector<std::uint8_t> save() const {
    std::vector<std::uint8_t> bytes;
    auto put=[&](std::uint64_t value){for(unsigned i=0;i<8;++i)bytes.push_back(std::uint8_t(value>>(8*i)));};
    // Preserve byte-for-byte legacy v2 snapshots unless IFLS has a new value.
    // A live 16-byte exclusive monitor uses v4. Historical states retain
    // their exact v2/v3 encoding; no RAM or runtime state is duplicated.
    const unsigned version=counter_latency.enabled?(counter_latency.instructions_per_tick>1?7:6):pci.enabled?5:exclusive_size==16?4:uart_ifls?3:2;
    put(version);put(enabled);put(waiting);put(event);put(el);put(sp_select);put(daif);put(image_kind);
    for(auto value:{sp0,sp1,sctlr,tcr,ttbr0,ttbr1,mair,vbar,elr,spsr,esr,far,par,cpacr,contextidr,
                   tpidr0,tpidrro,tpidr1,cntkctl,ticks,exceptions,cntp_cval,cntv_cval,exclusive_address})put(value);
    put(exclusive_size);
    for(auto value:{cntp_ctl,cntv_ctl,dist_ctl,cpu_ctl,pmr,bpr,uart_cr,uart_ibrd,uart_fbrd,uart_lcr,uart_imsc})put(value);
    for(auto value:enabled_irqs)put(value);
    for(auto value:pending_irqs)put(value);
    for(auto value:active_irqs)put(value);
    for(auto value:irq_config)put(value);
    bytes.insert(bytes.end(),priority.begin(),priority.end());bytes.insert(bytes.end(),targets.begin(),targets.end());
    put(mdscr);if(version>=3)put(uart_ifls);
    put(receive.size());bytes.insert(bytes.end(),receive.begin(),receive.end());
    if(version>=5){const auto p=pci.enabled?pci.save():std::vector<std::uint8_t>{};put(p.size());bytes.insert(bytes.end(),p.begin(),p.end());}
    if(version>=6){const auto c=counter_latency.save();bytes.insert(bytes.end(),c.begin(),c.end());}return bytes;
  }
  static MachineState restore(const std::vector<std::uint8_t>& bytes) {
    std::size_t at=0;
    auto take=[&](){
      if(at>bytes.size()||bytes.size()-at<8)throw std::runtime_error("Truncated machine state");
      std::uint64_t value=0;for(unsigned i=0;i<8;++i)value|=std::uint64_t(bytes[at++])<<(8*i);return value;
    };
    auto small=[&](std::uint64_t maximum){auto value=take();if(value>maximum)throw std::runtime_error("Invalid machine state field");return unsigned(value);};
    const auto version=take();if(version<1||version>7)throw std::runtime_error("Unsupported machine state version");
    MachineState m;m.enabled=small(1);m.waiting=small(1);m.event=small(1);m.el=small(1);m.sp_select=small(1);
    m.daif=small(0x3c0);m.image_kind=small(2);
    if(!m.enabled||(m.daif&~0x3c0u)||(!m.el&&m.sp_select)||!m.image_kind)throw std::runtime_error("Invalid machine execution state");
    for(auto field:{&m.sp0,&m.sp1,&m.sctlr,&m.tcr,&m.ttbr0,&m.ttbr1,&m.mair,&m.vbar,&m.elr,&m.spsr,&m.esr,&m.far,&m.par,&m.cpacr,&m.contextidr,
                    &m.tpidr0,&m.tpidrro,&m.tpidr1,&m.cntkctl,&m.ticks,&m.exceptions,&m.cntp_cval,&m.cntv_cval,&m.exclusive_address})*field=take();
    m.exclusive_size=small(version>=4?16:8);
    if(m.exclusive_size!=0&&m.exclusive_size!=1&&m.exclusive_size!=2&&m.exclusive_size!=4&&m.exclusive_size!=8&&m.exclusive_size!=16)throw std::runtime_error("Invalid exclusive monitor size");
    if(m.exclusive_size==16&&(m.exclusive_address&15))throw std::runtime_error("Unaligned exclusive pair monitor");
    m.cntp_ctl=small(3);m.cntv_ctl=small(3);m.dist_ctl=small(1);m.cpu_ctl=small(1);m.pmr=small(255);m.bpr=small(7);
    m.uart_cr=small(65535);m.uart_ibrd=small(65535);m.uart_fbrd=small(63);m.uart_lcr=small(255);m.uart_imsc=small(0x7ff);
    for(auto& value:m.enabled_irqs)value=std::uint32_t(small(UINT32_MAX));
    for(auto& value:m.pending_irqs)value=std::uint32_t(small(UINT32_MAX));
    for(auto& value:m.active_irqs)value=std::uint32_t(small(UINT32_MAX));
    for(auto& value:m.irq_config)value=std::uint32_t(small(UINT32_MAX));
    if(at>bytes.size()||bytes.size()-at<128)throw std::runtime_error("Truncated interrupt state");
    for(auto& value:m.priority)value=bytes[at++];for(auto& value:m.targets)value=bytes[at++];
    if(version>=2){m.mdscr=take();if(m.mdscr&~UINT64_C(0x1000))throw std::runtime_error("Unsupported debug control state");}
    if(version>=3){m.uart_ifls=small(63);
      if((m.uart_ifls&7)>4||((m.uart_ifls>>3)&7)>4)throw std::runtime_error("Invalid PL011 FIFO threshold state");}
    const auto count=take();
    if(count>4096||count>bytes.size()-at)throw std::runtime_error("Invalid serial input state");
    m.receive.assign(bytes.begin()+at,bytes.begin()+at+std::size_t(count));at+=std::size_t(count);
    if(version>=5){const auto n=take();const auto tail=version>=7?64u:version>=6?48u:0u;
      if(n>196608||bytes.size()-at<tail||n!=bytes.size()-at-tail||(!n&&version==5))throw std::runtime_error("Invalid PCI section");
      if(n)m.pci=GuestPciState::restore(std::vector<std::uint8_t>(bytes.begin()+at,bytes.begin()+at+std::size_t(n)));at+=std::size_t(n);
    }
    if(version>=6){m.counter_latency=GuestCounterLatency::restore(std::vector<std::uint8_t>(bytes.begin()+at,bytes.end()));at=bytes.size();
      if((version==7)!=(m.counter_latency.instructions_per_tick>1))throw std::runtime_error("Counter model differs from machine version");}
    if(at!=bytes.size())throw std::runtime_error("Trailing machine state");
    if(m.vbar&2047)throw std::runtime_error("Unaligned exception vectors in machine state");
    return m;
  }
};

// FDT v17: self-contained 64-bit-address single-core board, with explicit RAM,
// PL011, GICv2 and architected timers. No fabricated Android services/devices.
class GuestDeviceTree {
 public:
  using Data=std::vector<std::uint8_t>;
  Data structure,strings;
  static void be32(Data& data,std::uint32_t value){for(int i=3;i>=0;--i)data.push_back(std::uint8_t(value>>(i*8)));}
  static void be64(Data& data,std::uint64_t value){be32(data,std::uint32_t(value>>32));be32(data,std::uint32_t(value));}
  static void text(Data& data,const std::string& value){data.insert(data.end(),value.begin(),value.end());data.push_back(0);}
  static void align(Data& data){while(data.size()%4)data.push_back(0);}
  void begin(const std::string& name){be32(structure,1);text(structure,name);align(structure);}
  void end(){be32(structure,2);}
  void property(const std::string& name,const Data& data){
    be32(structure,3);be32(structure,std::uint32_t(data.size()));be32(structure,std::uint32_t(strings.size()));text(strings,name);
    structure.insert(structure.end(),data.begin(),data.end());align(structure);
  }
  void string(const std::string& name,const std::string& value){Data data;text(data,value);property(name,data);}
  void cells(const std::string& name,std::initializer_list<std::uint32_t> values){Data data;for(auto value:values)be32(data,value);property(name,data);}
  void reg(std::uint64_t address,std::uint64_t size){Data data;be64(data,address);be64(data,size);property("reg",data);}
  Data finish(){
    be32(structure,9);Data result;
    for(auto value:{0xd00dfeedu,std::uint32_t(56+structure.size()+strings.size()),56u,std::uint32_t(56+structure.size()),40u,17u,16u,0u,std::uint32_t(strings.size()),std::uint32_t(structure.size())})be32(result,value);
    be64(result,0);be64(result,0);result.insert(result.end(),structure.begin(),structure.end());result.insert(result.end(),strings.begin(),strings.end());return result;
  }
};
// Stable boot profiles for the same XI/Ether AArch64 virtual board.
// The Android profile appends the *actual build 16644040* vendor cmdline,
// while retaining REVERSE's real PL011/GIC/timer addresses. These strings
// do not advertise Cuttlefish devices that REVERSE has not implemented.
inline const std::string& guest_default_linux_bootargs() {
  static const std::string args=
      "console=ttyAMA0 earlycon=pl011,0x09000000 keep_bootcon nokaslr maxcpus=1";
  return args;
}
inline const std::string& guest_android_cuttlefish_bootargs() {
  static const std::string args=guest_default_linux_bootargs()+
      " printk.devkmsg=on audit=1 panic=-1 8250.nr_uarts=1"
      " binder.impl=rust cma=0 firmware_class.path=/vendor/etc/"
      " loop.max_part=7 init=/init bootconfig";
  return args;
}
// Explicit fresh-boot profiles, never applied to a running checkpoint.
// The official vendor ramdisk also contains recovery. Android must select
// first_stage_ramdisk and one of its actual packaged fstab suffixes.
inline const std::string& guest_android_cuttlefish_normal_bootargs() {
  static const std::string args=guest_android_cuttlefish_bootargs()+
      " androidboot.force_normal_boot=1 androidboot.fstab_suffix=cf.ext4.cts"
      " androidboot.slot_suffix=_a";
  return args;
}
// Linux itself omits only the optional sched_ext initcall in this variant.
// The original kernel, initrd, ISA, ticks and state authority are intact.
inline const std::string& guest_android_cuttlefish_normal_no_scx_bootargs() {
  static const std::string args=guest_android_cuttlefish_normal_bootargs()+
      " initcall_blacklist=scx_init";
  return args;
}
inline std::vector<std::uint8_t> guest_device_tree(std::uint64_t ram,
    std::uint64_t initrd_address=0,std::uint64_t initrd_size=0,
    const std::string& bootargs={},bool pci=false) {
  GuestDeviceTree tree;tree.begin("");tree.string("compatible","reverse,minimal-arm64-v1");tree.string("model","REVERSE own ARM64 machine");
  tree.cells("#address-cells",{2});tree.cells("#size-cells",{2});tree.cells("interrupt-parent",{1});
  tree.begin("chosen");tree.string("bootargs",bootargs.empty()?guest_default_linux_bootargs():bootargs);tree.string("stdout-path","/pl011@9000000");
  if(initrd_size){GuestDeviceTree::Data data;GuestDeviceTree::be64(data,initrd_address);tree.property("linux,initrd-start",data);data.clear();GuestDeviceTree::be64(data,initrd_address+initrd_size);tree.property("linux,initrd-end",data);}tree.end();
  tree.begin("memory@40000000");tree.string("device_type","memory");tree.reg(MachineState::ram_base,ram);tree.end();
  tree.begin("cpus");tree.cells("#address-cells",{1});tree.cells("#size-cells",{0});tree.begin("cpu@0");tree.string("device_type","cpu");tree.string("compatible","arm,armv8");tree.cells("reg",{0});tree.end();tree.end();
  tree.begin("intc@8000000");tree.string("compatible","arm,cortex-a15-gic");tree.cells("#interrupt-cells",{3});tree.property("interrupt-controller",{});tree.cells("phandle",{1});
  if(pci)tree.cells("#address-cells",{0}); // PCI interrupt-map has no parent address cells.
  GuestDeviceTree::Data ranges;GuestDeviceTree::be64(ranges,MachineState::gic_dist);GuestDeviceTree::be64(ranges,0x10000);GuestDeviceTree::be64(ranges,MachineState::gic_cpu);GuestDeviceTree::be64(ranges,0x10000);tree.property("reg",ranges);tree.end();
  tree.begin("timer");tree.string("compatible","arm,armv8-timer");tree.cells("interrupts",{1,13,0x104,1,14,0x104,1,11,0x104,1,10,0x104});tree.property("always-on",{});tree.end();
  tree.begin("apb-pclk");tree.string("compatible","fixed-clock");tree.cells("#clock-cells",{0});tree.cells("clock-frequency",{24000000});tree.cells("phandle",{2});tree.end();
  tree.begin("pl011@9000000");GuestDeviceTree::Data compatible;GuestDeviceTree::text(compatible,"arm,pl011");GuestDeviceTree::text(compatible,"arm,primecell");tree.property("compatible",compatible);
  tree.reg(MachineState::uart_base,0x1000);tree.cells("interrupts",{0,1,4});tree.cells("clocks",{2,2});GuestDeviceTree::Data names;GuestDeviceTree::text(names,"uartclk");GuestDeviceTree::text(names,"apb_pclk");tree.property("clock-names",names);tree.end();
  if(pci){
    tree.begin("pcie@30000000");tree.string("compatible","pci-host-ecam-generic");tree.string("device_type","pci");
    tree.cells("#address-cells",{3});tree.cells("#size-cells",{2});tree.cells("#interrupt-cells",{1});tree.reg(GuestPciState::ecam,GuestPciState::ecam_size);tree.cells("bus-range",{0,15});
    tree.cells("ranges",{0x02000000,0,0x10000000,0,0x10000000,0,0x10000000});tree.property("dma-coherent",{});
    tree.cells("interrupt-map-mask",{0xf800,0,0,7});tree.cells("interrupt-map",{0x800,0,0,1,1,0,16,4,0x1000,0,0,1,1,0,17,4});tree.end();
  }
  tree.begin("psci");tree.string("compatible","arm,psci-0.2");tree.string("method","hvc");tree.end();tree.end();return tree.finish();
}
} // namespace reverse_pc::guest
