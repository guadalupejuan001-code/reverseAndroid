// Continue a pinned, clean C27 budget without restarting its CPU or editing its state.
#include "guest_budget_cycles.hpp"
#include "guest_file_session.hpp"
#include <csignal>
#include <iostream>
using namespace reverse_pc::guest;
static volatile std::sig_atomic_t stopping=0;
static void stop(int){stopping=1;}
static const std::string state_sha="9db40b80214344583f855ccc777eb74963ece41ca5a1afab8de0737507f0e939";
static const std::string report_sha="7ff064cd92bb1fffa7de798e971abc456dd8032b7cebc0bb7c6cc44aee34e903";
static const std::string disk_sha="4c2eaf3fa7d81e9649072fc81f0651f4129bd9f41d4a9f9964cc627741a7b92e";
static const std::string seed_sha="699e17ffab1ef3618567ee928b25cdc0b6c7feed3d520147e9111c76a751a6cb";
static std::uint64_t number(const char* text,std::uint64_t maximum){const std::string s(text);require(!s.empty()&&s.find_first_not_of("0123456789")==std::string::npos,"Expected decimal budget");const auto n=std::stoull(s);require(n&&n<=maximum,"Budget out of range");return n;}
int main(int argc,char** argv){try{
  if(argc==2&&std::string(argv[1])=="--pins"){
    std::cout<<"{\"schema\":\"reverse-c28-native-pins-v1\",\"state_sha256\":"<<quote(state_sha)<<",\"report_sha256\":"<<quote(report_sha)<<",\"disk_sha256\":"<<quote(disk_sha)<<",\"counter_seed_sha256\":"<<quote(seed_sha)<<",\"host_snapshot_transport_bytes\":"<<MinimalGuest::max_explicit_adaptive_snapshot<<"}\n";return 0;
  }
  require(argc==7,"Expected C27.rvg C27.json original-disk.raw new-journal wall-ms instructions");
  BootCycleLimits limits;limits.wall_ms=number(argv[5],900000);limits.instructions=number(argv[6],2000000000);
  limits.chunk=50000;limits.checkpoint_ms=60000;limits.checkpoint_instructions=100000000;
  require(!std::filesystem::exists(argv[4]),"Output journal already exists");
  std::cerr<<"REVERSE_C28_HOST_PHASE {\"phase\":\"verify_original_checkpoint\"}\n";
  const auto input=guest_read_file(argv[1],MinimalGuest::max_explicit_adaptive_snapshot);
  require(GuestSha256::hash(input)==state_sha,"Wrong C27 checkpoint");
  require(GuestSha256::hash(guest_read_file(argv[2],8*1024*1024))==report_sha,"Wrong C27 report");
  std::cerr<<"REVERSE_C28_HOST_PHASE {\"phase\":\"verify_full_original_disk\",\"bytes\":10888413184}\n";
  const auto disk=GuestBlockImage::open(argv[3]);require(disk->size()==UINT64_C(10888413184)&&disk->sha256()==disk_sha,"Wrong original disk");
  std::cerr<<"REVERSE_C28_HOST_PHASE {\"phase\":\"restore_original_machine\"}\n";
  auto guest=MinimalGuest::from_snapshot(input,disk,MinimalGuest::max_explicit_adaptive_snapshot);require(guest.snapshot()==input,"Restore modified C27 state");
  require(guest.registers().instructions==2833048839&&guest.registers().pc==UINT64_C(0x5568b464ec),"Wrong C27 boundary");
  require(guest.machine().enabled&&guest.machine().image_kind==2&&guest.machine().pci.enabled&&guest.machine().el<=1&&!guest.registers().halted&&!guest.machine().waiting,"Wrong C27 machine state");
  require(guest.logical_ram()==2147483648,"RAM profile changed");
  const auto& clock=guest.machine().counter_latency;
  require(clock.enabled&&clock.instructions_per_tick==64&&clock.subtick==23,"Counter cadence changed");
  require(GuestSha256::hash(Bytes(clock.seed.begin(),clock.seed.end()))==seed_sha,"Counter seed changed");
  require(guest.host_snapshot_transport_capacity()==MinimalGuest::max_explicit_adaptive_snapshot,"Explicit host bound lost");
  const std::string provenance="{\"experiment\":\"c28-c27-clean-continuation\",\"input_checkpoint_sha256\":"+quote(state_sha)+",\"input_report_sha256\":"+quote(report_sha)+",\"snapshot_round_trip_verified\":true,\"source_c27_original_init_verified\":true,\"source_c27_second_stage_verified\":true,\"source_c27_selinux_enforcing_verified\":true,\"host_snapshot_transport_bytes\":1073741824,\"host_transport_policy_serialized\":false,\"cycle_limit\":3,\"timing_seed_origin\":\"checkpoint\",\"timing_seed_sha256\":"+quote(seed_sha)+",\"disk_sha256\":"+quote(disk_sha)+",\"ram_profile_changed\":false,\"original_state_modified\":false}";
  BootCycleJournal journal(argv[4]);std::signal(SIGTERM,stop);std::signal(SIGINT,stop);
  bool failure=false;std::size_t uart=0;auto last=std::chrono::steady_clock::now(),frame_time=last;
  std::uint64_t captured_frame=UINT64_MAX;
  const auto save=[&](const MinimalGuest& g,const BootCycleReport& r){journal.publish(g,r,provenance);if(journal.pending_cleanup())std::cerr<<"REVERSE_PCI_JOURNAL_CLEANUP {\"pending\":"<<journal.pending_cleanup()<<",\"error\":"<<quote(journal.cleanup_error().message())<<"}\n";std::cerr<<"REVERSE_PCI_SAVED {\"instructions\":"<<g.registers().instructions<<",\"generation\":"<<journal.sequence()<<"}\n";};
  const auto progress=[&](const MinimalGuest& g,const BootCycleReport& r){
    if(r.console.size()>uart){std::cerr<<"REVERSE_PCI_UART "<<quote(r.console.substr(uart))<<'\n';uart=r.console.size();}
    for(const char* s:{"init: InitFatalReboot:","init: Failed to mount required partitions early","init: Failed to mount /system:","queue_fs_event() failed: Invalid code: -1","Reboot failed -- System halted","reboot: Restarting system"})if(r.console.find(s)!=std::string::npos)failure=true;
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
  std::cerr<<"REVERSE_C28_CYCLES_COMPLETED "<<cycles<<'\n';
  if(failure){result.reason="guest_boot_failure";result.error="Android reported a fatal mount/reboot failure";save(guest,result);}
  std::cout<<boot_cycle_json(guest,result,GuestSha256::hash(guest.snapshot()),provenance)<<'\n';
  return result.reason=="instruction_budget"||result.reason=="host_wall_budget"?0:2;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
