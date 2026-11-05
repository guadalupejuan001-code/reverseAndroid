// Host-only continuation of the independently checked C07 state.
// CPU, devices and RAM remain owned by the existing XI/Ether MinimalGuest.
#include "guest_boot_cycle.hpp"
#include "guest_file_session.hpp"
#include "guest_android_init_proof.hpp"
#include <csignal>
#include <iostream>
using namespace reverse_pc::guest;
namespace {
constexpr const char* checkpoint_sha="845fb40da060147a23449ecfd4e7a3d8b11def8d66696093c6e6728798b82694";
constexpr const char* report_sha="5d426d1fec117b8ef955bbbccb0e944586f36c6fe8594d9c684575dd0067e574";
constexpr const char* disk_sha="28303ac7b2832cbde4e1c56551bf81816b55bc946cc75a444f2605316ed97e1c";
volatile std::sig_atomic_t stopping=0;
void stop(int){stopping=1;}
std::uint64_t number(const char* text,std::uint64_t maximum){
  const std::string s(text);
  require(!s.empty()&&s.find_first_not_of("0123456789")==std::string::npos,"Expected positive decimal budget");
  const auto value=std::stoull(s);require(value&&value<=maximum,"Budget outside supported range");return value;
}
std::string next_instruction(const MinimalGuest& guest){
  try{const auto pa=guest.physical_address(guest.registers().pc,false,true);
    return "{\"pc\":"+quote(hex(guest.registers().pc))+",\"physical_pc\":"+quote(hex(pa))+",\"opcode\":"+quote(hex(guest.read(pa,4)))+"}";
  }catch(const std::exception& e){return "{\"inspection_error\":"+quote(e.what())+"}";}
  catch(const MachineFault& e){return "{\"inspection_fault\":"+quote(hex(e.syndrome))+",\"address\":"+quote(hex(e.address))+"}";}
}
}
int main(int argc,char** argv){try{
  require(argc==8,"Expected C07.rvg C07.json C06-entry-proof.json disk.raw new-output-directory wall-ms instructions");
  BootCycleLimits limits;limits.wall_ms=number(argv[6],300000);limits.instructions=number(argv[7],1000000000);
  limits.chunk=250000;limits.checkpoint_ms=15000;limits.checkpoint_instructions=25000000;
  require(!std::filesystem::exists(argv[5]),"Continuation output must be a new directory");
  const auto input=guest_read_file(argv[1],MinimalGuest::max_adaptive_snapshot);
  require(GuestSha256::hash(input)==checkpoint_sha,"C07 checkpoint hash mismatch");
  require(GuestSha256::hash(guest_read_file(argv[2],65536))==report_sha,"C07 report hash mismatch");
  require(GuestSha256::hash(guest_read_file(argv[3],65536))==android_c06_report_sha256,"C06 ancestor proof hash mismatch");
  const auto disk=GuestBlockImage::open(argv[4]);
  require(disk->sha256()==disk_sha&&disk->size()==UINT64_C(8738832384),"Original Android disk identity mismatch");
  auto guest=MinimalGuest::from_snapshot(input,disk);
  require(guest.snapshot()==input,"C07 restore changed native bytes");
  require(guest.registers().instructions==UINT64_C(6350795356)&&guest.registers().pc==UINT64_C(0xffffffc0800dfa28),"C07 native boundary differs");
  require(guest.machine().enabled&&guest.machine().image_kind==2&&guest.machine().pci.enabled&&guest.machine().el==1&&!guest.registers().halted&&!guest.machine().waiting,"C07 native machine differs");
  require(guest.logical_ram()==UINT64_C(134217728),"Cannot replace the original RAM profile");
  const auto& clock=guest.machine().counter_latency;
  require(clock.enabled&&clock.instructions_per_tick==64&&clock.subtick==60,"C07 counter cadence or phase differs");
  require(GuestSha256::hash(Bytes(clock.seed.begin(),clock.seed.end()))==android_c06_counter_seed_sha256,"Cannot replace the persisted counter stream");
  const std::string provenance="{\"continuation_kind\":\"pinned-c07-descendant\",\"input_checkpoint_sha256\":"+quote(checkpoint_sha)+",\"input_report_sha256\":"+quote(report_sha)+",\"ancestor_entry_checkpoint_sha256\":"+quote(android_c06_checkpoint_sha256)+",\"ancestor_entry_report_sha256\":"+quote(android_c06_report_sha256)+",\"ancestor_init_verified\":true,\"timing_seed_origin\":\"checkpoint\",\"timing_seed_sha256\":"+quote(android_c06_counter_seed_sha256)+",\"counter_instructions_per_tick\":64,\"disk_sha256\":"+quote(disk_sha)+",\"disk_bytes\":8738832384,\"ram_profile_changed\":false,\"android_services_verified\":false,\"android_display_verified\":false,\"native_android_games_verified\":false}";
  BootCycleJournal journal(argv[5]);std::signal(SIGTERM,stop);std::signal(SIGINT,stop);
  auto last=std::chrono::steady_clock::now();std::size_t uart=0;
  const auto save=[&](const MinimalGuest& g,const BootCycleReport& r){journal.publish(g,r,provenance);std::cerr<<"REVERSE_C08_SAVED {\"instructions\":"<<g.registers().instructions<<",\"generation\":"<<journal.sequence()<<"}\n";};
  const auto progress=[&](const MinimalGuest& g,const BootCycleReport& r){
    if(r.console.size()>uart){std::cerr<<"REVERSE_C08_UART "<<quote(r.console.substr(uart))<<'\n';uart=r.console.size();}
    const auto now=std::chrono::steady_clock::now();
    if(now-last>=std::chrono::seconds(2)){std::cerr<<"REVERSE_C08_HEARTBEAT {\"instructions\":"<<g.registers().instructions<<",\"advanced\":"<<g.registers().instructions-r.initial_instructions<<",\"pc\":"<<quote(hex(g.registers().pc))<<",\"elapsed_ms\":"<<r.elapsed_ms<<"}\n";last=now;}
  };
  // No entry instruction is replayed or asserted here. The three immutable
  // input hashes establish the previously executed C06 -> C07 ancestry.
  // Per-run first/prior-entry flags remain false; ancestry is separate evidence.
  const auto result=run_boot_cycle(guest,limits,save,{},progress,[]{return stopping!=0;});
  std::cout<<"{\"journal_generations\":"<<journal.sequence()<<",\"next_instruction\":"<<next_instruction(guest)<<",\"cycle\":"<<boot_cycle_json(guest,result,GuestSha256::hash(guest.snapshot()),provenance)<<"}\n";
  return result.reason=="architectural_stop"||result.reason=="no_instruction_progress"||result.reason=="guest_kernel_panic"?2:0;
}catch(const std::exception& e){std::cerr<<"C08: "<<e.what()<<'\n';return 1;}}
