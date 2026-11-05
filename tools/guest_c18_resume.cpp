// Continue the exact, independently audited C17 state; do not restart or edit it.
#include "guest_budget_cycles.hpp"
#include "guest_file_session.hpp"
#include <csignal>
#include <iostream>
using namespace reverse_pc::guest;
static volatile std::sig_atomic_t stopping=0;
static void stop(int){stopping=1;}
struct Origin {const char* state;const char* report;std::uint64_t instructions,pc,subtick;};
// Two independently verified clean C17 boundaries. No caller-selected CPU or clock edits.
static constexpr Origin origins[]={
 {"ff336b8dac31c8059259e8ae2fc9a043626f6f57a84f1061d56fed77c90e8a31","f3b13ca1e2b69fdafe223da565bfab069b53c2cff3d6d69c55f02526a5774407",1445008639,UINT64_C(0xffffffc0804973c8),63},
 {"3ebccb5a9503dfc103229b1a9675490d7c07e7a7fb8fa90ed664ffb86da1bba2","7393c558c7ab7d5e60c9d6486c64f93e7acd9fe9121652fc16a75c10104eff95",1710758639,UINT64_C(0xffffffc080262a48),59}
};
static const std::string disk_sha="28303ac7b2832cbde4e1c56551bf81816b55bc946cc75a444f2605316ed97e1c";
static const std::string seed_sha="78b0aeb569a7e87dc9e970bd1a46c57dfe3364c2f49f41cf688a57f0afa1b12f";
static std::uint64_t number(const char* text,std::uint64_t maximum){const std::string s(text);require(!s.empty()&&s.find_first_not_of("0123456789")==std::string::npos,"Expected decimal budget");const auto n=std::stoull(s);require(n&&n<=maximum,"Budget out of range");return n;}
int main(int argc,char** argv){try{
  require(argc==7,"Expected C17.rvg C17.json original-disk.raw new-journal wall-ms instructions");
  BootCycleLimits limits;limits.wall_ms=number(argv[5],600000);limits.instructions=number(argv[6],2000000000);
  limits.chunk=50000;limits.checkpoint_ms=15000;limits.checkpoint_instructions=25000000;
  require(!std::filesystem::exists(argv[4]),"Output journal already exists");
  std::cerr<<"REVERSE_C18_HOST_PHASE {\"phase\":\"verify_original_checkpoint\"}\n";
  const auto input=guest_read_file(argv[1],MinimalGuest::max_adaptive_snapshot);
  const auto state_sha=GuestSha256::hash(input);const Origin* selected=nullptr;
  for(const auto& origin:origins)if(state_sha==origin.state)selected=&origin;
  require(selected,"Wrong C17 checkpoint");const auto& origin=*selected;const std::string report_sha=origin.report;
  require(GuestSha256::hash(guest_read_file(argv[2],131072))==report_sha,"Wrong C17 report");
  std::cerr<<"REVERSE_C18_HOST_PHASE {\"phase\":\"verify_full_original_disk\",\"bytes\":8738832384}\n";
  const auto disk=GuestBlockImage::open(argv[3]);require(disk->size()==UINT64_C(8738832384)&&disk->sha256()==disk_sha,"Wrong original disk");
  std::cerr<<"REVERSE_C18_HOST_PHASE {\"phase\":\"restore_original_machine\"}\n";
  auto guest=MinimalGuest::from_snapshot(input,disk);require(guest.snapshot()==input,"Restore modified C17 state");
  require(guest.registers().instructions==origin.instructions&&guest.registers().pc==origin.pc,"Wrong C17 boundary");
  require(guest.machine().enabled&&guest.machine().image_kind==2&&guest.machine().pci.enabled&&guest.machine().el==1&&!guest.registers().halted&&!guest.machine().waiting,"Wrong C17 machine state");
  require(guest.logical_ram()==134217728,"RAM profile changed");
  const auto& clock=guest.machine().counter_latency;
  require(clock.enabled&&clock.instructions_per_tick==64&&clock.subtick==origin.subtick,"Counter cadence changed");
  require(GuestSha256::hash(Bytes(clock.seed.begin(),clock.seed.end()))==seed_sha,"Counter seed changed");
  const std::string provenance="{\"experiment\":\"c18-c17-continuous-budget-boot\",\"input_checkpoint_sha256\":"+quote(state_sha)+",\"input_report_sha256\":"+quote(report_sha)+",\"snapshot_round_trip_verified\":true,\"cycle_limit\":3,\"timing_seed_origin\":\"checkpoint\",\"timing_seed_sha256\":"+quote(seed_sha)+",\"disk_sha256\":"+quote(disk_sha)+",\"ram_profile_changed\":false,\"original_state_modified\":false}";
  BootCycleJournal journal(argv[4]);std::signal(SIGTERM,stop);std::signal(SIGINT,stop);
  bool failure=false;std::size_t uart=0;auto last=std::chrono::steady_clock::now(),frame_time=last;
  std::uint64_t captured_frame=UINT64_MAX;
  const auto save=[&](const MinimalGuest& g,const BootCycleReport& r){journal.publish(g,r,provenance);if(journal.pending_cleanup())std::cerr<<"REVERSE_PCI_JOURNAL_CLEANUP {\"pending\":"<<journal.pending_cleanup()<<",\"error\":"<<quote(journal.cleanup_error().message())<<"}\n";std::cerr<<"REVERSE_PCI_SAVED {\"instructions\":"<<g.registers().instructions<<",\"generation\":"<<journal.sequence()<<"}\n";};
  const auto progress=[&](const MinimalGuest& g,const BootCycleReport& r){
    if(r.console.size()>uart){std::cerr<<"REVERSE_PCI_UART "<<quote(r.console.substr(uart))<<'\n';uart=r.console.size();}
    for(const char* s:{"init: InitFatalReboot:","init: Failed to mount required partitions early","init: Failed to mount /system:","Reboot failed -- System halted","reboot: Restarting system"})if(r.console.find(s)!=std::string::npos)failure=true;
    const auto now=std::chrono::steady_clock::now();if(now-last>=std::chrono::seconds(2)){std::cerr<<"REVERSE_PCI_CYCLE_HEARTBEAT {\"instructions\":"<<g.registers().instructions<<",\"pc\":"<<quote(hex(g.registers().pc))<<",\"elapsed_ms\":"<<r.elapsed_ms<<"}\n";last=now;}
    const auto& display=g.machine().pci;
    if(display.scan_w&&display.scan_h&&display.flushes!=captured_frame&&now-frame_time>=std::chrono::seconds(2)){
      frame_time=now;
      try{
        const auto pixels=g.display_rgb();require(pixels.size()==std::size_t(display.scan_w)*display.scan_h*3,"Frame extent differs");
        const auto root=std::filesystem::path(argv[4]).parent_path(),pending=root/"frame.pending.ppm",target=root/"frame.ppm";
        std::ofstream out(pending,std::ios::binary|std::ios::trunc);out<<"P6\n"<<display.scan_w<<' '<<display.scan_h<<"\n255\n";
        out.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()));out.flush();require(bool(out),"Cannot capture native display");out.close();std::filesystem::rename(pending,target);
        captured_frame=display.flushes;std::cerr<<"REVERSE_PCI_FRAME {\"width\":"<<display.scan_w<<",\"height\":"<<display.scan_h<<",\"frame\":"<<captured_frame<<"}\n";
      }catch(const std::exception& e){std::cerr<<"REVERSE_PCI_FRAME_ERROR "<<quote(e.what())<<'\n';}
    }
  };
  // Resume only clean budget boundaries; never restart an ISA failure, panic or halted machine.
  unsigned cycles=0;auto result=run_boot_budget_cycles(guest,limits,3,save,progress,[&]{return stopping!=0||failure;},&cycles);
  std::cerr<<"REVERSE_C18_CYCLES_COMPLETED "<<cycles<<'\n';
  if(failure){result.reason="guest_boot_failure";result.error="Android reported a fatal mount/reboot failure";save(guest,result);}
  std::cout<<boot_cycle_json(guest,result,GuestSha256::hash(guest.snapshot()),provenance)<<'\n';
  return result.reason=="instruction_budget"||result.reason=="host_wall_budget"?0:2;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
