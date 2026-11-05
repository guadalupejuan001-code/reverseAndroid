// Bounded execution of the explicit lights-port boot epoch with the original CPU.
#include "guest_boot_cycle.hpp"
#include "guest_file_session.hpp"
#include "guest_android_init_proof.hpp"
#include "guest_console_input.hpp"
#include <csignal>
#include <iostream>
using namespace reverse_pc::guest;
static volatile std::sig_atomic_t stopping=0;
static void stop(int){stopping=1;}
int main(int argc,char** argv){try{
  require(argc==11||argc==13,"Expected source report disk journal wall-ms instructions state-sha report-sha epoch-root-sha prior-epoch-init-verified [explicit-input-file input-sha]");
  require(!std::filesystem::exists(argv[4]),"Journal already exists");
  const auto number=[](const char* p,std::uint64_t max){const std::string s(p);require(!s.empty()&&s.find_first_not_of("0123456789")==std::string::npos,"Expected decimal budget");const auto n=std::stoull(s);require(n&&n<=max,"Budget exceeds bound");return n;};
  const bool prior_init=std::string(argv[10])=="true";require(prior_init||std::string(argv[10])=="false","Invalid epoch init proof flag");
  const auto valid_sha=[](const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;};
  for(unsigned i:{7u,8u,9u})require(valid_sha(argv[i]),"Invalid explicit SHA-256 pin");
  BootCycleLimits limits;limits.wall_ms=number(argv[5],900000);limits.instructions=number(argv[6],2000000000);limits.chunk=1000000;limits.checkpoint_ms=180000;limits.checkpoint_instructions=300000000;
  std::cerr<<"EPOCH_PHASE verify_source_and_original_disk\n";
  auto input=guest_read_file(argv[1],MinimalGuest::max_view_snapshot);require(GuestDiskSha256::hash(input)==argv[7],"Wrong pinned source");
  require(GuestDiskSha256::hash(guest_read_file(argv[2],8*1024*1024))==argv[8],"Wrong bound report");
  const auto disk=GuestBlockImage::open(argv[3]);require(disk->size()==10888413184&&disk->sha256()=="4c2eaf3fa7d81e9649072fc81f0651f4129bd9f41d4a9f9964cc627741a7b92e","Original disk changed");
  auto guest=MinimalGuest::from_snapshot_view(input,disk,MinimalGuest::max_view_snapshot);
  std::size_t compared=0;guest.snapshot_to([&](const std::uint8_t* p,std::size_t n){require(compared+n<=input.size()&&std::equal(p,p+n,input.begin()+compared),"Source roundtrip changed bytes");compared+=n;});require(compared==input.size(),"Source roundtrip extent");Bytes().swap(input);
  require(guest.logical_ram()==2147483648&&guest.machine().enabled&&guest.machine().pci.enabled&&!guest.registers().halted&&!guest.machine().waiting,"Wrong epoch board state");
  const auto& clock=guest.machine().counter_latency;require(clock.enabled&&clock.instructions_per_tick==64&&GuestSha256::hash(Bytes(clock.seed.begin(),clock.seed.end()))=="699e17ffab1ef3618567ee928b25cdc0b6c7feed3d520147e9111c76a751a6cb","Original clock profile changed");
  // No ambient environment variable can add an input. The optional event is
  // applied only after exact source/report, original roundtrip and clock checks.
  const auto uart_input=argc==13?guest_console_input_file(argv[11]):Bytes{};
  if(argc==13){require(valid_sha(argv[12])&&!uart_input.empty(),"Missing authenticated input bytes");require(GuestSha256::hash(uart_input)==argv[12],"Explicit input SHA-256 mismatch");require(guest.machine().receive.empty(),"Cannot append a new event while prior UART bytes remain pending");}
  const auto uart_provenance=guest_console_input_event(guest,uart_input);
  const std::string provenance="{\"experiment\":\"c76-lights-port-epoch\",\"epoch_root_checkpoint_sha256\":"+quote(argv[9])+",\"input_checkpoint_sha256\":"+quote(argv[7])+",\"input_report_sha256\":"+quote(argv[8])+",\"snapshot_round_trip_verified\":true,\"source_epoch_init_verified\":"+(prior_init?"true":"false")+",\"host_snapshot_transport_bytes\":2147483648,\"host_transport_policy_serialized\":false,\"source_cpu_modified_before_resume\":false,\"new_uart_input_bytes\":"+std::to_string(uart_input.size())+",\"live_android_properties_written\":false,\"timing_seed_origin\":\"checkpoint\",\"timing_seed_sha256\":\"699e17ffab1ef3618567ee928b25cdc0b6c7feed3d520147e9111c76a751a6cb\",\"disk_sha256\":"+quote(disk->sha256())+uart_provenance+"}";
  const auto entry=[](MinimalGuest& g){require(g.registers().pc==0x2a4000,"Not original init entry");std::ostringstream code;code<<std::hex<<std::setfill('0');for(unsigned i=0;i<64;++i)code<<std::setw(2)<<unsigned(g.read8(g.physical_address(g.registers().pc+i,false,true)));require(code.str()==android_original_init_code,"Original init code changed");const auto n=g.registers().instructions;g.run(1);return !g.machine().el&&g.registers().pc==0x2a4004&&g.registers().instructions==n+1;};
  std::signal(SIGTERM,stop);std::signal(SIGINT,stop);BootCycleJournal journal(argv[4]);
  bool failure=false;std::size_t uart=0;auto last=std::chrono::steady_clock::now();
  const auto save=[&](const MinimalGuest& g,const BootCycleReport& r){journal.publish(g,r,provenance);std::cerr<<"REVERSE_PCI_SAVED {\"instructions\":"<<g.registers().instructions<<",\"generation\":"<<journal.sequence()<<"}\n";};
  const auto progress=[&](const MinimalGuest& g,const BootCycleReport& r){
    if(r.console.size()>uart){std::cerr<<"REVERSE_PCI_UART "<<quote(r.console.substr(uart))<<'\n';uart=r.console.size();}
    for(const char* s:{"init: InitFatalReboot:","init: Failed to mount required partitions early","init: Failed to mount /system:","queue_fs_event() failed: Invalid code: -1","Reboot failed -- System halted","reboot: Restarting system"})if(r.console.find(s)!=std::string::npos)failure=true;
    const auto now=std::chrono::steady_clock::now();if(now-last>=std::chrono::seconds(2)){std::cerr<<"REVERSE_PCI_CYCLE_HEARTBEAT {\"instructions\":"<<g.registers().instructions<<",\"pc\":"<<quote(hex(g.registers().pc))<<",\"elapsed_ms\":"<<r.elapsed_ms<<"}\n";last=now;}
  };
  auto result=run_boot_cycle(guest,limits,save,prior_init?UserEntry{}:UserEntry(entry),progress,[&]{return stopping||failure;});
  if(failure){result.reason="guest_boot_failure";result.error="Android reported a fatal boot/mount failure";save(guest,result);}
  std::cout<<boot_cycle_json(guest,result,guest.snapshot_sha256(),provenance)<<'\n';
  return result.reason=="instruction_budget"||result.reason=="host_wall_budget"?0:2;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
