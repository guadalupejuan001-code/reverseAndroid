#include "guest_boot_cycle.hpp"
#include "guest_file_session.hpp"
#include "minimal_guest_pci_demo.hpp"
#include "minimal_guest_system_demo.hpp"
#include <iostream>
using namespace reverse_pc::guest;
static unsigned checks=0;
static void check(bool value,const char* why){++checks;require(value,why);}
static MinimalGuest graphics(){MinimalGuest g(128*1024*1024,0);g.load_linux_image(pci_graphics_witness_image(),{},"",{},true);return g;}
static MinimalGuest user_entry(){
  // Real MSR/ERET transition and a NOP at EL0, derived from the existing
  // independently assembled SPSR.SSBS witness. This fixture is not Android.
  const std::uint32_t code[]{0xd2820000u,0xd5184000u,0xd2800301u,0xf2a80101u,0xd5184021u,0xd69f03e0u,0xd503201fu,
    0xd503201fu,0xd503201fu,0xd503201fu,0xd503201fu};
  auto image=system_demo_elf();image.resize(256);for(auto word:code)put(image,word,4);
  for(unsigned i=0;i<8;i++){image[96+i]=std::uint8_t(std::uint64_t(sizeof(code))>>(8*i));image[104+i]=image[96+i];}
  MinimalGuest g(128*1024*1024,0);g.load_elf(image,true);return g;
}
static MinimalGuest demand_entry(bool irq){
  auto image=system_demo_elf();image.resize(256);image.resize(256+0x4000);
  for(unsigned i=0;i<8;i++){image[96+i]=std::uint8_t(UINT64_C(0x4000)>>(8*i));image[104+i]=image[96+i];}
  unsigned at=256;auto emit=[&](std::uint32_t word){for(unsigned i=0;i<4;i++)image.at(at++)=std::uint8_t(word>>(8*i));};
  auto mov=[&](unsigned r,std::uint64_t n){for(unsigned i=0;i<4;i++)emit((i?0xf2800000u:0xd2800000u)|(i<<21)|(unsigned((n>>(16*i))&65535)<<5)|r);};
  auto msr=[&](unsigned id,unsigned rt){emit(0xd5100000u|(id<<5)|rt);};
  mov(0,0x40081000);msr(guest_sysreg(3,0,12,0,0),0);
  mov(0,0x40010000);mov(1,0x401);emit(0xf9000001);mov(1,0x40000705);emit(0xf9000401);emit(0xf900081f);
  msr(guest_sysreg(3,0,2,0,0),0);msr(guest_sysreg(3,0,2,0,1),0);
  mov(1,0xff00);msr(guest_sysreg(3,0,10,2,0),1);mov(1,0x80190019);msr(guest_sysreg(3,0,2,0,2),1);
  mov(1,0x30d00801);msr(guest_sysreg(3,0,1,0,0),1);emit(0xd5033fdf);
  if(irq){
    mov(20,MachineState::gic_dist);mov(21,MachineState::gic_cpu);mov(0,1);emit(0xb9000280);emit(0xb90002a0);
    mov(0,255);emit(0xb90006a0);mov(0,0x40000000);emit(0xb9010280);
    msr(guest_sysreg(3,3,14,2,0),31);mov(0,1);msr(guest_sysreg(3,3,14,2,1),0);
  }
  mov(0,0x81001000);msr(guest_sysreg(3,0,4,1,0),0);mov(0,0x80083000);msr(guest_sysreg(3,0,4,0,1),0);msr(guest_sysreg(3,0,4,0,0),31);emit(0xd69f03e0);
  at=256+0x1400;emit(0x14000300); // Lower-EL synchronous vector -> native page-table handler.
  at=256+0x1480;emit(0x14000320); // Lower-EL IRQ vector -> timer acknowledgement.
  at=256+0x2000;mov(0,0x40010000);mov(1,0x400007c5);emit(0xf9000801);emit(0xd5033f9f);emit(0xd508871f);emit(0xd5033fdf);emit(0xd69f03e0);
  at=256+0x2100;mov(21,MachineState::gic_cpu);emit(0xb9400ea0);msr(guest_sysreg(3,3,14,2,1),31);emit(0xb90012a0);emit(0xd69f03e0);
  at=256+0x3000;emit(0xd503201f);emit(0xd4000001);emit(0xd503201f);emit(0x14000000);
  MinimalGuest g(128*1024*1024,0);g.load_elf(image,true);return g;
}
static bool verify_demand_entry(MinimalGuest& g){
  require(g.registers().pc==0x80083000,"Demand fixture returned to a different EL0 PC");
  require(g.read(g.physical_address(g.registers().pc,false,true),4)==0xd503201f,"Demand fixture instruction bytes differ");g.run(1);return !g.machine().el&&g.registers().pc==0x80083004;
}
int main(){try{
  auto direct=graphics();direct.run(1000);auto split=graphics();split.run(200);
  const auto input=split.snapshot();split=MinimalGuest::from_snapshot(input);
  BootCycleLimits limits;limits.instructions=1000;limits.wall_ms=30000;limits.chunk=37;limits.checkpoint_instructions=70;
  std::vector<Bytes> saves;const auto report=run_boot_cycle(split,limits,[&](const MinimalGuest& g,const BootCycleReport&){saves.push_back(g.snapshot());});
  check(report.initial_instructions==200,"Cycle restarted the CPU");check(report.reason=="halted","Real graphics program did not halt");
  check(split.snapshot()==direct.snapshot(),"Resumed and direct complete snapshots differ");check(split.display_rgb()==direct.display_rgb(),"Resumed display differs");
  check(report.console=="REVERSE: PCI GPU 2D OK\n","UART continuation differs");check(saves.size()>=4,"Periodic instruction saves did not occur");
  check(!report.first_user_instruction_verified,"Bare-metal graphics was called userspace");
  for(const auto& state:saves){const auto restored=MinimalGuest::from_snapshot(state);check(restored.snapshot()==state,"Periodic state does not restore exactly");}
  auto bounded=graphics();limits.instructions=103;const auto bounded_report=run_boot_cycle(bounded,limits);
  check(bounded.registers().instructions==103&&bounded_report.reason=="instruction_budget","Instruction boundary was exceeded");
  auto paused=graphics();const auto paused_state=paused.snapshot();const auto paused_report=run_boot_cycle(paused,limits,{},{},{},[]{return true;});
  check(paused_report.reason=="host_interrupted"&&paused.snapshot()==paused_state,"Host stop changed the guest");
  auto bad_image=pci_graphics_witness_image();for(unsigned i=0;i<4;i++)bad_image[256+13*4+i]=255;
  MinimalGuest bad(128*1024*1024,0);bad.load_linux_image(bad_image,{},"",{},true);
  auto exact=bad;exact.run(14);const auto exact_state=exact.snapshot();limits.instructions=100;
  const auto failed=run_boot_cycle(bad,limits);
  check(failed.reason=="architectural_stop"&&!failed.error.empty(),"Unsupported transition was concealed");
  check(bad.snapshot()==exact_state,"Fault localization changed the successful prefix");
  auto rejected=bad;bool threw=false;try{rejected.run(1);}catch(const std::exception&){threw=true;}
  check(threw&&rejected.snapshot()==exact_state,"Faulting instruction was committed");
  auto late_image=pci_graphics_witness_image();bool found_hvc=false;
  for(unsigned at=256;at<4096;at+=4)if(get(late_image,at,4)==0xd4000002){
    for(unsigned i=0;i<4;i++)late_image[at+i]=255;found_hvc=true;break;
  }
  check(found_hvc,"Graphics witness lost its real poweroff instruction");
  MinimalGuest late(128*1024*1024,0);late.load_linux_image(late_image,{},"",{},true);auto late_reference=late;late_reference.run(527);
  limits.instructions=1000;limits.chunk=1000;const auto late_report=run_boot_cycle(late,limits);
  check(late_report.reason=="architectural_stop"&&late.snapshot()==late_reference.snapshot(),"Fault rollback/localization lost DMA or device state");
  check(late_report.console=="REVERSE: PCI GPU 2D OK\n","Fault localization lost the successful UART prefix");
  check(late.display_rgb()==direct.display_rgb(),"Fault localization changed the actual presented pixels");
  MinimalGuest wake(128*1024*1024,0);wake.load_elf(system_demo_elf(),true);auto wake_reference=wake;
  const auto wake_direct=wake_reference.run(10000);BootCycleLimits wake_limits;wake_limits.instructions=10000;wake_limits.wall_ms=30000;wake_limits.chunk=1;
  const auto wake_report=run_boot_cycle(wake,wake_limits);
  check(wake_report.reason=="halted","A timer-backed WFI at the transaction boundary was mistaken for an external-device wait");
  check(wake.snapshot()==wake_reference.snapshot(),"One-instruction chunks changed actual timer/IRQ/ERET state");
  check(wake_report.console==wake_direct.output,"Timer wakeup lost native UART output");
  auto sleep_image=system_demo_elf();for(unsigned i=0;i<4;i++)sleep_image[256+i]=std::uint8_t(UINT32_C(0xd503207f)>>(i*8));
  MinimalGuest sleep(128*1024*1024,0);sleep.load_elf(sleep_image,true);auto sleep_reference=sleep;sleep_reference.run(1);
  const auto sleep_report=run_boot_cycle(sleep,wake_limits);
  check(sleep_report.reason=="waiting_for_device_or_interrupt"&&sleep.registers().instructions==1,"A genuinely blocked WFI was not returned to the host");
  check(sleep.snapshot()==sleep_reference.snapshot(),"A blocked WFI retry mutated the native state");
  auto entry=user_entry(),entry_reference=entry;entry_reference.run(7);limits.instructions=7;
  const auto entry_report=run_boot_cycle(entry,limits,{},[](MinimalGuest& g){
    require(!g.machine().el&&g.registers().pc==0x40080018,"The real ERET entry was missed");g.run(1);return g.registers().pc==0x4008001c;
  });
  check(entry_report.first_user_instruction_verified,"Verified first userspace opcode was not recorded");
  check(entry.snapshot()==entry_reference.snapshot(),"Entry observation changed CPU state or exceeded its budget");
  auto milestone=user_entry();limits.instructions=1000;limits.stop_after_first_user_instruction=true;
  std::vector<Bytes> milestone_saves;
  const auto milestone_report=run_boot_cycle(milestone,limits,[&](const MinimalGuest& g,const BootCycleReport&){milestone_saves.push_back(g.snapshot());},[](MinimalGuest& g){
    require(!g.machine().el&&g.registers().pc==0x40080018,"Milestone missed the ERET boundary");g.run(1);return g.registers().pc==0x4008001c;
  });
  check(milestone_report.first_user_instruction_verified&&milestone_report.reason=="first_user_instruction_verified","Verified first-instruction stop was not reported");
  check(milestone.snapshot()==entry_reference.snapshot(),"Milestone executed beyond the single verified userspace instruction");
  check(milestone_saves.size()==2&&milestone_saves.back()==entry_reference.snapshot(),"Milestone did not save the exact verified boundary");
  bool needs_verifier=false;try{run_boot_cycle(milestone,limits);}catch(const std::exception&){needs_verifier=true;}
  check(needs_verifier&&milestone.snapshot()==entry_reference.snapshot(),"Milestone stop without a verifier was accepted");
  limits.stop_after_first_user_instruction=false;
  auto unverified=user_entry();unverified.run(6);const auto boundary=unverified.snapshot();
  const auto unverified_report=run_boot_cycle(unverified,limits,{},[](MinimalGuest& trial){trial.run(1);return false;});
  check(unverified_report.reason=="unverified_userspace_entry"&&!unverified_report.first_user_instruction_verified,"Unverified entry was accepted");
  check(unverified.snapshot()==boundary,"Failed entry verifier committed a partial transition");
  const auto faulted_entry=run_boot_cycle(unverified,limits,{},[](MinimalGuest&)->bool{throw MachineFault{0x96000004,0x1234};});
  check(faulted_entry.reason=="unverified_userspace_entry"&&!faulted_entry.first_user_instruction_verified,"Architectural observation fault escaped its boundary");
  check(unverified.snapshot()==boundary,"Architectural observation fault changed the CPU");
  const auto forged_entry=run_boot_cycle(unverified,limits,{},[](MinimalGuest& g)->bool{throw MachineFault{0x82000005,g.registers().pc};});
  check(forged_entry.reason=="unverified_userspace_entry"&&!forged_entry.first_user_instruction_verified&&unverified.snapshot()==boundary,"A forged instruction-abort observation changed or verified the caller");
  for(bool irq:{false,true}){
    auto demand=demand_entry(irq),reference=demand;reference.run(1000,true);
    if(irq){reference.run(1);reference.run(1000,true);}
    reference.run(1);reference.run(1000,true);reference.run(1);
    BootCycleLimits demand_limits;demand_limits.instructions=1000;demand_limits.wall_ms=30000;demand_limits.chunk=11;demand_limits.stop_after_first_user_instruction=true;
    const auto demand_report=run_boot_cycle(demand,demand_limits,{},verify_demand_entry);
    check(demand_report.reason=="first_user_instruction_verified"&&demand_report.first_user_instruction_verified,"Native demand paging did not reach the verified first opcode");
    check(demand_report.entry_instruction_faults==1&&demand_report.entry_interrupts==unsigned(irq),"Entry fault or pending timer was fabricated or lost");
    check(demand.snapshot()==reference.snapshot(),"Observed demand paging changed the actual CPU, page tables or device state");
  }
  auto fault_budget=demand_entry(false);fault_budget.run(1000,true);auto fault_reference=fault_budget;fault_reference.run(1);
  BootCycleLimits fault_limits;fault_limits.instructions=1;fault_limits.wall_ms=30000;fault_limits.chunk=1;fault_limits.stop_after_first_user_instruction=true;
  const auto budget_report=run_boot_cycle(fault_budget,fault_limits,{},verify_demand_entry);
  check(budget_report.reason=="instruction_budget"&&!budget_report.first_user_instruction_verified&&budget_report.entry_instruction_faults==1,"A native fetch fault falsely established the first-instruction milestone");
  check(fault_budget.snapshot()==fault_reference.snapshot(),"Demand-fault delivery exceeded its one-transition budget");
  auto recovered_fault=MinimalGuest::from_snapshot(fault_budget.snapshot());fault_limits.instructions=1000;
  const auto recovered_report=run_boot_cycle(recovered_fault,fault_limits,{},verify_demand_entry);
  fault_reference.run(1000,true);fault_reference.run(1);
  check(recovered_report.first_user_instruction_verified&&recovered_report.entry_instruction_faults==0,"A restart inside the native fault handler did not reach the original first opcode");
  check(recovered_fault.snapshot()==fault_reference.snapshot(),"Restarting after an entry fault changed the complete native state");
  auto wrong_fault=demand_entry(false);wrong_fault.run(1000,true);const auto wrong_before=wrong_fault.snapshot();
  const auto wrong_report=run_boot_cycle(wrong_fault,fault_limits,{},[](MinimalGuest& g)->bool{throw MachineFault{0x82000005,g.registers().pc+4};});
  check(wrong_report.reason=="unverified_userspace_entry"&&wrong_fault.snapshot()==wrong_before,"A different observed fault address was delivered to the native CPU");
  // Continue an already verified native EL0 boundary. A proof is bound to
  // the entire source; it must never re-run the first-entry callback.
  auto continuation=entry_reference;
  const auto proof_state=continuation.snapshot();
  BootCycleEntryProof prior{GuestSha256::hash(proof_state),continuation.registers().instructions,continuation.registers().pc};
  BootCycleLimits post_limits;post_limits.instructions=3;post_limits.wall_ms=30000;post_limits.chunk=2;
  auto post_reference=continuation;post_reference.run(3);
  unsigned entry_calls=0;
  const auto post_report=run_boot_cycle(continuation,post_limits,{},[&](MinimalGuest&)->bool{++entry_calls;throw std::runtime_error("Entry repeated");},{},{},&prior);
  check(post_report.reason=="instruction_budget"&&entry_calls==0,"Proven continuation repeated the original entry verifier");
  check(post_report.prior_user_instruction_verified&&!post_report.first_user_instruction_verified&&post_report.verified_entry_checkpoint_sha256==prior.checkpoint_sha256,"Carried proof was falsely reported as newly executed");
  check(continuation.snapshot()==post_reference.snapshot(),"Proven continuation changed the complete native state or exceeded budget");
  auto reject_proof=[&](MinimalGuest& g,const BootCycleEntryProof& proof,const BootCycleLimits& bound){
    const auto before=g.snapshot();unsigned saves=0;bool rejected=false;
    try{run_boot_cycle(g,bound,[&](const MinimalGuest&,const BootCycleReport&){++saves;},[](MinimalGuest&)->bool{return true;},{},{},&proof);}catch(const std::exception&){rejected=true;}
    check(rejected&&saves==0&&g.snapshot()==before,"Rejected continuation proof saved or mutated native state");
  };
  auto proof_guest=entry_reference;auto forged=prior;forged.checkpoint_sha256[0]=forged.checkpoint_sha256[0]=='0'?'1':'0';reject_proof(proof_guest,forged,post_limits);
  forged=prior;++forged.pc;reject_proof(proof_guest,forged,post_limits);
  forged=prior;++forged.instructions;reject_proof(proof_guest,forged,post_limits);
  forged=prior;forged.checkpoint_sha256="not-a-hash";reject_proof(proof_guest,forged,post_limits);
  auto changed_ram=proof_guest;changed_ram.write(0x40090000,0x42,1);reject_proof(changed_ram,prior,post_limits);
  auto changed_clock=proof_guest;changed_clock.enable_counter_latency({});reject_proof(changed_clock,prior,post_limits);
  auto repeat_limits=post_limits;repeat_limits.stop_after_first_user_instruction=true;reject_proof(proof_guest,prior,repeat_limits);
  // Real unsupported userspace opcode: preserve the precise successful prefix.
  auto post_bad=proof_guest;post_bad.write(post_bad.physical_address(post_bad.registers().pc+8),0xffffffff,4);
  BootCycleEntryProof bad_proof{GuestSha256::hash(post_bad.snapshot()),post_bad.registers().instructions,post_bad.registers().pc};
  auto post_bad_reference=post_bad;post_bad_reference.run(2);post_limits.instructions=20;post_limits.chunk=20;
  const auto post_bad_report=run_boot_cycle(post_bad,post_limits,{},[](MinimalGuest&)->bool{throw std::runtime_error("entry replay");},{},{},&bad_proof);
  check(post_bad_report.reason=="architectural_stop"&&!post_bad_report.error.empty()&&post_bad_report.prior_user_instruction_verified,"Userspace architecture failure was hidden or lost its proven origin");
  check(post_bad.snapshot()==post_bad_reference.snapshot(),"Userspace failure committed the rejected instruction or lost its prefix");
  auto syscall=demand_entry(false);syscall.run(1000,true);syscall.run(1);syscall.run(1000,true);syscall.run(1);
  BootCycleEntryProof syscall_proof{GuestSha256::hash(syscall.snapshot()),syscall.registers().instructions,syscall.registers().pc};
  auto syscall_reference=syscall;syscall_reference.run(100);const auto before_exceptions=syscall.machine().exceptions;
  post_limits.instructions=100;post_limits.chunk=3;
  const auto syscall_report=run_boot_cycle(syscall,post_limits,{},[](MinimalGuest&)->bool{throw std::runtime_error("entry repeated after syscall");},{},{},&syscall_proof);
  check(syscall_report.reason=="instruction_budget"&&syscall_report.prior_user_instruction_verified&&!syscall.machine().el&&syscall.machine().exceptions==before_exceptions+1,"Proven userspace did not execute native SVC, handler and ERET");
  check(syscall.snapshot()==syscall_reference.snapshot(),"Proven syscall continuation changed CPU, mappings, clock or XI memory");
  auto undefined=entry_reference;const auto undef_pc=undefined.registers().pc;
  undefined.write(undefined.physical_address(undef_pc),0xd5300000u|(guest_sysreg(3,0,0,0,0)<<5),4);
  const auto undef_before=undefined.snapshot();auto delivered_undefined=undefined;delivered_undefined.run(1);
  check(delivered_undefined.machine().el==1&&delivered_undefined.machine().elr==undef_pc&&delivered_undefined.machine().esr==(1u<<25),"Default native EL0 undefined exception behavior changed");
  BootCycleEntryProof undef_proof{GuestSha256::hash(undef_before),undefined.registers().instructions,undef_pc};
  post_limits.stop_before_user_undefined=true;
  const auto undef_report=run_boot_cycle(undefined,post_limits,{},{},{},{},&undef_proof);
  check(undef_report.reason=="architectural_stop"&&undef_report.error.find("EL0 undefined instruction trap")!=std::string::npos&&undefined.snapshot()==undef_before,"Diagnostic boundary committed an undefined trap or hid its exact PC");
  post_limits.stop_before_user_undefined=false;
  auto panic_image=system_demo_elf();panic_image.resize(256);
  auto panic_emit=[&](std::uint32_t w){put(panic_image,w,4);};
  for(unsigned i=0;i<4;i++)panic_emit((i?0xf2800000u:0xd2800000u)|(i<<21)|(unsigned((MachineState::uart_base>>(16*i))&65535)<<5));
  const std::string panic_text="Kernel panic - not syncing: test\n";
  for(unsigned char c:panic_text){panic_emit(0x52800001u|(unsigned(c)<<5));panic_emit(0x39000001u);}
  panic_emit(0x14000000u);
  for(unsigned i=0;i<8;i++){panic_image[96+i]=std::uint8_t(std::uint64_t(panic_image.size()-256)>>(8*i));panic_image[104+i]=panic_image[96+i];}
  MinimalGuest panic_guest(128*1024*1024,0);panic_guest.load_elf(panic_image,true);auto panic_reference=panic_guest;
  post_limits.instructions=1000;post_limits.chunk=13;
  const auto panic_report=run_boot_cycle(panic_guest,post_limits);
  panic_reference.run(panic_guest.registers().instructions);
  check(panic_report.reason=="guest_kernel_panic"&&panic_report.console.find("Kernel panic - not syncing")!=std::string::npos&&panic_guest.registers().instructions<100,"A kernel panic was counted as sustained boot progress");
  check(panic_guest.snapshot()==panic_reference.snapshot(),"Panic observation changed the native successful prefix");
  const auto root=std::filesystem::path("evidence/c04/cycle-journal-test-"+std::to_string(::getpid()));
  std::filesystem::create_directories(root.parent_path());BootCycleJournal journal(root);
  journal.publish(bad,failed);journal.publish(split,report);journal.publish(bounded,bounded_report);
  check(journal.sequence()==3,"Journal generation was lost");check(!std::filesystem::exists(root/"generation-1"),"Journal retained unbounded history");
  check(std::filesystem::exists(root/"generation-2")&&std::filesystem::exists(root/"generation-3"),"Journal lost a complete generation");
  const auto newest=guest_read_file((root/"generation-3/checkpoint.rvg").string(),MinimalGuest::max_adaptive_snapshot);
  check(newest==bounded.snapshot(),"Latest journal checkpoint differs");
  const auto pointer_bytes=guest_read_file((root/"latest.json").string(),4096);const std::string pointer(pointer_bytes.begin(),pointer_bytes.end());
  check(pointer.find("generation-3")!=std::string::npos&&pointer.find(GuestSha256::hash(newest))!=std::string::npos,"Journal pointer is not bound to the state");
  std::filesystem::create_directory(root/".pending-4");std::ofstream(root/".pending-4/checkpoint.rvg")<<"incomplete";
  check(guest_read_file((root/"latest.json").string(),4096)==pointer_bytes,"Unfinished generation replaced the last complete state");
  bool protected_output=false;try{BootCycleJournal same(root);}catch(const std::exception&){protected_output=true;}
  check(protected_output&&guest_read_file((root/"generation-3/checkpoint.rvg").string(),MinimalGuest::max_adaptive_snapshot)==newest,"Existing journal output was overwritten");
  std::filesystem::remove_all(root);
  std::cout<<"{\"checks\":"<<checks<<",\"exact_resume\":true,\"persisted_pixels\":true,\"fault_prefix_preserved\":true,\"atomic_generation_pointer\":true,\"android_boot\":false}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
