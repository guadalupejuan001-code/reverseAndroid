// Continue the exact, independently audited C16 state; do not restart or edit it.
#include "guest_boot_cycle.hpp"
#include "guest_file_session.hpp"
#include <csignal>
#include <iostream>
using namespace reverse_pc::guest;
static volatile std::sig_atomic_t stopping=0;
static void stop(int){stopping=1;}
static const std::string state_sha="d2ef1f650948453e902122a7bcf6ddda577effb469c5c101a0eaec5f25394df5";
static const std::string report_sha="c11ef7f66b4e2138e8b6d450116ebc838d9f19514e9c1f8173fb17207e5a3f7f";
static const std::string disk_sha="28303ac7b2832cbde4e1c56551bf81816b55bc946cc75a444f2605316ed97e1c";
static const std::string seed_sha="78b0aeb569a7e87dc9e970bd1a46c57dfe3364c2f49f41cf688a57f0afa1b12f";
static std::uint64_t number(const char* text,std::uint64_t maximum){const std::string s(text);require(!s.empty()&&s.find_first_not_of("0123456789")==std::string::npos,"Expected decimal budget");const auto n=std::stoull(s);require(n&&n<=maximum,"Budget out of range");return n;}
int main(int argc,char** argv){try{
  require(argc==7,"Expected C16.rvg C16.json original-disk.raw new-journal wall-ms instructions");
  BootCycleLimits limits;limits.wall_ms=number(argv[5],600000);limits.instructions=number(argv[6],2000000000);
  limits.chunk=50000;limits.checkpoint_ms=15000;limits.checkpoint_instructions=25000000;
  require(!std::filesystem::exists(argv[4]),"Output journal already exists");
  std::cerr<<"REVERSE_C17_HOST_PHASE {\"phase\":\"verify_original_checkpoint\"}\n";
  const auto input=guest_read_file(argv[1],MinimalGuest::max_adaptive_snapshot);
  require(GuestSha256::hash(input)==state_sha,"Wrong C16 checkpoint");
  require(GuestSha256::hash(guest_read_file(argv[2],131072))==report_sha,"Wrong C16 report");
  std::cerr<<"REVERSE_C17_HOST_PHASE {\"phase\":\"verify_full_original_disk\",\"bytes\":8738832384}\n";
  const auto disk=GuestBlockImage::open(argv[3]);require(disk->size()==UINT64_C(8738832384)&&disk->sha256()==disk_sha,"Wrong original disk");
  std::cerr<<"REVERSE_C17_HOST_PHASE {\"phase\":\"restore_original_machine\"}\n";
  auto guest=MinimalGuest::from_snapshot(input,disk);require(guest.snapshot()==input,"Restore modified C16 state");
  require(guest.registers().instructions==1254508639&&guest.registers().pc==UINT64_C(0x7f8335e058),"Wrong C16 boundary");
  require(guest.machine().enabled&&guest.machine().image_kind==2&&guest.machine().pci.enabled&&guest.machine().el==0&&!guest.registers().halted&&!guest.machine().waiting,"Wrong C16 machine state");
  require(guest.logical_ram()==134217728,"RAM profile changed");
  const auto& clock=guest.machine().counter_latency;
  require(clock.enabled&&clock.instructions_per_tick==64&&clock.subtick==31,"Counter cadence changed");
  require(GuestSha256::hash(Bytes(clock.seed.begin(),clock.seed.end()))==seed_sha,"Counter seed changed");
  require(guest.read(guest.physical_address(guest.registers().pc,false,true),4)==0x2ea16c00,"Fault opcode changed");
  // Replay on a copy first. The actual continuation still starts at the original opcode.
  auto replay=guest;Vector128 expected;
  for(unsigned n=0;n<2;n++)expected.lane(n,32,std::min(guest.floating().v[0].lane(n,32),guest.floating().v[1].lane(n,32)));
  replay.run(1);
  require(replay.registers().instructions==1254508640&&replay.registers().pc==UINT64_C(0x7f8335e05c),"Original fault instruction did not retire");
  require(replay.floating().v[0].low==expected.low&&replay.floating().v[0].high==expected.high,"Original fault result differs");
  require(guest.snapshot()==input,"Replay changed original continuation");
  const std::string provenance="{\"experiment\":\"c17-c16-minmax-continuation\",\"input_checkpoint_sha256\":"+quote(state_sha)+",\"input_report_sha256\":"+quote(report_sha)+",\"input_init_execution_evidence\":\"C13 original entry and mounts via pinned C16 result\",\"fault_instruction_replayed_on_copy\":true,\"fault_instruction_result\":"+std::to_string(expected.low)+",\"timing_seed_origin\":\"checkpoint\",\"timing_seed_sha256\":"+quote(seed_sha)+",\"disk_sha256\":"+quote(disk_sha)+",\"ram_profile_changed\":false,\"original_state_modified\":false}";
  BootCycleJournal journal(argv[4]);std::signal(SIGTERM,stop);std::signal(SIGINT,stop);
  bool failure=false;std::size_t uart=0;auto last=std::chrono::steady_clock::now();
  const auto save=[&](const MinimalGuest& g,const BootCycleReport& r){journal.publish(g,r,provenance);if(journal.pending_cleanup())std::cerr<<"REVERSE_PCI_JOURNAL_CLEANUP {\"pending\":"<<journal.pending_cleanup()<<",\"error\":"<<quote(journal.cleanup_error().message())<<"}\n";std::cerr<<"REVERSE_PCI_SAVED {\"instructions\":"<<g.registers().instructions<<",\"generation\":"<<journal.sequence()<<"}\n";};
  const auto progress=[&](const MinimalGuest& g,const BootCycleReport& r){
    if(r.console.size()>uart){std::cerr<<"REVERSE_PCI_UART "<<quote(r.console.substr(uart))<<'\n';uart=r.console.size();}
    for(const char* s:{"init: InitFatalReboot:","init: Failed to mount required partitions early","init: Failed to mount /system:","Reboot failed -- System halted","reboot: Restarting system"})if(r.console.find(s)!=std::string::npos)failure=true;
    const auto now=std::chrono::steady_clock::now();if(now-last>=std::chrono::seconds(2)){std::cerr<<"REVERSE_PCI_CYCLE_HEARTBEAT {\"instructions\":"<<g.registers().instructions<<",\"pc\":"<<quote(hex(g.registers().pc))<<",\"elapsed_ms\":"<<r.elapsed_ms<<"}\n";last=now;}
  };
  // Original /init entry and successful mounts remain attributed to pinned C13 evidence.
  auto result=run_boot_cycle(guest,limits,save,{},progress,[&]{return stopping!=0||failure;});
  if(failure){result.reason="guest_boot_failure";result.error="Android reported a fatal mount/reboot failure";save(guest,result);}
  std::cout<<boot_cycle_json(guest,result,GuestSha256::hash(guest.snapshot()),provenance)<<'\n';
  return result.reason=="instruction_budget"||result.reason=="host_wall_budget"?0:2;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
