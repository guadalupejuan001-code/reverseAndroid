// Fresh boot experiment using ONLY the immutable original media from C12.
// Never restores C12's terminal CPU/RAM/device state or inherits its init proof.
#include "guest_boot_cycle.hpp"
#include "guest_file_session.hpp"
#include "guest_android_init_proof.hpp"
#include <csignal>
#include <sys/random.h>
#include <iostream>
using namespace reverse_pc::guest;
static volatile std::sig_atomic_t stopping=0;
static void request_stop(int){stopping=1;}
static const std::string c12_sha="88f890e0237a3723cadf308ee5fcd23778d1f101e36c87e40e6ca13eb8aa0117";
static const std::string kernel_sha="f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8";
static const std::string ramdisk_sha="c9daaaf44bfc5ea2701a355a455a136fe4a98b91ad1e2a4965641a0c47fbc1da";
static const std::string disk_sha="28303ac7b2832cbde4e1c56551bf81816b55bc946cc75a444f2605316ed97e1c";
static const std::string avb_args="androidboot.vbmeta.size=22912 androidboot.vbmeta.hash_alg=sha256 androidboot.vbmeta.digest=92f4ce73ee87fb244fee21aee3de3ddef2fd52d7631e12fc850be8348f3c9ed3";
static std::pair<Bytes,Bytes> immutable_media(const Bytes& state){
  require(GuestSha256::hash(state)==c12_sha,"C12 evidence hash mismatch");
  require(state.size()>=360&&(state[3]=='2'||state[3]=='3'),"Not RVG2/RVG3");
  const auto at=8+(state[3]=='3'?44:43)*8;const auto len=get(state,8+39*8,8);
  require(at<=state.size()&&len<=state.size()-at&&len>=128,"Invalid immutable image extent");
  require(std::string(state.begin()+at,state.begin()+at+4)=="RVI3","Expected original PCI media");
  const auto kn=get(state,at+8,8),dt=get(state,at+16,8),rd=get(state,at+24,8),an=get(state,at+32,8),dn=get(state,at+40,8);
  require(kn<=MinimalGuest::max_linux_kernel&&dt<=65536&&rd<=MinimalGuest::max_linux_initrd&&an<=1024&&dn==80&&48+kn+dt+rd+an+dn==len,"Invalid original media bounds");
  Bytes kernel(state.begin()+at+48,state.begin()+at+48+kn),ramdisk(state.begin()+at+48+kn+dt,state.begin()+at+48+kn+dt+rd);
  require(GuestSha256::hash(kernel)==kernel_sha&&GuestSha256::hash(ramdisk)==ramdisk_sha,"Original kernel or ramdisk changed");
  const auto aa=at+48+kn+dt+rd;const std::string old_args(state.begin()+aa,state.begin()+aa+an);
  require(old_args==guest_android_cuttlefish_normal_no_scx_bootargs()+" androidboot.boot_devices=30000000.pcie","Unexpected source boot profile");
  const auto dev=at+len-80;require(get(state,dev,8)==1&&get(state,dev+8,8)==UINT64_C(8738832384)&&std::string(state.begin()+dev+16,state.begin()+dev+80)==disk_sha,"Different source disk identity");
  return {std::move(kernel),std::move(ramdisk)};
}
int main(int argc,char** argv){try{
  require(argc==6,"Expected C12-evidence.rvg original-disk.raw new-journal wall-ms verified-avb-bootargs.txt");
  const std::string wall(argv[4]);require(!wall.empty()&&wall.find_first_not_of("0123456789")==std::string::npos,"Wall budget must be decimal");
  BootCycleLimits limits;limits.wall_ms=std::stoull(wall);require(limits.wall_ms&&limits.wall_ms<=1200000,"Wall bound 1..1200000 ms");
  limits.instructions=UINT64_C(8000000000);limits.chunk=50000;
  require(!std::filesystem::exists(argv[3]),"Fresh experiment needs a new journal");
  const auto supplied=guest_read_file(argv[5],1024);require(std::string(supplied.begin(),supplied.end())==avb_args,"Unverified AVB handoff");
  const auto media=immutable_media(guest_read_file(argv[1],MinimalGuest::max_adaptive_snapshot));
  const auto disk=GuestBlockImage::open(argv[2]);require(disk->size()==UINT64_C(8738832384)&&disk->sha256()==disk_sha,"Wrong original disk");
  const auto args=guest_android_cuttlefish_normal_no_scx_bootargs()+" androidboot.boot_devices=30000000.pcie "+avb_args;
  MinimalGuest guest(128*1024*1024,0);guest.load_linux_image(media.first,media.second,args,disk,true);guest.enable_adaptive_memory();
  std::array<std::uint8_t,32> seed{};std::size_t pos=0;while(pos<seed.size()){const auto n=::getrandom(seed.data()+pos,seed.size()-pos,0);if(n<0&&errno==EINTR)continue;require(n>0,"Host OS entropy unavailable");pos+=std::size_t(n);}
  guest.enable_counter_latency(seed);guest.configure_counter_rate(64);
  require(guest.registers().instructions==0&&guest.machine().el==1,"Fresh boot was not initialized");
  const auto source="{\"experiment\":\"c13-fresh-avb-handoff\",\"terminal_c12_restored\":false,\"immutable_media_source_sha256\":"+quote(c12_sha)+",\"kernel_sha256\":"+quote(kernel_sha)+",\"ramdisk_sha256\":"+quote(ramdisk_sha)+",\"disk_sha256\":"+quote(disk_sha)+",\"disk_bytes\":8738832384,\"bootargs\":"+quote(args)+",\"ram_bytes\":134217728,\"counter_instructions_per_tick\":64,\"timing_seed_origin\":\"host_os_getrandom_new_experiment\",\"timing_seed_sha256\":"+quote(GuestSha256::hash(Bytes(seed.begin(),seed.end())))+",\"prior_init_proof_reused\":false,\"android_display_verified\":false,\"native_android_games_verified\":false}";
  BootCycleJournal journal(argv[3]);std::signal(SIGTERM,request_stop);std::signal(SIGINT,request_stop);
  std::size_t uart=0;bool failure=false;auto last=std::chrono::steady_clock::now();
  const auto save=[&](const MinimalGuest& g,const BootCycleReport& r){journal.publish(g,r,source);std::cerr<<"REVERSE_PCI_SAVED {\"instructions\":"<<g.registers().instructions<<",\"generation\":"<<journal.sequence()<<"}\n";};
  const auto entry=[](MinimalGuest& g){require(g.registers().pc==0x2a4000,"Not original init entry");std::ostringstream code;code<<std::hex<<std::setfill('0');for(unsigned i=0;i<64;i++)code<<std::setw(2)<<unsigned(g.read8(g.physical_address(g.registers().pc+i,false,true)));require(code.str()==android_original_init_code,"Original init bytes changed");const auto n=g.registers().instructions;g.run(1);return !g.machine().el&&g.registers().pc==0x2a4004&&g.registers().instructions==n+1;};
  const auto progress=[&](const MinimalGuest& g,const BootCycleReport& r){
    if(r.console.size()>uart){std::cerr<<"REVERSE_PCI_UART "<<quote(r.console.substr(uart))<<'\n';uart=r.console.size();}
    for(const char* text:{"init: InitFatalReboot:","init: Failed to mount required partitions early","init: Failed to mount /system:","Reboot failed -- System halted","reboot: Restarting system"})if(r.console.find(text)!=std::string::npos)failure=true;
    const auto now=std::chrono::steady_clock::now();if(now-last>=std::chrono::seconds(2)){std::cerr<<"REVERSE_PCI_CYCLE_HEARTBEAT {\"instructions\":"<<g.registers().instructions<<",\"pc\":"<<quote(hex(g.registers().pc))<<",\"el\":"<<g.machine().el<<",\"elapsed_ms\":"<<r.elapsed_ms<<"}\n";last=now;}
  };
  auto result=run_boot_cycle(guest,limits,save,entry,progress,[&]{return stopping!=0||failure;});
  if(failure){result.reason="guest_boot_failure";result.error="Android reported a fatal boot/mount failure";save(guest,result);}
  std::cout<<boot_cycle_json(guest,result,GuestSha256::hash(guest.snapshot()),source)<<'\n';
  return result.reason=="instruction_budget"||result.reason=="host_wall_budget"?0:2;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
