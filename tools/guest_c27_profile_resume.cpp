// Continue an independently audited clean budget boundary from the fixed C27 blank-data board.
// Invoke through android_c27_profile_resume.mjs, which validates the pinned source evidence.
#include "guest_budget_cycles.hpp"
#include "guest_file_session.hpp"
#include "guest_console_input.hpp"
#include <csignal>
#include <cstdlib>
#include <iostream>
using namespace reverse_pc::guest;
#ifdef REVERSE_PC_COMPACT_RESTORE_VIEW
static constexpr auto host_capacity=MinimalGuest::max_view_snapshot;
#else
static constexpr auto host_capacity=MinimalGuest::max_explicit_adaptive_snapshot;
#endif
static volatile std::sig_atomic_t stopping=0;
static void stop(int){stopping=1;}
static const std::string disk_sha="4c2eaf3fa7d81e9649072fc81f0651f4129bd9f41d4a9f9964cc627741a7b92e";
static const std::string root_sha="9db40b80214344583f855ccc777eb74963ece41ca5a1afab8de0737507f0e939";
static const std::string seed_sha="699e17ffab1ef3618567ee928b25cdc0b6c7feed3d520147e9111c76a751a6cb";
static std::uint64_t number(const char* text,std::uint64_t maximum,bool zero=false){const std::string s(text);require(!s.empty()&&s.find_first_not_of("0123456789")==std::string::npos,"Expected decimal budget");const auto n=std::stoull(s);require((n||zero)&&n<=maximum,"Budget out of range");return n;}
int main(int argc,char** argv){try{
  if(argc==2&&std::string(argv[1])=="--pins"){
    std::cout<<"{\"schema\":\"reverse-c27-profile-native-pins-v1\",\"disk_sha256\":"<<quote(disk_sha)<<",\"root_checkpoint_sha256\":"<<quote(root_sha)<<",\"counter_seed_sha256\":"<<quote(seed_sha)<<",\"logical_ram_bytes\":2147483648,\"counter_instructions_per_tick\":64,\"host_snapshot_transport_bytes\":"<<host_capacity<<"}\n";return 0;
  }
  require(argc==13,"Expected state report disk journal wall-ms instructions cycles state-sha report-sha initial-instructions pc counter-subtick; use android_c27_profile_resume.mjs");
  const auto console_input=guest_console_input_file(std::getenv("REVERSE_PC_UART_INPUT_FILE"));
  const std::string state_sha=argv[8],report_sha=argv[9],pc_text=argv[11];
  const auto valid_sha=[](const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;};
  require(valid_sha(state_sha)&&valid_sha(report_sha),"Invalid source SHA-256 pins");
  require(pc_text.size()>2&&pc_text.size()<=18&&pc_text.substr(0,2)=="0x"&&pc_text.find_first_not_of("0123456789abcdef",2)==std::string::npos,"Invalid source PC");
  const auto initial=number(argv[10],UINT64_MAX),pc=std::uint64_t(std::stoull(pc_text.substr(2),nullptr,16)),phase=number(argv[12],63,true);
  const auto cycle_limit=unsigned(number(argv[7],4));
  BootCycleLimits limits;limits.wall_ms=number(argv[5],900000);limits.instructions=number(argv[6],2000000000);
  // C39 host-only, opt-in scheduling. Does not change the guest CPU, machine
  // timer, quantum memory authority, snapshot format or instruction budget.
  // Larger transactions reduce host commit/copy overhead; checkpoints trade
  // recovery granularity for lower repeated snapshot serialization cost.
  const auto override_budget=[&](const char* key,std::uint64_t maximum,std::uint64_t baseline){
    const auto* value=std::getenv(key);return value?number(value,maximum):baseline;
  };
  limits.chunk=override_budget("REVERSE_PC_C39_CHUNK",1000000,50000);
  limits.checkpoint_ms=override_budget("REVERSE_PC_C39_CHECKPOINT_MS",180000,60000);
  limits.checkpoint_instructions=override_budget("REVERSE_PC_C39_CHECKPOINT_INSTRUCTIONS",300000000,100000000);
  std::cerr<<"REVERSE_C39_HOST_POLICY {\"chunk\":"<<limits.chunk<<",\"checkpoint_ms\":"<<limits.checkpoint_ms
    <<",\"checkpoint_instructions\":"<<limits.checkpoint_instructions<<"}\n";
  require(!std::filesystem::exists(argv[4]),"Output journal already exists");
  std::cerr<<"REVERSE_C27_PROFILE_HOST_PHASE {\"phase\":\"verify_original_checkpoint\"}\n";
  auto input=guest_read_file(argv[1],host_capacity);
  require(GuestSha256::hash(input)==state_sha,"Wrong pinned checkpoint");
  require(GuestSha256::hash(guest_read_file(argv[2],8*1024*1024))==report_sha,"Wrong pinned report");
  std::cerr<<"REVERSE_C27_PROFILE_HOST_PHASE {\"phase\":\"verify_full_original_disk\",\"bytes\":10888413184}\n";
  const auto disk=GuestBlockImage::open(argv[3]);require(disk->size()==UINT64_C(10888413184)&&disk->sha256()==disk_sha,"Wrong original disk");
  std::cerr<<"REVERSE_C27_PROFILE_HOST_PHASE {\"phase\":\"restore_original_machine\"}\n";
#ifdef REVERSE_PC_COMPACT_RESTORE_VIEW
  auto guest=MinimalGuest::from_snapshot_view(input,disk,host_capacity);
#else
  auto guest=MinimalGuest::from_snapshot(input,disk,host_capacity);
#endif
  std::size_t compared=0;
  guest.snapshot_to([&](const std::uint8_t* data,std::size_t length){
    require(compared<=input.size()&&length<=input.size()-compared&&std::equal(data,data+length,input.begin()+compared),"Restore modified the pinned guest state");compared+=length;
  });
  require(compared==input.size(),"Restore changed pinned guest state length");
  Bytes().swap(input); // Release transport; the original compact state owns all restored data.
  require(guest.registers().instructions==initial&&guest.registers().pc==pc,"Wrong pinned instruction boundary");
  require(guest.machine().enabled&&guest.machine().image_kind==2&&guest.machine().pci.enabled&&guest.machine().el<=1&&!guest.registers().halted&&!guest.machine().waiting,"Wrong fixed board state");
  require(guest.logical_ram()==2147483648,"RAM profile changed");
  const auto& clock=guest.machine().counter_latency;
  require(clock.enabled&&clock.instructions_per_tick==64&&clock.subtick==phase,"Counter cadence changed");
  require(GuestSha256::hash(Bytes(clock.seed.begin(),clock.seed.end()))==seed_sha,"Counter seed changed");
  require(guest.host_snapshot_transport_capacity()==host_capacity,"Explicit host bound lost");
  // Only after the original state has round-tripped and every source pin passed.
  // Empty or absent input preserves the original execution and provenance bytes.
  const auto uart_provenance=guest_console_input_event(guest,console_input);
  const std::string provenance="{\"experiment\":\"c27-profile-budget-continuation\",\"root_checkpoint_sha256\":"+quote(root_sha)+",\"input_checkpoint_sha256\":"+quote(state_sha)+",\"input_report_sha256\":"+quote(report_sha)+",\"snapshot_round_trip_verified\":true,\"source_profile_verified\":true,\"host_snapshot_transport_bytes\":"+std::to_string(host_capacity)+",\"host_transport_policy_serialized\":false,\"cycle_limit\":"+std::to_string(cycle_limit)+",\"timing_seed_origin\":\"checkpoint\",\"timing_seed_sha256\":"+quote(seed_sha)+",\"disk_sha256\":"+quote(disk_sha)+",\"ram_profile_changed\":false,\"original_state_modified\":false"+uart_provenance+"}";
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
  unsigned cycles=0;auto result=run_boot_budget_cycles(guest,limits,cycle_limit,save,progress,[&]{return stopping!=0||failure;},&cycles);
  std::cerr<<"REVERSE_C27_PROFILE_CYCLES_COMPLETED "<<cycles<<'\n';
  if(failure){result.reason="guest_boot_failure";result.error="Android reported a fatal mount/reboot failure";save(guest,result);}
  std::cout<<boot_cycle_json(guest,result,guest.snapshot_sha256(),provenance)<<'\n';
  return result.reason=="instruction_budget"||result.reason=="host_wall_budget"?0:2;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

