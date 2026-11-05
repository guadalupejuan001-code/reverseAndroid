// Explicit fresh machine: preserve original signed media, add writable userdata,
// and declare a 2 GiB RAM profile. Never restore a previous CPU or inherit init proof.
#include "guest_boot_cycle.hpp"
#include "guest_file_session.hpp"
#include "guest_android_init_proof.hpp"
#include <csignal>
#include <sys/random.h>
#include <iostream>
using namespace reverse_pc::guest;
static volatile std::sig_atomic_t stopping=0;
static void stop(int){stopping=1;}
static const std::string c18_sha="1922206a01c7bf9392b20afccc195ae7a885954fec85ef662c791b9b17cd024c";
static const std::string c19_sha="c74b0d4f1bd874901c6ee9b1464d419fde1ed49af58ed4b13cfc803506e95f1d";
static const std::string kernel_sha="f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8";
static const std::string original_ramdisk_sha="c9daaaf44bfc5ea2701a355a455a136fe4a98b91ad1e2a4965641a0c47fbc1da";
static const std::string original_disk_sha="28303ac7b2832cbde4e1c56551bf81816b55bc946cc75a444f2605316ed97e1c";
static const std::string avb="androidboot.vbmeta.size=22912 androidboot.vbmeta.hash_alg=sha256 androidboot.vbmeta.digest=92f4ce73ee87fb244fee21aee3de3ddef2fd52d7631e12fc850be8348f3c9ed3";
static std::uint64_t decimal(const char* text,std::uint64_t maximum){const std::string s(text);require(!s.empty()&&s.find_first_not_of("0123456789")==std::string::npos,"Expected decimal budget");const auto n=std::stoull(s);require(n&&n<=maximum,"Budget exceeds bound");return n;}
int main(int argc,char** argv){try{
 require(argc==9,"Expected original-media.rvg new-disk.raw expected-disk-sha256 expected-disk-bytes new-initrd expected-initrd-sha256 new-journal wall-ms");
 BootCycleLimits limits;limits.wall_ms=decimal(argv[8],1500000);limits.instructions=UINT64_C(8000000000);limits.chunk=50000;limits.checkpoint_ms=60000;limits.checkpoint_instructions=100000000;
 require(!std::filesystem::exists(argv[7]),"Fresh experiment needs a new journal");
 const std::string disk_sha=argv[3];require(disk_sha.size()==64&&disk_sha.find_first_not_of("0123456789abcdef")==std::string::npos&&disk_sha!=original_disk_sha,"New media identity is required");
 const auto disk_bytes=decimal(argv[4],UINT64_C(1)<<40);require(disk_bytes==UINT64_C(17330864128)&&disk_sha=="0e829fad2cd1cfef8ea707044270d97559127c6cd48bdf268e7eb5cf1b573ce6","Wrong verified C20 media identity");
 std::cerr<<"REVERSE_C21_HOST_PHASE {\"phase\":\"verify_original_immutable_media\"}\n";
 const auto source=guest_read_file(argv[1],MinimalGuest::max_adaptive_snapshot);const auto source_sha=GuestSha256::hash(source);require(source_sha==c18_sha||source_sha==c19_sha,"Wrong original media source");
 const std::size_t at=8+44*8;const auto len=get(source,8+39*8,8);require(source.size()>at&&len<=source.size()-at&&len>=48&&source[3]=='3',"Invalid original media extent");
 require(std::string(source.begin()+at,source.begin()+at+4)=="RVI3","Original source is not PCI media");
 const auto kn=get(source,at+8,8),dt=get(source,at+16,8),rd=get(source,at+24,8),an=get(source,at+32,8),dn=get(source,at+40,8);
 require(kn<=MinimalGuest::max_linux_kernel&&dt<=65536&&rd<=MinimalGuest::max_linux_initrd&&an<=1024&&dn==80&&48+kn+dt+rd+an+dn==len,"Invalid immutable sections");
 const Bytes kernel(source.begin()+at+48,source.begin()+at+48+kn),original_ramdisk(source.begin()+at+48+kn+dt,source.begin()+at+48+kn+dt+rd);
 require(GuestSha256::hash(kernel)==kernel_sha&&GuestSha256::hash(original_ramdisk)==original_ramdisk_sha,"Original signed media changed");
 const std::string ramdisk_sha=argv[6];require(ramdisk_sha=="5278d736d20865d82be9783593690350496d1bcccb157769795316c5e527eefb","Wrong verified C21 initrd identity");
 const auto ramdisk=guest_read_file(argv[5],MinimalGuest::max_linux_initrd);require(GuestSha256::hash(ramdisk)==ramdisk_sha&&ramdisk_sha!=original_ramdisk_sha,"Wrong profile initrd");
 const auto config_begin=[](const Bytes& data){
   require(data.size()>=20&&std::string(data.end()-12,data.end())=="#BOOTCONFIG\n","Missing Linux bootconfig trailer");
   const auto size=get(data,data.size()-20,4);require(size&&size<=32767&&size<=data.size()-20,"Bootconfig extent rejected");
   std::uint32_t sum=0;for(std::size_t p=data.size()-20-size;p<data.size()-20;p++)sum+=data[p];require(sum==get(data,data.size()-16,4),"Bootconfig checksum rejected");return data.size()-20-std::size_t(size);
 };
 const auto old_begin=config_begin(original_ramdisk),new_begin=config_begin(ramdisk);
 require(old_begin==new_begin&&std::equal(original_ramdisk.begin(),original_ramdisk.begin()+old_begin,ramdisk.begin()),"Original ramdisk CPIO changed");
 std::size_t old_end=original_ramdisk.size()-20;while(old_end>old_begin&&!original_ramdisk[old_end-1])--old_end;
 require(ramdisk.size()-20-new_begin>=old_end-old_begin,"Derived config lost source content");
 require(std::equal(original_ramdisk.begin()+old_begin,original_ramdisk.begin()+old_end,ramdisk.begin()+new_begin),"Original bootconfig changed");
 const auto aa=at+48+kn+dt+rd;const std::string args(source.begin()+aa,source.begin()+aa+an);
 require(args==guest_android_cuttlefish_normal_no_scx_bootargs()+" androidboot.boot_devices=30000000.pcie "+avb,"Original verified AVB handoff changed");
 std::cerr<<"REVERSE_C21_HOST_PHASE {\"phase\":\"verify_full_new_disk\"}\n";
 const auto disk=GuestBlockImage::open(argv[2]);require(disk->size()==disk_bytes&&disk->sha256()==disk_sha,"Wrong new disk");
 MinimalGuest guest(UINT64_C(2147483648),0);guest.load_linux_image(kernel,ramdisk,args,disk,true);guest.enable_adaptive_memory();
 std::array<std::uint8_t,32> seed{};std::size_t pos=0;while(pos<seed.size()){const auto n=::getrandom(seed.data()+pos,seed.size()-pos,0);if(n<0&&errno==EINTR)continue;require(n>0,"Host OS entropy unavailable");pos+=std::size_t(n);}
 guest.enable_counter_latency(seed);guest.configure_counter_rate(64);require(guest.registers().instructions==0&&guest.machine().el==1,"Fresh CPU was not initialized");
 const std::string provenance="{\"experiment\":\"c21-fresh-userdata-apex-profile\",\"fresh_machine\":true,\"original_checkpoint_cpu_restored\":false,\"prior_init_proof_reused\":false,\"source_media_checkpoint_sha256\":"+quote(source_sha)+",\"kernel_sha256\":"+quote(kernel_sha)+",\"ramdisk_sha256\":"+quote(ramdisk_sha)+",\"original_ramdisk_sha256\":"+quote(original_ramdisk_sha)+",\"source_cpio_payload_unchanged\":true,\"bootconfig_derived\":true"+",\"original_disk_sha256\":"+quote(original_disk_sha)+",\"disk_sha256\":"+quote(disk_sha)+",\"disk_bytes\":"+std::to_string(disk_bytes)+",\"ram_bytes\":2147483648,\"timing_seed_origin\":\"host_os_getrandom_new_experiment\",\"timing_seed_sha256\":"+quote(GuestSha256::hash(Bytes(seed.begin(),seed.end())))+",\"counter_instructions_per_tick\":64,\"bootargs\":"+quote(args)+"}";
  BootCycleJournal journal(argv[7]);std::signal(SIGTERM,stop);std::signal(SIGINT,stop);
  bool failure=false;std::size_t uart=0;auto last=std::chrono::steady_clock::now(),frame_time=last;
  std::uint64_t captured_frame=UINT64_MAX;
  const auto save=[&](const MinimalGuest& g,const BootCycleReport& r){journal.publish(g,r,provenance);if(journal.pending_cleanup())std::cerr<<"REVERSE_PCI_JOURNAL_CLEANUP {\"pending\":"<<journal.pending_cleanup()<<",\"error\":"<<quote(journal.cleanup_error().message())<<"}\n";std::cerr<<"REVERSE_PCI_SAVED {\"instructions\":"<<g.registers().instructions<<",\"generation\":"<<journal.sequence()<<"}\n";};
  const auto progress=[&](const MinimalGuest& g,const BootCycleReport& r){
    if(r.console.size()>uart){std::cerr<<"REVERSE_PCI_UART "<<quote(r.console.substr(uart))<<'\n';uart=r.console.size();}
    for(const char* s:{"apexd-bootstrap: Found two APEX packages for module", "init: Service apexd-bootstrap has 'reboot_on_failure'", "init: InitFatalReboot:","init: Failed to mount required partitions early","init: Failed to mount /system:","Reboot failed -- System halted","reboot: Restarting system"})if(r.console.find(s)!=std::string::npos)failure=true;
    const auto now=std::chrono::steady_clock::now();if(now-last>=std::chrono::seconds(2)){std::cerr<<"REVERSE_PCI_CYCLE_HEARTBEAT {\"instructions\":"<<g.registers().instructions<<",\"pc\":"<<quote(hex(g.registers().pc))<<",\"elapsed_ms\":"<<r.elapsed_ms<<"}\n";last=now;}
    const auto& display=g.machine().pci;
    if(display.scan_w&&display.scan_h&&display.flushes!=captured_frame&&now-frame_time>=std::chrono::seconds(2)){
      frame_time=now;
      try{
        const auto pixels=g.display_rgb();require(pixels.size()==std::size_t(display.scan_w)*display.scan_h*3,"Frame extent differs");
        const auto root=std::filesystem::path(argv[7]).parent_path(),pending=root/"frame.pending.ppm",target=root/"frame.ppm";
        std::ofstream out(pending,std::ios::binary|std::ios::trunc);out<<"P6\n"<<display.scan_w<<' '<<display.scan_h<<"\n255\n";
        out.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()));out.flush();require(bool(out),"Cannot capture native display");out.close();std::filesystem::rename(pending,target);
        captured_frame=display.flushes;std::cerr<<"REVERSE_PCI_FRAME {\"width\":"<<display.scan_w<<",\"height\":"<<display.scan_h<<",\"frame\":"<<captured_frame<<"}\n";
      }catch(const std::exception& e){std::cerr<<"REVERSE_PCI_FRAME_ERROR "<<quote(e.what())<<'\n';}
    }
  };
  const auto entry=[](MinimalGuest& g){require(g.registers().pc==0x2a4000,"Not original init entry");std::ostringstream code;code<<std::hex<<std::setfill('0');for(unsigned i=0;i<64;i++)code<<std::setw(2)<<unsigned(g.read8(g.physical_address(g.registers().pc+i,false,true)));require(code.str()==android_original_init_code,"Original init bytes changed");const auto n=g.registers().instructions;g.run(1);return !g.machine().el&&g.registers().pc==0x2a4004&&g.registers().instructions==n+1;};
  auto result=run_boot_cycle(guest,limits,save,entry,progress,[&]{return stopping!=0||failure;});
  if(failure){result.reason="guest_boot_failure";result.error="Android reported a fatal boot/mount failure";save(guest,result);}
  std::cout<<boot_cycle_json(guest,result,GuestSha256::hash(guest.snapshot()),provenance)<<'\n';
  return result.reason=="instruction_budget"||result.reason=="host_wall_budget"?0:2;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
