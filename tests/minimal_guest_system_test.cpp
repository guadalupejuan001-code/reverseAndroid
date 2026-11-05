#include "minimal_guest_system_demo.hpp"
#include <functional>
#include <iostream>
using namespace reverse_pc::guest;
namespace {
unsigned checks=0;
void check(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
void fails(const std::function<void()>& action,const char* message){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}check(rejected,message);}
void fault(const MinimalGuest& machine,std::uint64_t address,bool writing,bool executing,unsigned status,const char* message){
  bool rejected=false;try{machine.physical_address(address,writing,executing);}catch(const MachineFault& result){rejected=(result.syndrome&63)==status&&result.address==address;}
  check(rejected,message);
}
MinimalGuest initial(){MinimalGuest result(128*1024*1024);result.load_elf(system_demo_elf(),true);return result;}
void boot_and_restore(){
  auto whole=initial();const auto all=whole.run(10000);
  check(all.halted&&all.exit_code==0,"Bare-metal system did not shut down through PSCI");
  check(all.output=="REVERSE: EL1 MMU IRQ EL0 SVC FAULT OK\n","System witness did not verify its own architecture");
  check(whole.machine().exceptions==4&&whole.registers().x[23]==1&&whole.registers().x[24]==1,"IRQ/SVC/permission-abort transitions failed");
  check(whole.read(0x40009000,8)==0&&whole.read(0x40009008,8)==42,"Denied store changed RAM or successful store was lost");
  check(whole.runtime().memory().stats().overrides==9,"System witness materialized redundant RAM state");
  check(whole.snapshot()[3]=='2',"System mode did not use versioned machine snapshot");
  auto sliced=initial();std::string output;
  for(unsigned i=0;i<200&&!sliced.registers().halted;++i){const auto run=sliced.run(7);output+=run.output;sliced=MinimalGuest::from_snapshot(sliced.snapshot());}
  check(output==all.output&&sliced.snapshot()==whole.snapshot(),"System checkpoint changed interrupts, timer time, PC, output or memory");
  auto reset=initial();const auto pristine=reset.snapshot();auto broken=system_demo_elf();broken[18]=62;
  fails([&]{reset.load_elf(broken,true);},"Wrong ISA system ELF accepted");check(reset.snapshot()==pristine,"Invalid system ELF replaced active state");
  auto corrupt=pristine;corrupt.back()^=1;fails([&]{MinimalGuest::from_snapshot(corrupt);},"Corrupt machine checkpoint accepted");
}
void observe_userspace_without_execution(){
  auto stopped=initial();const auto event=stopped.run(10000,true);
  check(event.executed>0&&event.executed<10000&&!event.halted&&stopped.machine().el==0,
        "Host EL0 observation did not stop at an actual architectural entry");
  const auto snapshot=stopped.snapshot();auto counted=initial();
  counted.run(stopped.registers().instructions);
  check(counted.snapshot()==snapshot,"EL0 observation changed the real CPU transition");
  const auto again=stopped.run(10000,true);
  check(again.executed==0&&again.output.empty()&&stopped.snapshot()==snapshot,
        "EL0 observer executed or changed an already stopped userspace");
  auto resumed=MinimalGuest::from_snapshot(snapshot);const auto tail=resumed.run(10000);
  auto continuous=initial();const auto all=continuous.run(10000);
  check(event.output+tail.output==all.output&&resumed.snapshot()==continuous.snapshot(),
        "Observed/resumed userspace differs from continuous machine execution");
}
void inactive_debug_slot(){
  for(unsigned op2=4;op2<=7;++op2){
    const auto write=0xd5100002u|(op2<<5); // MSR slot0,X2
    const auto read=0xd5300003u|(op2<<5); // MRS X3,slot0
    auto guest=initial();const auto pc=guest.registers().pc;
    guest.write(pc,write,4);guest.write(pc+4,read,4);
    const auto clear=guest.run(2);
    check(clear.executed==2&&guest.machine().exceptions==0&&guest.registers().x[3]==0,
          "Inactive breakpoint/watchpoint slot did not clear/read as zero");
    check(MinimalGuest::from_snapshot(guest.snapshot()).snapshot()==guest.snapshot(),
          "Inactive debug state changed checkpoint restoration");
    auto nonzero=initial();const auto start=nonzero.registers().pc;
    nonzero.write(start,0xd2800022,4);nonzero.write(start+4,write,4);nonzero.run(1);
    const auto before=nonzero.snapshot();
    fails([&]{nonzero.run(1);},"Nonzero unsupported hardware debug programming accepted");
    check(nonzero.snapshot()==before,"Rejected debug programming changed the machine");
    auto unknown=initial();unknown.write(unknown.registers().pc,write|(1u<<8),4);
    fails([&]{unknown.run(1);},"Unimplemented additional debug slot accepted");
    auto user=initial();user.run(10000,true);const auto exceptions=user.machine().exceptions;
    user.write(user.physical_address(user.registers().pc,false,true),write,4);user.run(1);
    check(user.machine().el==1&&user.machine().exceptions==exceptions+1&&
          ((user.machine().esr>>26)&63)==0,
          "EL0 debug write bypassed the architectural undefined-instruction trap");
  }
}
void translation(){
  auto guest=initial();for(unsigned i=0;i<60&&!(guest.machine().sctlr&1);++i)guest.run(1);
  check(guest.machine().sctlr&1,"Witness never enabled the MMU");
  check(guest.physical_address(0x40080000,false,true)==0x40080000,"TTBR0 identity mapping failed");
  check(guest.physical_address(UINT64_C(0xffffff8040080000),false,true)==0x40080000,"TTBR1 upper canonical mapping failed");
  fault(guest,UINT64_C(0x0100000040080000),false,false,0,"Noncanonical virtual address accepted");
  fault(guest,0x80009000,true,false,13,"Read-only block allowed a write");
  guest.write(0x40010008,0x40000345,8);fault(guest,0x40080000,false,false,9,"Missing access flag accepted");
  guest.write(0x40010008,0x40000745,8);fault(guest,0x40080000,false,true,13,"EL1 executed a region writable by EL0");
  guest.write(0x40010008,UINT64_C(0x0020000040000705),8);fault(guest,0x40080000,false,true,13,"Privileged execute-never ignored");
  guest.write(0x40010008,0x40011003,8);guest.write(0x40011000,0x40000745,8);
  check(guest.physical_address(0x40001234)==0x40001234,"Level-2 table/block walk failed");
  guest.write(0x40011000,0x40012003,8);guest.write(0x40012008,0x40001747,8);
  check(guest.physical_address(0x40001234)==0x40001234,"Level-3 4 KiB page walk failed");
  fault(guest,0x40002234,false,false,7,"Unmapped level-3 page accepted");
  guest.write(0x40010008,UINT64_C(0x4000000040011003),8); // APTable read-only restriction.
  fault(guest,0x40001234,true,false,15,"Table permission restriction ignored");
  auto user=initial();for(unsigned i=0;i<150&&user.machine().el;++i)user.run(1);
  check(user.machine().el==0,"ERET did not enter EL0");
  fault(user,MachineState::uart_base,false,false,13,"EL0 accessed privileged device mapping");
  check(user.physical_address(0x80080000,false,true)==0x40080000,"EL0 read-only code alias denied");
}
void store_invalidates_its_own_translation(){
  auto g=initial();for(unsigned i=0;i<60&&!(g.machine().sctlr&1);++i)g.run(1);
  g.write(0x40010010,0x40000705,8);
  const auto pc=g.registers().pc;
  const std::uint32_t code[]{0xd2800216,0xf2b00036,0xd280e020,0xf90002c0};
  for(unsigned i=0;i<4;++i)g.write(pc+4*i,code[i],4);
  check(g.physical_address(0x80010010,true)==0x40010010,"Writable table alias is incorrect");
  g.run(4);
  check(g.machine().exceptions==0&&g.read(0x40010010,8)==0x701,
        "Single store did not update its own page-table descriptor");
  check(g.physical_address(0x80010010,true)==0x10010,
        "Store retained a stale write translation installed by its own validation");
}
void page_table_store_dependencies(){
  for(const bool tail:{false,true}){
    auto g=initial();for(unsigned i=0;i<60&&!(g.machine().sctlr&1);++i)g.run(1);
    g.write(0x40010018,0x40011003,8);
    g.write(0x40011000,0x40012003,8);
    g.write(0x40012000,0x40030703,8);
    g.write(0x40012008,0x40012703,8);
    g.write(0x40013000,0x40050703,8);
    g.write(0x40030000,0x1111222233334444,8);
    g.write(0x40050000,0x5555666677778888,8);
    g.write(0x400a0000,0xf9000020,4); // STR X0,[X1]
    g.write(0x400a0004,0xf9400062,4); // LDR X2,[X3]
    auto saved=g.snapshot();
    auto patch=[&](std::size_t at,std::uint64_t value){
      for(unsigned i=0;i<8;++i)saved[at+i]=std::uint8_t(value>>(i*8));
    };
    patch(24,0x400a0000);
    patch(72,tail?UINT64_C(0x4005070312345678):UINT64_C(0x40013003));
    patch(80,tail?UINT64_C(0xc0000ffc):UINT64_C(0x40011000));
    patch(96,0xc0000000);
    patch(saved.size()-8,digest(saved,saved.size()-8));
    g=MinimalGuest::from_snapshot(saved);
    check(g.physical_address(0xc0000000)==0x40030000,"Dependency witness initial mapping is incorrect");
    g.run(2);
    check(g.machine().exceptions==0&&g.registers().x[2]==UINT64_C(0x5555666677778888),
          "Ancestor or store-tail PTE update retained stale address hints");
    check(g.physical_address(0xc0000000)==0x40050000,
          "Page-table dependency update did not affect the next translation");
    if(tail)check(g.read(0x40030ffc,4)==0x12345678&&g.read(0x40012000,4)==0x40050703,
                  "Cross-page table write changed a different physical extent");
    auto restored=MinimalGuest::from_snapshot(g.snapshot());
    check(restored.snapshot()==g.snapshot()&&restored.physical_address(0xc0000000)==0x40050000,
          "Nonpersistent table footprint changed restoration");
  }
}
void quantum_alias_invalidates_translation(){
  auto g=initial();for(unsigned i=0;i<60&&!(g.machine().sctlr&1);++i)g.run(1);
  g.write(0x40010018,0x40011003,8);g.write(0x40011000,0x40012003,8);
  g.write(0x40012000,0x40030703,8);
  g.write(0x40030000,0x1111222233334444,8);g.write(0x40050000,0x5555666677778888,8);
  g.write(0x400a0000,0x39000020,4); // STRB W0,[X1], into the other quantum view.
  g.write(0x400a0004,0xf9400062,4); // LDR X2,[X3], using the changed PTE.
  auto saved=g.snapshot();
  auto patch=[&](std::size_t at,std::uint64_t value){for(unsigned i=0;i<8;++i)saved[at+i]=std::uint8_t(value>>(8*i));};
  patch(24,0x400a0000);patch(72,5);patch(80,0x40060000);patch(96,0xc0000000);
  patch(saved.size()-8,digest(saved,saved.size()-8));g=MinimalGuest::from_snapshot(saved);
  const auto pte=std::to_string(UINT64_C(0x40012002)),other=std::to_string(UINT64_C(0x40060000));
  g.apply_xi_delta("mem_bind "+pte+" 20000\nmem_bind "+other+" 20000\nmem_store "+pte+" 3\n");
  check(g.physical_address(0xc0000000)==0x40030000,"Quantum PTE alias initial mapping is incorrect");
  g.run(2);
  check(g.machine().exceptions==0&&g.registers().x[2]==UINT64_C(0x5555666677778888),
        "Store through a remote quantum alias retained a stale PTE hint");
  check(g.read8(0x40012002)==5&&g.read8(0x40060000)==5&&g.runtime().memory().stats().quantum_bindings==2,
        "Quantum PTE alias stopped sharing the original runtime authority");
  check(MinimalGuest::from_snapshot(g.snapshot()).snapshot()==g.snapshot(),
        "Quantum-linked page-table state did not restore exactly");
}
void shared_quantum_authority(){
  auto guest=initial();guest.apply_xi_delta("#!reverse-pc-machine-v1\nset r0 0\nlabel loop\nh $r0\nadd r0 1\ncmp r0 4096\njlt loop\nhalt\n");
  check(guest.runtime().core().stats().active_qubits==4096,"Existing XI 4096-qubit capability was not preserved");
  const auto core=guest.runtime().core_snapshot();auto run=guest.run(10000);
  check(run.halted&&guest.runtime().core_snapshot()==core,"Classical ARM64 execution altered the existing quantum state");
  guest.apply_xi_delta("mem_read 1073778696\n");
  check(guest.read8(0x40009008)==42,"XI and ARM64 did not share the same InverseMemory authority");
  check(MinimalGuest::from_snapshot(guest.snapshot()).runtime().core_snapshot()==core,"System checkpoint lost the original quantum authority");
}
void exclusives(){
  const std::uint32_t code[]{0xd2880001,0xf2a80001,0xd2800520,0xf9000020,0xc85f7c20,0x91000400,0xc8027c20,
    0xc85f7c23,0xd5033f5f,0xc8047c23,0xc8dffc25,0xd2800b06,0xc89ffc26,0xc8dffc27,0xd2800100,0xf2b08000,0xd4000002};
  auto image=system_demo_elf();image.resize(256);
  for(auto instruction:code)put(image,instruction,4);
  for(unsigned i=0;i<8;++i){image[96+i]=std::uint8_t((sizeof(code))>>(8*i));image[104+i]=image[96+i];}
  MinimalGuest guest(128*1024*1024);guest.load_elf(image,true);guest.run(5);
  check(guest.machine().exclusive_size==8,"LDXR did not establish an exclusive monitor");
  auto restored=MinimalGuest::from_snapshot(guest.snapshot());check(restored.snapshot()==guest.snapshot(),"Exclusive monitor was lost in checkpoint");
  restored.run(100);check(restored.registers().x[2]==0&&restored.registers().x[4]==1,"STXR success/CLREX failure status is incorrect");
  check(restored.registers().x[5]==42&&restored.registers().x[7]==88&&restored.read(0x40004000,8)==88,"Exclusive/acquire/release data values differ");
  guest.apply_xi_delta("mem_store 1073758208 99\n");guest.run(100);
  check(guest.registers().x[2]==1&&guest.registers().x[5]==99,"XI write did not invalidate the same exclusive monitor");
}
void exclusive_pairs(){
  const auto fixture=[](bool wide,bool acquire,bool release,bool clear,std::uint64_t address=0x40004000){
    const unsigned size=wide?8:4;
    const std::uint32_t load=(wide?0xc87f06c0u:0x887f06c0u)|(acquire?0x8000u:0u);
    const std::uint32_t store=(wide?0xc82a12c9u:0x882a12c9u)|(release?0x8000u:0u);
    std::vector<std::uint32_t> code{
      0xd2800016u|std::uint32_t((address&65535)<<5),
      0xf2a00016u|std::uint32_t(((address>>16)&65535)<<5),
      0xd2800229u,0xd2800444u,load // X9=17, X4=34
    };
    if(clear)code.push_back(0xd5033f5fu);
    code.push_back(store);code.push_back(0x2a0a03ebu);code.push_back(store);
    auto image=system_demo_elf();image.resize(256);
    for(auto op:code)put(image,op,4);
    for(unsigned i=0;i<8;++i){
      image[96+i]=std::uint8_t(std::uint64_t(code.size()*4)>>(i*8));image[104+i]=image[96+i];
    }
    MinimalGuest g(128*1024*1024);g.load_elf(image,true);
    g.write(0x40004000,wide?UINT64_C(0x1122334455667788):UINT64_C(0x87654321),size);
    g.write(0x40004000+size,wide?UINT64_C(0x99aabbccddeeff00):UINT64_C(0xfedcba98),size);
    return g;
  };
  for(bool wide:{false,true})for(bool acquire:{false,true})for(bool release:{false,true}){
    const unsigned size=wide?8:4;auto g=fixture(wide,acquire,release,false);g.run(5);
    check(g.registers().x[0]==(wide?UINT64_C(0x1122334455667788):UINT64_C(0x87654321))&&
          g.registers().x[1]==(wide?UINT64_C(0x99aabbccddeeff00):UINT64_C(0xfedcba98)),
          "Exclusive pair load order or 32-bit zero extension failed");
    check(g.machine().exclusive_address==0x40004000&&g.machine().exclusive_size==2*size,
          "Exclusive pair did not reserve the exact physical extent");
    check(g.machine().save()[0]==(wide?4:2),"Exclusive monitor checkpoint version is wrong");
    const auto before=g.snapshot();auto resumed=MinimalGuest::from_snapshot(before);
    check(resumed.snapshot()==before,"Live pair monitor was lost during RVG restore");
    auto direct=g;direct.run(1);resumed.run(1);
    check(direct.snapshot()==resumed.snapshot(),"Resumed exclusive pair differs from direct execution");
    check(direct.registers().x[10]==0&&direct.read(0x40004000,size)==17&&
          direct.read(0x40004000+size,size)==34,"Exclusive pair successful store was not complete");
    check(direct.machine().exclusive_size==0&&direct.machine().save()[0]==2,
          "Completed exclusive pair retained a monitor or changed legacy state");
    direct.run(2);
    check(direct.registers().x[11]==0&&direct.registers().x[10]==1&&
          direct.read(0x40004000,size)==17&&direct.read(0x40004000+size,size)==34,
          "A second store without a new pair reservation succeeded");
    check(g.snapshot()==before,"Pair execution mutated its source snapshot");
  }
  auto clear=fixture(true,false,true,true);clear.run(5);clear.run(1);
  check(clear.machine().exclusive_size==0,"CLREX failed to clear a 16-byte reservation");
  clear.run(1);check(clear.registers().x[10]==1&&clear.read(0x40004000,8)==UINT64_C(0x1122334455667788),
    "CLREX did not prevent a paired store");
  auto external=fixture(true,false,true,false);external.run(5);
  external.apply_xi_delta("mem_store 1073758208 99\n");
  const auto changed=external.read(0x40004000,8);external.run(1);
  check(external.registers().x[10]==1&&external.read(0x40004000,8)==changed&&
        external.read(0x40004008,8)==UINT64_C(0x99aabbccddeeff00),
        "An XI write did not invalidate the same paired exclusive monitor");
  auto unaligned=fixture(true,false,false,false,0x40004008);unaligned.run(4);
  const auto memory=unaligned.runtime().memory_persistent_snapshot();
  unaligned.run(1);
  check(unaligned.machine().exceptions==1&&(unaligned.machine().esr&63)==0x21&&
        unaligned.machine().far==0x40004008&&unaligned.machine().exclusive_size==0&&
        unaligned.runtime().memory_persistent_snapshot()==memory,
        "Unaligned pair failed to take an atomic alignment fault");
  auto live=fixture(true,false,false,false);live.run(5);auto forged=live.machine().save();
  forged[0]=2;fails([&]{MachineState::restore(forged);},"Legacy machine version accepted a 16-byte reservation");
}
void paired_store_pins_physical_translation(){
  auto g=initial();for(unsigned i=0;i<60&&!(g.machine().sctlr&1);++i)g.run(1);
  check(g.machine().sctlr&1,"Pair page-table witness did not enable the MMU");
  // Writable alias VA 0x80000000 -> PA 0x40000000. The pair overwrites
  // the very table entry that translates its own first destination.
  g.write(0x40010010,0x40000705,8);
  const auto pc=g.registers().pc;
  const std::uint32_t code[]{
    0xd2800216u,0xf2b00036u, // X22=0x80010010
    0xd280e029u,0xd2800444u, // X9=0x701, X4=34
    0xc87f06c0u,0xc82a92c9u,0xd503201fu
  };
  for(unsigned i=0;i<sizeof(code)/sizeof(code[0]);++i)
    g.write(g.physical_address(pc+4*i,false,true),code[i],4);
  check(g.physical_address(0x80010010)==0x40010010,
        "Pair self-remapping alias was not initially valid");
  g.run(6);
  check(g.machine().exceptions==0&&g.registers().x[10]==0&&
        g.read(0x40010010,8)==0x701&&g.read(0x40010018,8)==34,
        "Paired store retranslated or partially published its second half");
  check(g.physical_address(0x80010010)==0x10010,
        "A paired page-table write left a stale translation hint");
}
void lazy_ram_range_error_parity(){
  // Same exact permitted range, fault type and diagnostic text. No change to
  // the single QuantumRuntime/InverseMemory byte authority.
  MinimalGuest guest(128*1024*1024);
  guest.load_elf(system_demo_elf(),true);
  const auto initial=guest.snapshot();
  // The high valid byte is addressable without constructing error text.
  check(guest.read8(MachineState::ram_base+128*1024*1024-1)==0,
        "Last legal guest RAM byte was rejected");
  check(guest.snapshot()==initial,
        "Read-only valid RAM access changed the original XI/Ether checkpoint");
  bool caught=false;
  try{(void)guest.read8(MachineState::ram_base+128*1024*1024);}
  catch(const std::runtime_error& e){
    caught=true;
    check(std::string(e.what())=="Guest address outside logical RAM: 0x48000000",
          "Lazy RAM fault did not retain the original exact text");
  }
  check(caught,"Out-of-bounds guest read was accepted");
  caught=false;
  try{(void)guest.read8(MachineState::ram_base-1);}
  catch(const std::runtime_error& e){
    caught=true;
    check(std::string(e.what())=="Guest address outside logical RAM: 0x3fffffff",
          "Lazy RAM underflow fault did not retain the exact text");
  }
  check(caught,"Guest RAM address underflow was accepted");
  fails([&]{(void)guest.read(MachineState::ram_base+128*1024*1024-1,2);},
        "Cross-boundary RAM read was accepted");
  check(guest.snapshot()==initial,
        "Rejected RAM reads modified the authoritative XI/Ether checkpoint");
}
void image_boot_protocol(){
  Bytes kernel(64);auto patch=[&](std::size_t at,std::uint64_t value,unsigned size){for(unsigned i=0;i<size;++i)kernel[at+i]=std::uint8_t(value>>(i*8));};
  patch(0,0x14000010,4);patch(8,0x80000,8);patch(16,128,8);patch(24,2,8);patch(56,0x644d5241,4);
  put(kernel,0xd2800100,4);put(kernel,0xf2b08000,4);put(kernel,0xd4000002,4); // PSCI SYSTEM_OFF
  MinimalGuest guest(128*1024*1024);guest.load_linux_image(kernel,Bytes{1,2,3,4});
  check(guest.read(guest.registers().x[0],4)==0xedfe0dd0,"Boot x0 does not point at a valid big-endian FDT magic");
  check(guest.read(0x44000000,4)==0x04030201,"Initrd backing address is wrong");
  check(guest.runtime().memory().stats().overrides==0,"Image boot materialized physical RAM");
  auto restored=MinimalGuest::from_snapshot(guest.snapshot());check(restored.snapshot()==guest.snapshot(),"Image/DTB/initrd checkpoint is not exact");
  check(restored.run(10).halted,"Image-format protocol fixture did not execute at its physical entry");
  const auto before=guest.snapshot();kernel[56]^=1;fails([&]{guest.load_linux_image(kernel);},"Invalid Linux Image accepted");
  check(guest.snapshot()==before,"Invalid Image replaced current machine");
  const auto original=guest.machine().receive;guest.console_input("hello\n");
  check(MinimalGuest::from_snapshot(guest.snapshot()).machine().receive=="hello\n","Serial input queue was not persistent");
  const auto queued=guest.snapshot();fails([&]{guest.console_input(std::string(4096,'x'));},"Serial queue budget ignored");check(guest.snapshot()==queued,"Rejected serial input changed machine");
  MachineState machine;machine.enabled=true;machine.image_kind=1;machine.mdscr=0x1000;
  check(MachineState::restore(machine.save()).mdscr==0x1000,"MDSCR trap-control state did not persist");
  machine.mdscr=0;auto legacy=machine.save();legacy[0]=1;legacy.erase(legacy.end()-16,legacy.end()-8);
  check(MachineState::restore(legacy).save()==machine.save(),"Previous RVG2 machine-state v1 no longer restores");
}
// Header-sized fixture using the *measured* 35,777,024-byte upstream GKI
// length. Unlike the independent CI experiment, this is NOT an Android boot.
void android_bootargs_snapshot_profile(){
  // One original XI/Ether CPU; opt-in Android bootargs are data in RVI2,
  // never a new CPU, a fake /init, or a change to legacy Linux RVG states.
  Bytes kernel(64);
  const auto write=[&](std::size_t at,std::uint64_t value,unsigned n){
    for(unsigned i=0;i<n;++i)kernel.at(at+i)=std::uint8_t(value>>(8*i));
  };
  write(0,0xd503201f,4); // real AArch64 NOP
  write(8,0,8);write(16,128,8);write(24,0,8);
  write(56,0x644d5241,4);
  const Bytes initrd{1,2,3,4,5};
  const auto& args=guest_android_cuttlefish_bootargs();
  check(args.find("init=/init")!=std::string::npos &&
        args.find("bootconfig")!=std::string::npos &&
        args.find("binder.impl=rust")!=std::string::npos &&
        args.find("console=ttyAMA0")!=std::string::npos,
        "Opt-in Android bootargs do not preserve official vendor and REVERSE board values");

  MinimalGuest legacy(128*1024*1024);
  legacy.load_linux_image(kernel,initrd);
  const auto legacy_state=legacy.snapshot();
  // RVG2 image begins after 8+43*8 bytes. Legacy must remain RVI1.
  check(legacy_state[8+43*8+3]=='1',
        "Default Linux startup changed away from historical RVI1");
  check(MinimalGuest::from_snapshot(legacy_state).snapshot()==legacy_state,
        "Legacy Linux RVI1 checkpoint was modified by Android profile support");

  MinimalGuest guest(128*1024*1024);
  guest.load_linux_image(kernel,initrd,args);
  const auto profile_state=guest.snapshot();
  check(args.find("androidboot.force_normal_boot=")==std::string::npos&&args.find("initcall_blacklist=")==std::string::npos,
        "Historical Android profile was silently changed to a new boot configuration");
  for(const auto& normal:{guest_android_cuttlefish_normal_bootargs(),guest_android_cuttlefish_normal_no_scx_bootargs()}){
    MinimalGuest target(128*1024*1024);target.load_linux_image(kernel,initrd,normal);
    const auto saved=target.snapshot();
    check(saved!=profile_state&&MinimalGuest::from_snapshot(saved).snapshot()==saved&&
          normal.find("androidboot.force_normal_boot=1")!=std::string::npos&&
          normal.find("androidboot.fstab_suffix=cf.ext4.cts")!=std::string::npos&&
          normal.find("androidboot.slot_suffix=_a")!=std::string::npos,
          "Explicit normal-boot RVI2 profile did not persist its actual Android inputs");
  }
  check(profile_state[8+43*8+3]=='2' &&
        guest.image_bytes()==40+kernel.size()+initrd.size()+
        guest_device_tree(128*1024*1024,0x44000000,initrd.size(),args).size()+
        args.size(),"Android boot profile is not backed by bounded immutable RVI2");
  const auto expected=guest_device_tree(128*1024*1024,0x44000000,initrd.size(),args);
  const auto addr=guest.registers().x[0];
  for(std::size_t i=0;i<expected.size();++i)
    check(guest.read8(addr+i)==expected[i],
          "Real guest physical FDT differs from RVI2 bootargs device tree");
  const auto contains=[&](const std::string& text){
    return std::search(expected.begin(),expected.end(),text.begin(),text.end())
      !=expected.end();
  };
  check(contains("bootconfig") && contains("init=/init") &&
        contains("stdout-path") && !contains("virtio,block"),
        "FDT does not contain the authentic Android startup parameters");
  check(guest.runtime().memory().stats().overrides==0,
        "Boot profile unexpectedly materialized guest RAM deltas");

  guest.enable_adaptive_memory();
  const auto adaptive=guest.snapshot();
  const auto restored=MinimalGuest::from_snapshot(adaptive);
  check(restored.snapshot()==adaptive,
        "RVI2 adaptive RVG checkpoint failed byte-identical roundtrip");
  auto one=guest;one.run(1);
  auto sliced=MinimalGuest::from_snapshot(adaptive);sliced.run(1);
  check(one.snapshot()==sliced.snapshot() &&
        one.registers().pc==MachineState::ram_base+4,
        "RVI2 guest instruction replay diverged after checkpoint restoration");

  const auto stable=guest.snapshot();
  fails([&]{guest.load_linux_image(kernel,initrd,"init=/init\nbootconfig");},
        "Injected newline accepted in kernel command line");
  check(guest.snapshot()==stable,
        "Invalid Android boot profile changed live XI/Ether state");
  fails([&]{guest.load_linux_image(kernel,initrd,std::string(1025,'x'));},
        "Kernel bootargs accepted beyond 1024 byte budget");
  check(guest.snapshot()==stable,
        "Oversized bootargs mutated live XI/Ether guest");

  // A recomputed RVG FNV alone is NOT enough: the RVI2 tree must still match
  // the immutable bootargs payload. Reject a forged trailing profile.
  auto forged=adaptive;
  const std::string original="init=/init";
  const auto found=std::search(forged.begin(),forged.end(),
                               original.begin(),original.end());
  check(found!=forged.end(),"Could not locate saved RVI2 Android boot profile");
  *std::next(found,original.size()-1)='x';
  const std::uint64_t checksum=digest(forged,forged.size()-8);
  for(unsigned i=0;i<8;++i)forged[forged.size()-8+i]=std::uint8_t(checksum>>(i*8));
  fails([&]{MinimalGuest::from_snapshot(forged);},
        "Tampered RVI2 bootargs with recomputed checksum silently accepted");
}
void large_linux_gki_loader_budget(){
  constexpr std::size_t authentic_kernel_bytes=35777024;
  Bytes kernel(authentic_kernel_bytes,0);
  const auto patch=[&](std::size_t at,std::uint64_t value,unsigned count){
    for(unsigned i=0;i<count;++i)kernel.at(at+i)=std::uint8_t(value>>(8*i));
  };
  patch(0,0xd503201f,4); // one actual AArch64 NOP, not a fabricated /init
  patch(8,0,8);patch(16,36503552,8);patch(24,0xa,8);
  patch(56,0x644d5241,4);
  kernel.back()=0xa5;
  MinimalGuest guest(128*1024*1024);
  guest.load_linux_image(kernel);
  check(guest.image_bytes()>MinimalGuest::max_image,"Real-size GKI fixture still blocked by legacy ELF quota");
  check(guest.image_bytes()<MinimalGuest::max_snapshot,"GKI fixture unexpectedly exceeds legacy RVG transport");
  check(guest.registers().pc==MachineState::ram_base,"GKI text_offset=0 was not honored");
  check(guest.read8(MachineState::ram_base+authentic_kernel_bytes-1)==0xa5,
        "Large immutable Image backing lost its last physical byte");
  check(guest.runtime().memory().stats().overrides==0,
        "Large Linux Image materialized RAM deltas without any stores");
  const auto before=guest.snapshot();
  auto restored=MinimalGuest::from_snapshot(before);
  check(restored.snapshot()==before,"Large Linux RVI1 RVG2 checkpoint did not round-trip byte-identically");
  check(restored.read8(MachineState::ram_base+authentic_kernel_bytes-1)==0xa5,
        "Large Linux restored backing is not byte-exact");
  const auto result=restored.run(1);
  check(result.executed==1&&restored.registers().pc==MachineState::ram_base+4,
        "Large Linux immutable backing did not execute one actual NOP");
  check(restored.runtime().memory().stats().overrides==0,
        "Executing immutable GKI-sized backing unexpectedly changed XI memory");
  const auto stable=guest.snapshot();
  auto invalid=kernel;invalid[56]=0;
  fails([&]{guest.load_linux_image(invalid);},"Malformed large Image was accepted");
  check(guest.snapshot()==stable,"Rejected large Image modified old guest checkpoint");
  invalid.clear();
  auto oversized=system_demo_elf();oversized.resize(MinimalGuest::max_image+1);
  fails([&]{guest.load_elf(oversized,true);},"Legacy ELF 16 MiB image cap was loosened");
  check(guest.snapshot()==stable,"Rejected ELF mutated live GKI state");
  MinimalGuest roomy(256*1024*1024);
  Bytes kernel_over_limit(MinimalGuest::max_linux_kernel+1,0);
  fails([&]{roomy.load_linux_image(kernel_over_limit);},
        "Linux kernel larger than host safety budget was accepted");
}
void cross_page_access(){
  auto guest=initial();for(unsigned i=0;i<60&&!(guest.machine().sctlr&1);++i)guest.run(1);
  guest.write(0x40010018,0x40011003,8);guest.write(0x40011000,0x40012003,8);
  guest.write(0x40012000,0x40030703,8);guest.write(0x40012008,0x40050703,8);
  guest.write(0x400a0000,0xf9000020,4);guest.write(0x400a0004,0xf9400022,4); // STR X0,[X1]; LDR X2,[X1]
  auto enter=[](MinimalGuest& target,std::uint64_t address,std::uint64_t value){
    auto state=target.snapshot();auto patch=[&](std::size_t at,std::uint64_t n){for(unsigned i=0;i<8;++i)state[at+i]=std::uint8_t(n>>(i*8));};
    patch(24,0x400a0000);patch(72,value);patch(80,address);patch(state.size()-8,digest(state,state.size()-8));target=MinimalGuest::from_snapshot(state);
  };
  enter(guest,0xc0000ffc,UINT64_C(0x123456789abcdef0));guest.run(2);
  check(guest.read(0x40030ffc,4)==0x9abcdef0&&guest.read(0x40050000,4)==0x12345678,"Cross-page store assumed physically adjacent pages");
  check(guest.registers().x[2]==UINT64_C(0x123456789abcdef0),"Cross-page load joined the wrong physical pages");
  guest.write(0x400a0000,0x69400820,4);enter(guest,0xc0000ffc,0);guest.run(1); // LDPSW X0,X2,[X1]
  check(guest.registers().x[0]==UINT64_C(0xffffffff9abcdef0)&&guest.registers().x[2]==0x12345678,"LDPSW did not sign-extend separate mapped pages");
  guest.write(0x400a0000,0xf9000020,4);
  guest.write(0x40012008,0x40050783,8);enter(guest,0xc0000ffc,UINT64_MAX);guest.run(1);
  check(guest.machine().exceptions==1&&guest.machine().far==0xc0001000&&(guest.machine().esr&63)==15,"Cross-page permission fault has incorrect address/status");
  check(guest.read(0x40030ffc,4)==0x9abcdef0&&guest.read(0x40050000,4)==0x12345678,"Faulting cross-page store changed an earlier byte");
  // Changing the page table must invalidate every retained translation hint
  // before the very next access observes permissions or aliases.
  guest.write(0x40012008,0x40060703,8);enter(guest,0xc0000ffc,UINT64_C(0x0badcafe00000000));guest.run(2);
  check(guest.read(0x40060000,4)==0x0badcafe&&guest.read(0x40050000,4)==0x12345678,"Access-local mapping reuse became a stale persistent TLB");
}

void unprivileged_memory_access(){
  auto guest=initial();
  for(unsigned i=0;i<60 && !(guest.machine().sctlr&1);++i)guest.run(1);
  check((guest.machine().sctlr&1)!=0&&guest.machine().el==1,
        "Unprivileged test did not start at EL1 with MMU");
  // Map 0xC0000000 and the next 4 KiB to distinct physical pages.
  guest.write(0x40010018,0x40011003,8);
  guest.write(0x40011000,0x40012003,8);
  guest.write(0x40012000,0x40030703,8); // First page EL1 only.
  guest.write(0x40012008,0x40050703,8); // Second page EL1 only.
  // Exact independently assembled AArch64 STTR/LDTR opcodes.
  constexpr std::uint32_t sttr=0xf8000820,ldtr=0xf8400822;
  constexpr std::uint32_t privileged_str=0xf9000020;
  guest.write(0x400a0000,sttr,4);
  auto enter=[](MinimalGuest& target,std::uint64_t address,std::uint64_t value){
    auto state=target.snapshot();
    auto patch=[&](std::size_t at,std::uint64_t n){
      for(unsigned i=0;i<8;++i)state[at+i]=std::uint8_t(n>>(i*8));
    };
    patch(24,0x400a0000);patch(72,value);patch(80,address);
    patch(state.size()-8,digest(state,state.size()-8));
    target=MinimalGuest::from_snapshot(state);
  };
  const auto initial_state=guest.snapshot();
  constexpr std::uint64_t payload=UINT64_C(0x123456789abcdef0);
  enter(guest,0xc0000ffc,payload);
  const auto core=guest.runtime().core_snapshot();
  guest.run(1);
  check(guest.machine().exceptions==1&&guest.machine().far==0xc0000ffc &&
        (guest.machine().esr&63)==15 && ((guest.machine().esr>>26)&63)==0x25,
        "STTR failed to enforce user permission with an EL1 data abort");
  check(guest.read(0x40030ffc,4)==0 && guest.read(0x40050000,4)==0,
        "Faulting unprivileged store partially modified protected memory");
  check(guest.runtime().core_snapshot()==core,
        "Unprivileged permission fault changed quantum state");

  auto normal=MinimalGuest::from_snapshot(initial_state);
  normal.write(0x400a0000,privileged_str,4);
  enter(normal,0xc0000ffc,payload);normal.run(1);
  check(normal.machine().exceptions==0 &&
        normal.read(0x40030ffc,4)==0x9abcdef0 &&
        normal.read(0x40050000,4)==0x12345678,
        "Ordinary EL1 store was unexpectedly restricted to EL0 permissions");

  auto partial=MinimalGuest::from_snapshot(initial_state);
  partial.write(0x40012000,0x40030743,8); // EL0 may write first page only.
  enter(partial,0xc0000ffc,payload);partial.run(1);
  check(partial.machine().exceptions==1&&partial.machine().far==0xc0001000 &&
        (partial.machine().esr&63)==15 &&
        partial.read(0x40030ffc,4)==0 && partial.read(0x40050000,4)==0,
        "Cross-page STTR did not roll back after EL0 permission fault");

  auto allowed=MinimalGuest::from_snapshot(initial_state);
  allowed.write(0x40012000,0x40030743,8);
  allowed.write(0x40012008,0x40050743,8);
  enter(allowed,0xc0000ffc,payload);
  const auto before_store=allowed.snapshot();
  allowed.run(1);
  check(allowed.machine().exceptions==0&&allowed.machine().el==1 &&
        allowed.registers().x[1]==0xc0000ffc &&
        allowed.read(0x40030ffc,4)==0x9abcdef0 &&
        allowed.read(0x40050000,4)==0x12345678,
        "STTR did not write user pages or changed the base register");
  auto resume=MinimalGuest::from_snapshot(allowed.snapshot());
  check(resume.snapshot()==allowed.snapshot(),
        "STTR snapshot differs after restoration");
  resume.write(0x400a0000,ldtr,4);
  enter(resume,0xc0000ffc,0);
  resume.run(1);
  check(resume.registers().x[2]==payload&&resume.machine().exceptions==0,
        "LDTR failed to load from user pages");
  auto denied=MinimalGuest::from_snapshot(initial_state);
  denied.write(0x400a0000,ldtr,4);
  enter(denied,0xc0000ffc,0);denied.run(1);
  check(denied.machine().exceptions==1&&denied.machine().far==0xc0000ffc,
        "LDTR ignored user-read access permissions");
  check(MinimalGuest::from_snapshot(before_store).snapshot()==before_store,
        "User-access regression changed legacy RVG checkpoint encoding");
}

void eret_ssbs_saved_status(){
  // Independently assembled AArch64 sequence:
  // MOV X0,#0x1000; MSR SPSR_EL1,X0; load ELR_EL1=0x40080018;
  // ERET; NOP at the EL0 target. The bit 12 SPSR marker is SSBS.
  const std::uint32_t code[]{
    0xd2820000u,0xd5184000u,0xd2800301u,0xf2a80101u,
    0xd5184021u,0xd69f03e0u,0xd503201fu
  };
  auto image=system_demo_elf();image.resize(256);
  for(const auto instruction:code)put(image,instruction,4);
  for(unsigned i=0;i<8;++i){
    image[96+i]=std::uint8_t(std::uint64_t(sizeof(code))>>(8*i));
    image[104+i]=image[96+i];
  }
  MinimalGuest witness(128*1024*1024);witness.load_elf(image,true);
  const auto before=witness.snapshot();
  witness.run(5);
  check(witness.machine().spsr==0x1000&&witness.machine().el==1,
        "The SSBS ERET witness did not set its saved process status");
  const auto checkpoint=witness.snapshot();
  auto restored=MinimalGuest::from_snapshot(checkpoint);
  check(restored.snapshot()==checkpoint,"SPSR with SSBS flag altered RVG history");
  restored.run(1);
  check(restored.machine().el==0&&restored.registers().pc==0x40080018 &&
        restored.machine().spsr==0x1000,
        "SPSR.SSBS was not accepted for the actual EL0 exception return");
  check(restored.registers().sp==restored.machine().sp0,
        "SSBS return did not restore the EL0 stack");
  restored.run(1);
  check(restored.machine().el==0&&restored.registers().pc==0x4008001c,
        "Target EL0 instruction did not execute after ERET");
  // A genuinely unsupported SPSR bit must still fail atomically.
  auto invalid=image;
  constexpr std::uint32_t mov_reserved=0xd2840000u; // MOV X0,#0x2000: reserved bit 13
  for(unsigned i=0;i<4;++i)invalid[256+i]=std::uint8_t(mov_reserved>>(i*8));
  MinimalGuest bad(128*1024*1024);bad.load_elf(invalid,true);
  bad.run(5);const auto saved=bad.snapshot();
  fails([&]{bad.run(1);},"Reserved SPSR bits were silently permitted");
  check(bad.snapshot()==saved,"Failed ERET violated atomic rollback");
  check(MinimalGuest::from_snapshot(before).snapshot()==before,
        "Previous guest checkpoint was changed by SSBS support");
}

void speculative_data_barrier(){
  // 0xd503229f is CSDB, independently assembled by Clang AArch64.
  const std::uint32_t instructions[]{
    0xd503229fu,0xd503201fu,0xd2800100u,0xf2b08000u,0xd4000002u
  };
  auto image=system_demo_elf();image.resize(256);
  for(auto instruction:instructions)put(image,instruction,4);
  for(unsigned i=0;i<8;++i){
    image[96+i]=std::uint8_t(std::uint64_t(sizeof(instructions))>>(8*i));
    image[104+i]=image[96+i];
  }
  MinimalGuest guest(128*1024*1024);guest.load_elf(image,true);
  const auto before_core=guest.runtime().core_snapshot();
  const auto before_memory=guest.runtime().memory_persistent_snapshot();
  const auto before_pc=guest.registers().pc;
  const auto before_registers=guest.registers().x;
  guest.run(1);
  check(guest.registers().pc==before_pc+4 &&
        guest.registers().x==before_registers &&
        guest.machine().exceptions==0,
        "CSDB did not execute as an architectural speculation-order hint");
  check(guest.runtime().core_snapshot()==before_core &&
        guest.runtime().memory_persistent_snapshot()==before_memory,
        "CSDB unexpectedly changed the XI/Ether state");
  const auto saved=guest.snapshot();
  auto restored=MinimalGuest::from_snapshot(saved);
  check(restored.snapshot()==saved,"CSDB checkpoint was not stable");
  const auto result=restored.run(10);
  check(result.halted&&result.exit_code==0,"Guest execution did not continue after CSDB");
}
void android_mmfr3_real_opcode() {
  // Exact 0xd5380761 observed in official Cuttlefish build 16644040 at
  // XI/Ether guest instruction 30,950: MRS X1, ID_AA64MMFR3_EL1.
  // Independent Clang AArch64 assembly must match this opcode in CI.
  auto image=system_demo_elf();image.resize(256);
  const std::uint32_t instructions[]{0xd28000e1u,0xd5380761u,0xd503201fu};
  for(auto instruction:instructions)put(image,instruction,4);
  for(unsigned i=0;i<8;++i){
    image[96+i]=std::uint8_t(std::uint64_t(sizeof(instructions))>>(i*8));
    image[104+i]=image[96+i];
  }
  MinimalGuest guest(128*1024*1024);
  guest.load_elf(image,true);
  const auto core=guest.runtime().core_snapshot();
  const auto memory=guest.runtime().memory_persistent_snapshot();
  guest.run(1);
  check(guest.registers().x[1]==7,"Sentinel before ID_AA64MMFR3 read failed");
  const auto before=guest.snapshot();
  auto one=guest;one.run(1);
  check(one.registers().x[1]==0&&
        one.registers().pc==guest.registers().pc+4&&
        one.machine().exceptions==0&&one.machine().el==1,
        "Real Android MRS ID_AA64MMFR3_EL1 did not return absent features");
  check(one.runtime().core_snapshot()==core&&
        one.runtime().memory_persistent_snapshot()==memory,
        "ID_AA64MMFR3 feature read changed XI/Ether state");
  auto restored=MinimalGuest::from_snapshot(before);
  restored.run(1);
  check(restored.snapshot()==one.snapshot(),
        "ID_AA64MMFR3 replay through RVG changed guest state");
}
void android_baseline_identification_opcodes() {
  // Actual instructions in the byte-pinned Cuttlefish CPU-info routine:
  // ISAR3 at +0x4007a8, MMFR4 at +0x4007d0, PFR2 at +0x4007e8,
  // FPFR0 at +0x400800. Test each named identification read, never a
  // blanket "unknown registers read zero" fallback.
  for(const std::uint32_t opcode:{0xd5380668u,0xd5380788u,0xd5380448u,0xd53804e8u}){
  auto image=system_demo_elf();image.resize(256);
  const std::uint32_t code[]{0xd28000e8u,opcode,0xd503201fu};
  for(auto instruction:code)put(image,instruction,4);
  for(unsigned i=0;i<8;++i){
    image[96+i]=std::uint8_t(std::uint64_t(sizeof(code))>>(8*i));
    image[104+i]=image[96+i];
  }
  MinimalGuest guest(128*1024*1024);guest.load_elf(image,true);guest.run(1);
  check(guest.registers().x[8]==7,"ISAR3 witness sentinel missing");
  const auto before=guest.snapshot();const auto registers=guest.registers();
  const auto core=guest.runtime().core_snapshot();
  const auto memory=guest.runtime().memory_persistent_snapshot();
  auto direct=guest;direct.run(1);
  check(direct.registers().x[8]==0&&direct.registers().pc==registers.pc+4&&
        direct.registers().instructions==registers.instructions+1&&
        direct.machine().el==1&&direct.machine().exceptions==0,
        "Actual Android ISAR3 read did not report absent features");
  auto expected=registers.x;expected[8]=0;
  check(direct.registers().x==expected&&direct.registers().sp==registers.sp&&
        direct.registers().nzcv==registers.nzcv,
        "ISAR3 changed unrelated architectural registers");
  check(direct.runtime().core_snapshot()==core&&
        direct.runtime().memory_persistent_snapshot()==memory,
        "Read-only ISAR3 altered the single XI/Ether authority");
  auto restored=MinimalGuest::from_snapshot(before);restored.run(1);
  check(restored.snapshot()==direct.snapshot(),"ISAR3 RVG resume changed the transition");
  check(guest.snapshot()==before,"ISAR3 modified its source checkpoint");

  // This identification register remains read-only. A rejected write must
  // roll back the whole transaction instead of silently changing a feature.
  const auto reject_opcode=[&](std::uint32_t opcode,const char* message){
    auto invalid=image;for(unsigned i=0;i<4;++i)invalid[260+i]=std::uint8_t(opcode>>(i*8));
    MinimalGuest reject(128*1024*1024);reject.load_elf(invalid,true);reject.run(1);
    const auto state=reject.snapshot();fails([&]{reject.run(1);},message);
    check(reject.snapshot()==state,"Rejected ISAR3/unknown access was not atomic");
  };
  reject_opcode(opcode&~UINT32_C(0x00200000),"Read-only identification register accepted a write");
  reject_opcode(0xd53806e8u,"Unimplemented adjacent ID register became readable");

  // An EL0 MRS must still take the existing architectural undefined trap.
  auto user=initial();for(unsigned i=0;i<150&&user.machine().el;++i)user.run(1);
  check(user.machine().el==0,"ISAR3 privilege witness did not reach EL0");
  const auto pc=user.registers().pc;
  user.write(user.physical_address(pc,false,true),opcode,4);
  const auto count=user.machine().exceptions;const auto x8=user.registers().x[8];
  user.run(1);
  check(user.machine().el==1&&user.machine().exceptions==count+1&&
        user.machine().elr==pc&&user.machine().esr==(UINT64_C(1)<<25)&&
        user.registers().x[8]==x8,
        "EL0 gained privileged ISAR3 access instead of taking the undefined trap");
  }
}
void identification(){
  auto image=system_demo_elf();image.resize(260);const std::uint32_t instruction=0xd53800c0; // MRS X0,REVIDR_EL1
  for(unsigned i=0;i<4;++i)image[256+i]=std::uint8_t(instruction>>(8*i));
  for(unsigned i=0;i<8;++i){image[96+i]=std::uint8_t(UINT64_C(4)>>(8*i));image[104+i]=image[96+i];}
  MinimalGuest guest(128*1024*1024);guest.load_elf(image,true);guest.run(1);
  check(guest.registers().x[0]==0&&guest.machine().exceptions==0,"Software implementation revision is not readable");
  for(auto id:{0xc024u,0xc025u,0xc03bu}){
    const auto op=0xd5200000u|(id<<5);for(unsigned i=0;i<4;++i)image[256+i]=std::uint8_t(op>>(8*i));
    guest.load_elf(image,true);guest.run(1);check(guest.registers().x[0]==0,"Absent SVE/SME feature incorrectly advertised");
  }
}
void uart_fifo_configuration(){
  // The pinned Linux 6.1 kernel writes IFLS via STRH W8,[X9] (0x79000128).
  for(bool halfword:{false,true}){
    const std::uint32_t code[]{
      0xd2a12001,0x52800240,                 // X1=PL011, W0=0x12
      halfword?0x79006820u:0xb9003420u,     // STRH/STR W0,[X1,#0x34]
      halfword?0x79406822u:0xb9403422u,     // LDRH/LDR W2,[X1,#0x34]
      0xd2800100,0xf2b08000,0xd4000002     // PSCI SYSTEM_OFF
    };
    auto image=system_demo_elf();image.resize(256);
    for(auto instruction:code)put(image,instruction,4);
    for(unsigned i=0;i<8;++i){image[96+i]=std::uint8_t(std::uint64_t(sizeof(code))>>(8*i));image[104+i]=image[96+i];}
    MinimalGuest guest(128*1024*1024);guest.load_elf(image,true);
    const auto baseline=guest.snapshot();
    check(guest.machine().uart_ifls==0,"Legacy machine default IFLS changed");
    guest.run(4);
    check(guest.machine().uart_ifls==0x12&&guest.registers().x[2]==0x12,
          "PL011 halfword/word IFLS write/readback failed");
    check(guest.machine().save()[0]==3,"Configured PL011 did not version its machine checkpoint");
    const auto restored=MinimalGuest::from_snapshot(guest.snapshot());
    check(restored.machine().uart_ifls==0x12&&restored.snapshot()==guest.snapshot(),
          "PL011 IFLS checkpoint was not exact");
    auto finish=restored;
    check(finish.run(3).halted,"Configured PL011 guest failed to shut down");
    MinimalGuest legacy(128*1024*1024);legacy.load_elf(image,true);
    check(legacy.machine().save()[0]==2&&legacy.snapshot()==baseline,
          "Legacy machine checkpoint bytes changed");
    check(MinimalGuest::from_snapshot(legacy.snapshot()).snapshot()==legacy.snapshot(),
          "Legacy PL011 checkpoint changed");
    auto invalid=image;constexpr std::uint32_t reserved=0x528000e0; // MOV W0,#7
    for(unsigned i=0;i<4;++i)invalid[260+i]=std::uint8_t(reserved>>(8*i));
    MinimalGuest reject(128*1024*1024);reject.load_elf(invalid,true);reject.run(2);
    const auto before=reject.snapshot();
    fails([&]{reject.run(1);},"Reserved PL011 FIFO trigger accepted");
    check(reject.snapshot()==before,"Rejected PL011 configuration changed CPU/device state");
  }
}
void interrupt_priority_initialization(){
  auto image=system_demo_elf();image.resize(256);const std::uint32_t code[]{0xd2801a01,0xf2a10021,0xb900003f,0xb9400020,0xd2800020,0xb9000020};
  for(auto instruction:code)put(image,instruction,4);
  for(unsigned i=0;i<8;++i){image[96+i]=std::uint8_t(std::uint64_t(sizeof(code))>>(8*i));image[104+i]=image[96+i];}
  MinimalGuest guest(128*1024*1024);guest.load_elf(image,true);guest.run(5);
  check(guest.machine().active_irqs[0]==0&&guest.machine().active_irqs[1]==0,"APR initialization created active interrupts");
  const auto before=guest.snapshot();fails([&]{guest.run(1);},"Unimplemented manual active-priority write accepted");
  check(guest.snapshot()==before,"Rejected APR write changed CPU/device state");
}
}
int main(){try{boot_and_restore();observe_userspace_without_execution();inactive_debug_slot();translation();store_invalidates_its_own_translation();page_table_store_dependencies();quantum_alias_invalidates_translation();shared_quantum_authority();exclusives();exclusive_pairs();paired_store_pins_physical_translation();lazy_ram_range_error_parity();image_boot_protocol();android_bootargs_snapshot_profile();large_linux_gki_loader_budget();cross_page_access();unprivileged_memory_access();eret_ssbs_saved_status();speculative_data_barrier();android_mmfr3_real_opcode();android_baseline_identification_opcodes();identification();uart_fifo_configuration();interrupt_priority_initialization();
  std::cout<<"{\"checks\":"<<checks<<",\"system\":\"own-EL1-MMU-IRQ-EL0-SVC\",\"existing_quantum_qubits\":4096,\"android_boot\":\"not_implemented\"}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
