// Resume the real C03 PCI checkpoint; never rebuild a fresh VM or change RAM.
#include "guest_boot_cycle.hpp"
#include "guest_file_session.hpp"
#include "guest_android_init_proof.hpp"
#include <csignal>
#include <sys/random.h>
#include <iostream>
using namespace reverse_pc::guest;
static volatile std::sig_atomic_t stopping=0;
static void stop_cycle(int){stopping=1;}
static const std::string kernel_sha="f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8";
static const std::string ramdisk_sha="c9daaaf44bfc5ea2701a355a455a136fe4a98b91ad1e2a4965641a0c47fbc1da";
static const std::string disk_sha="28303ac7b2832cbde4e1c56551bf81816b55bc946cc75a444f2605316ed97e1c";
static const std::string init_code=android_original_init_code;
static void validate_input(const Bytes& state,const std::string& expected){
  require(expected.size()==64&&expected.find_first_not_of("0123456789abcdef")==std::string::npos,"Expected exact input checkpoint SHA-256");
  require(GuestSha256::hash(state)==expected,"Input checkpoint SHA-256 mismatch");
  require(state.size()>=360&&(state[3]=='2'||state[3]=='3'),"Expected RVG2/RVG3");
  const auto at=8+(state[3]=='3'?44:43)*8;const auto len=get(state,8+39*8,8);
  require(at<=state.size()&&len<=state.size()-at&&len>=128,"Invalid immutable image extent");
  require(std::string(state.begin()+at,state.begin()+at+4)=="RVI3","Cycle requires the saved PCI board");
  const auto kn=get(state,at+8,8),dt=get(state,at+16,8),rd=get(state,at+24,8),args=get(state,at+32,8),devices=get(state,at+40,8);
  require(kn<=MinimalGuest::max_linux_kernel&&dt<=65536&&rd<=MinimalGuest::max_linux_initrd&&args<=1024&&devices==80&&48+kn+dt+rd+args+devices==len,"Invalid PCI image payload extents");
  require(GuestSha256::hash(Bytes(state.begin()+at+48,state.begin()+at+48+kn))==kernel_sha,"Different Android GKI");
  require(GuestSha256::hash(Bytes(state.begin()+at+48+kn+dt,state.begin()+at+48+kn+dt+rd))==ramdisk_sha,"Different normal ramdisk");
  const auto arg_at=at+48+kn+dt+rd;
  const std::string bootargs(state.begin()+arg_at,state.begin()+arg_at+args);
  require(bootargs==guest_android_cuttlefish_normal_no_scx_bootargs()+" androidboot.boot_devices=30000000.pcie","Different Android PCI boot profile");
  const auto device=at+len-80;
  require(get(state,device,8)==1&&get(state,device+8,8)==UINT64_C(8738832384)&&std::string(state.begin()+device+16,state.begin()+device+80)==disk_sha,"Different disk identity in checkpoint");
}
static std::string next_instruction(const MinimalGuest& guest){
  try{const auto pa=guest.physical_address(guest.registers().pc,false,true);
    return "{\"pc\":"+quote(hex(guest.registers().pc))+",\"physical_pc\":"+quote(hex(pa))+",\"opcode\":"+quote(hex(guest.read(pa,4)))+"}";
  }catch(const std::exception& e){return "{\"inspection_error\":"+quote(e.what())+"}";}
  catch(const MachineFault& e){return "{\"inspection_fault\":"+quote(hex(e.syndrome))+",\"address\":"+quote(hex(e.address))+"}";}
}
int main(int argc,char** argv){try{
  require(argc>=6&&argc<=13,"Expected input.rvg disk.raw new-output-directory milliseconds input-sha256 [--chunk=N] [--instructions=N] [--counter-latency=host-entropy] [--stop-after-init] [--counter-instructions-per-tick=N] [--continue-verified-init=report.json] [--stop-before-user-undefined]");
  BootCycleLimits limits;const std::string wall(argv[4]);
  require(!wall.empty()&&wall.find_first_not_of("0123456789")==std::string::npos,"Wall budget must be decimal milliseconds");
  limits.wall_ms=std::stoull(wall);require(limits.wall_ms&&limits.wall_ms<=1500000,"Wall budget must be 1..1500000 ms");
  bool seen_chunk=false,seen_steps=false,seed_clock=false;
  std::uint64_t counter_rate=0;const std::string rate_prefix="--counter-instructions-per-tick=";
  std::string prior_report_path;const std::string proof_prefix="--continue-verified-init=";
  for(int i=6;i<argc;i++){
    const std::string arg(argv[i]);if(arg=="--counter-latency=host-entropy"){require(!seed_clock,"Duplicate timing mode");seed_clock=true;continue;}
    if(arg=="--stop-before-user-undefined"){require(!limits.stop_before_user_undefined,"Duplicate undefined diagnostic stop");limits.stop_before_user_undefined=true;continue;}
    if(arg.rfind(proof_prefix,0)==0){require(prior_report_path.empty(),"Duplicate verified init proof");prior_report_path=arg.substr(proof_prefix.size());require(!prior_report_path.empty(),"Missing verified init proof path");continue;}
    if(arg=="--stop-after-init"){require(!limits.stop_after_first_user_instruction,"Duplicate first-init stop");limits.stop_after_first_user_instruction=true;continue;}
    if(arg.rfind(rate_prefix,0)==0){
      require(!counter_rate,"Duplicate counter cadence");const auto n=arg.substr(rate_prefix.size());
      require(!n.empty()&&n.find_first_not_of("0123456789")==std::string::npos,"Counter cadence must be decimal");
      counter_rate=std::stoull(n);require(counter_rate>=2&&counter_rate<=4096,"Counter cadence must be 2..4096 instructions per tick");continue;
    }
    const bool chunk=arg.rfind("--chunk=",0)==0,steps=arg.rfind("--instructions=",0)==0;
    require(chunk||steps,"Unknown cycle option");bool& seen=chunk?seen_chunk:seen_steps;require(!seen,"Duplicate cycle option");seen=true;
    const auto numeric=arg.substr(chunk?8:15);require(!numeric.empty()&&numeric.find_first_not_of("0123456789")==std::string::npos,"Cycle option must be a positive decimal integer");
    const auto value=std::stoull(numeric);require(value&&value<=(chunk?1000000:UINT64_C(8000000000)),"Cycle option exceeds its bound");
    if(chunk)limits.chunk=value;else limits.instructions=value;
  }
  require(prior_report_path.empty()||(!limits.stop_after_first_user_instruction&&!seed_clock),"A proven continuation cannot repeat init or reseed its clock");
  require(!std::filesystem::exists(argv[3]),"Cycle output must be a new directory");
  const auto input=guest_read_file(argv[1],MinimalGuest::max_adaptive_snapshot);validate_input(input,argv[5]);
  AndroidResumeSource resume_source=AndroidResumeSource::c06;
  if(!prior_report_path.empty())resume_source=android_check_resume_artifacts(input,guest_read_file(prior_report_path,65536));
  const auto disk=GuestBlockImage::open(argv[2]);require(disk->sha256()==disk_sha&&disk->size()==UINT64_C(8738832384),"Wrong external disk");
  auto guest=MinimalGuest::from_snapshot(input,disk);
  require(guest.machine().enabled&&guest.machine().image_kind==2&&guest.machine().pci.enabled,"Expected original PCI Linux machine");
  require(!prior_report_path.empty()||guest.machine().el||guest.registers().pc==0x2a4000,"Use the proven entry checkpoint to verify the first Android instruction");
  const auto initial=guest.snapshot();require(initial==input,"Restoring the checkpoint changed its bytes");
  BootCycleEntryProof prior_entry;
  if(!prior_report_path.empty())prior_entry=android_resume_entry_proof(guest,resume_source);
  std::string seed_origin=guest.machine().counter_latency.enabled?"checkpoint":"disabled";
  if(seed_clock&&!guest.machine().counter_latency.enabled){
    std::array<std::uint8_t,32> seed{};std::size_t at=0;
    while(at<seed.size()){const auto n=::getrandom(seed.data()+at,seed.size()-at,0);if(n<0&&errno==EINTR)continue;require(n>0,"Host OS entropy is unavailable");at+=std::size_t(n);}
    guest.enable_counter_latency(seed);seed_origin="host_os_getrandom";
  }
  std::string rate_origin=guest.machine().counter_latency.instructions_per_tick>1?"checkpoint":"legacy";
  if(counter_rate){
    require(guest.machine().counter_latency.enabled,"Counter cadence requires the persisted seeded clock");
    if(guest.machine().counter_latency.instructions_per_tick==1){guest.configure_counter_rate(counter_rate);rate_origin="configured";}
    else require(guest.machine().counter_latency.instructions_per_tick==counter_rate,"Cannot replace a persisted counter cadence");
  }
  const auto& timing=guest.machine().counter_latency;
  const std::string seed_id=timing.enabled?GuestSha256::hash(Bytes(timing.seed.begin(),timing.seed.end())):std::string{};
  const std::string source="{\"input_checkpoint_sha256\":"+quote(argv[5])+",\"kernel_sha256\":"+quote(kernel_sha)+",\"ramdisk_sha256\":"+quote(ramdisk_sha)+",\"disk_sha256\":"+quote(disk_sha)+",\"disk_bytes\":8738832384,\"counter_latency\":"+(timing.enabled?"true":"false")+",\"timing_seed_origin\":"+quote(seed_origin)+",\"timing_seed_sha256\":"+quote(seed_id)+",\"counter_instructions_per_tick\":"+std::to_string(timing.instructions_per_tick)+",\"counter_rate_origin\":"+quote(rate_origin)+",\"run_chunk\":"+std::to_string(limits.chunk)+",\"stop_after_init\":"+(limits.stop_after_first_user_instruction?"true":"false")+",\"android_services_verified\":false,\"android_display_verified\":false,\"native_android_games_verified\":false}";
  BootCycleJournal journal(argv[3]);std::signal(SIGTERM,stop_cycle);std::signal(SIGINT,stop_cycle);
  auto last=std::chrono::steady_clock::now();std::size_t uart=0;
  const auto save=[&](const MinimalGuest& g,const BootCycleReport& r){journal.publish(g,r,source);std::cerr<<"REVERSE_PCI_SAVED {\"instructions\":"<<g.registers().instructions<<",\"generation\":"<<journal.sequence()<<"}\n";};
  const auto entry=[](MinimalGuest& g){
    require(g.registers().pc==0x2a4000,"EL0 is not the pinned Android init entry");
    std::ostringstream code;code<<std::hex<<std::setfill('0');for(unsigned i=0;i<64;i++)code<<std::setw(2)<<unsigned(g.read8(g.physical_address(g.registers().pc+i,false,true)));
    require(code.str()==init_code,"Android init entry bytes differ");
    const auto n=g.registers().instructions;g.run(1);
    return !g.machine().el&&g.registers().pc==0x2a4004&&g.registers().instructions==n+1;
  };
  const auto progress=[&](const MinimalGuest& g,const BootCycleReport& r){
    if(r.console.size()>uart){std::cerr<<"REVERSE_PCI_UART "<<quote(r.console.substr(uart))<<'\n';uart=r.console.size();}
    const auto now=std::chrono::steady_clock::now();if(now-last>=std::chrono::seconds(2)){
      std::cerr<<"REVERSE_PCI_CYCLE_HEARTBEAT {\"instructions\":"<<g.registers().instructions<<",\"advanced\":"<<g.registers().instructions-r.initial_instructions<<",\"pc\":"<<quote(hex(g.registers().pc))<<",\"el\":"<<g.machine().el<<",\"elapsed_ms\":"<<r.elapsed_ms<<",\"original_android_init_first_instruction_verified\":"<<(r.first_user_instruction_verified?"true":"false")<<"}\n";last=now;}
  };
  const auto result=run_boot_cycle(guest,limits,save,entry,progress,[]{return stopping!=0;},prior_report_path.empty()?nullptr:&prior_entry);
  std::cout<<"{\"journal_generations\":"<<journal.sequence()<<",\"next_instruction\":"<<next_instruction(guest)<<",\"cycle\":"<<boot_cycle_json(guest,result,GuestSha256::hash(guest.snapshot()),source)<<"}\n";
  return result.reason=="architectural_stop"||result.reason=="unverified_userspace_entry"||result.reason=="no_instruction_progress"||result.reason=="guest_kernel_panic"?2:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
