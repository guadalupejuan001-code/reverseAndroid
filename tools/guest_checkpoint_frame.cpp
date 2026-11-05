// Read-only export of the original XI/Ether scanout at one authenticated snapshot.
// No CPU instructions execute. A frame by itself is not Android boot evidence.
#include "guest_file_session.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>
using namespace reverse_pc::guest;
static bool digest(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
static std::uint64_t decimal(const std::string& s){require(!s.empty()&&s.find_first_not_of("0123456789")==std::string::npos,"Invalid disk size");const auto n=std::stoull(s);require(n<=(UINT64_C(1)<<40),"Disk size exceeds bound");return n;}
static void exclusive_write(const char* path,const Bytes& bytes){
  const int fd=::open(path,O_WRONLY|O_CREAT|O_EXCL,0600);require(fd>=0,"Frame output must be a new file");
  bool ok=true;std::size_t at=0;
  while(at<bytes.size()){
    const auto n=::write(fd,bytes.data()+at,bytes.size()-at);
    if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}at+=std::size_t(n);
  }
  if(::fsync(fd))ok=false;if(::close(fd))ok=false;require(ok,"Incomplete frame output");
}
int main(int argc,char** argv){try{
  require(argc==7,"Expected checkpoint.rvg disk.raw|- checkpoint-sha256 disk-sha256 disk-bytes new-frame.ppm");
  const std::string state_sha=argv[3],disk_sha=argv[4];require(digest(state_sha)&&digest(disk_sha),"Invalid SHA-256 pins");
#ifdef REVERSE_PC_COMPACT_RESTORE_VIEW
  constexpr auto capacity=MinimalGuest::max_view_snapshot;
#else
  constexpr auto capacity=MinimalGuest::max_explicit_adaptive_snapshot;
#endif
  const auto disk_bytes=decimal(argv[5]);const auto input=guest_read_file(argv[1],capacity);
  require(GuestSha256::hash(input)==state_sha,"Wrong checkpoint SHA-256");
  std::shared_ptr<const GuestBlockImage> disk;
  if(disk_bytes){require(std::string(argv[2])!="-","Explicit disk is required");disk=GuestBlockImage::open(argv[2]);require(disk->size()==disk_bytes&&disk->sha256()==disk_sha,"Wrong disk identity");}
  else require(std::string(argv[2])=="-"&&disk_sha==std::string(64,'0'),"Invalid empty disk identity");
#ifdef REVERSE_PC_COMPACT_RESTORE_VIEW
  auto guest=MinimalGuest::from_snapshot_view(input,disk,capacity);
#else
  auto guest=MinimalGuest::from_snapshot(input,disk,capacity);
#endif
  const auto verify_original=[&]{
    std::size_t compared=0;
    guest.snapshot_to([&](const std::uint8_t* bytes,std::size_t size){
      require(compared<=input.size()&&size<=input.size()-compared&&std::equal(bytes,bytes+size,input.begin()+compared),"Restore changed the original checkpoint");compared+=size;
    });
    require(compared==input.size(),"Restore changed the original checkpoint extent");
  };
  verify_original();
  const auto count=guest.registers().instructions,pc=guest.registers().pc;const auto& display=guest.machine().pci;
  const auto rgb=guest.display_rgb();
  require(guest.registers().instructions==count&&guest.registers().pc==pc,"Read-only scanout changed the guest");verify_original();
  if(rgb.empty()){
    require(!display.scanout||!display.scan_w||!display.scan_h,"Empty active scanout");
    std::cout<<"{\"schema\":\"reverse-snapshot-scanout-v1\",\"status\":\"no_scanout\",\"source_checkpoint_sha256\":"<<quote(state_sha)<<",\"cpu_instructions_executed\":0,\"original_snapshot_unchanged\":true,\"android_boot_verified\":false,\"android_display_verified\":false}\n";return 3;
  }
  require(display.scan_w&&display.scan_w<=1280&&display.scan_h&&display.scan_h<=1024&&rgb.size()==std::size_t(display.scan_w)*display.scan_h*3,"Invalid RGB extent");
  const std::string header="P6\n"+std::to_string(display.scan_w)+" "+std::to_string(display.scan_h)+"\n255\n";
  Bytes ppm(header.begin(),header.end());ppm.insert(ppm.end(),rgb.begin(),rgb.end());exclusive_write(argv[6],ppm);
  std::cout<<"{\"schema\":\"reverse-snapshot-scanout-v1\",\"status\":\"scanout_exported\",\"source_checkpoint_sha256\":"<<quote(state_sha)
    <<",\"instructions\":"<<count<<",\"pc\":"<<quote(hex(pc))<<",\"frame_counter\":"<<display.flushes<<",\"width\":"<<display.scan_w<<",\"height\":"<<display.scan_h
    <<",\"rgb_sha256\":"<<quote(GuestSha256::hash(rgb))<<",\"ppm_sha256\":"<<quote(GuestSha256::hash(ppm))
    <<",\"cpu_instructions_executed\":0,\"original_snapshot_unchanged\":true,\"android_boot_verified\":false,\"android_display_verified\":false}\n";
  return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
