// Bounded real GKI/Android boot experiment on the original XI/Ether machine.
// Keeps one immutable disk attachment across chunks; never rehashes it per step.
#include "minimal_guest.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace reverse_pc::guest;
static Bytes read_file(const std::string& path,std::size_t limit){std::ifstream f(path,std::ios::binary|std::ios::ate);require(bool(f),"Input is unavailable");const auto n=f.tellg();require(n>0&&std::uint64_t(n)<=limit,"Input exceeds bounded transport");Bytes b(static_cast<std::size_t>(n));f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),std::streamsize(b.size()));require(bool(f),"Incomplete boot input");return b;}
int main(int argc,char** argv){try{
 require(argc==7,"Expected kernel initrd disk output.rvg milliseconds init-entry-code64-hex");
 const auto wall=std::stoull(argv[5]);require(wall>=1&&wall<=1500000,"Boot experiment wall bound is 1..1500000 ms");
 const std::string expected=argv[6];require(expected.size()==128&&expected.find_first_not_of("0123456789abcdef")==std::string::npos,"Expected pinned init entry code");
 const auto disk=GuestBlockImage::open(argv[3]);const auto kernel=read_file(argv[1],MinimalGuest::max_linux_kernel),initrd=read_file(argv[2],MinimalGuest::max_linux_initrd);
 require(GuestSha256::hash(kernel)=="f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8","Wrong genuine Android GKI");
 require(GuestSha256::hash(initrd)=="c9daaaf44bfc5ea2701a355a455a136fe4a98b91ad1e2a4965641a0c47fbc1da","Wrong normal Android ramdisk");
 // AOSP init creates global by-name links only for a declared boot device.
 // Our root platform node is pcie@30000000 -> 30000000.pcie in sysfs.
 const auto args=guest_android_cuttlefish_normal_no_scx_bootargs()+" androidboot.boot_devices=30000000.pcie";
 MinimalGuest guest(128*1024*1024,0);guest.load_linux_image(kernel,initrd,args,disk,true);
 const auto start=std::chrono::steady_clock::now();auto last=start;std::string console,reason="host_wall_budget",error;bool init=false;
 const auto elapsed=[&](){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();};
 while(elapsed()<std::int64_t(wall)&&guest.registers().instructions<UINT64_C(5000000000)&&!guest.registers().halted){
   RunResult result;
   try{result=guest.run(50000,!init);}
   catch(const std::exception& x){
     const auto before=guest;auto good=guest;std::uint64_t low=0,high=50000;std::string prefix;
     while(low+1<high){const auto mid=(low+high)/2;auto trial=before;try{const auto r=trial.run(mid,!init);good=std::move(trial);prefix=r.output;low=mid;}catch(const std::exception&){high=mid;}}
     guest=std::move(good);console+=prefix;reason="architectural_stop";error=x.what();break;
   }
   console+=result.output;if(!result.output.empty())std::cerr<<"REVERSE_PCI_GUEST_UART "<<quote(result.output)<<'\n';require(console.size()<=4*1024*1024,"UART evidence exceeds 4 MiB");
   if(!init&&!guest.machine().el){
     std::ostringstream code;code<<std::hex<<std::setfill('0');for(unsigned i=0;i<64;i++)code<<std::setw(2)<<unsigned(guest.read8(guest.physical_address(guest.registers().pc+i,false,true)));
     require(guest.registers().pc==0x2a4000&&code.str()==expected,"EL0 is not the pinned Android /init entry");
     const auto count=guest.registers().instructions;const auto first=guest.run(1);console+=first.output;
     require(guest.machine().el==0&&guest.registers().pc==0x2a4004&&guest.registers().instructions==count+1,"Real Android init first opcode did not execute");init=true;
   }
   if(result.waiting){reason="waiting_for_device_or_interrupt";break;}
   const auto now=std::chrono::steady_clock::now();if(now-last>=std::chrono::seconds(2)){std::cerr<<"REVERSE_PCI_BOOT_HEARTBEAT {\"instructions\":"<<guest.registers().instructions<<",\"pc\":"<<quote(hex(guest.registers().pc))<<",\"own_android_init\":"<<(init?"true":"false")<<",\"elapsed_ms\":"<<elapsed()<<"}\n";last=now;}
 }
 const auto state=guest.snapshot();std::ofstream out(argv[4],std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(state.data()),std::streamsize(state.size()));out.close();require(bool(out),"Cannot save PCI boot checkpoint");
 std::cout<<"{\"guest\":"<<guest.statistics()<<",\"stop_reason\":"<<quote(reason)<<",\"error\":"<<quote(error)<<",\"console\":"<<quote(console)<<",\"checkpoint_sha256\":"<<quote(GuestSha256::hash(state))<<",\"disk_sha256\":"<<quote(disk->sha256())<<",\"disk_bytes\":"<<disk->size()<<",\"host_elapsed_ms\":"<<elapsed()<<",\"original_android_init_first_instruction_verified\":"<<(init?"true":"false")<<",\"android_services_verified\":false,\"android_display_verified\":false,\"native_android_games_verified\":false}\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
