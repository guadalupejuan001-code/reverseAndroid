// Diagnostic real instruction timing. This never supplies a resume contract.
#include "guest_file_session.hpp"
#include <chrono>
#include <iostream>
#include <sys/resource.h>
using namespace reverse_pc::guest;
int main(int argc,char** argv){try{
 require(argc==6||argc==7,"Expected checkpoint disk output source-sha instruction-budget [host-chunk]");
 const std::string digest=argv[4];require(digest.size()==64,"Expected source SHA-256");
 const auto budget=std::stoull(argv[5]);require(budget>0&&budget<=100000000,"Invalid diagnostic instruction budget");
 const auto chunk=argc==7?std::stoull(argv[6]):UINT64_C(200000);
 require(chunk>0&&chunk<=1000000,"Invalid explicit diagnostic host chunk");
 auto input=guest_read_file(argv[1],MinimalGuest::max_view_snapshot);
 require(GuestSha256::hash(input)==digest,"Wrong diagnostic source");
 auto disk=GuestBlockImage::open(argv[2]);
 require(disk->size()==UINT64_C(10888413184)&&disk->sha256()=="4c2eaf3fa7d81e9649072fc81f0651f4129bd9f41d4a9f9964cc627741a7b92e","Wrong original Android disk");
 auto guest=MinimalGuest::from_snapshot_view(input,disk,MinimalGuest::max_view_snapshot);std::size_t at=0;
 guest.snapshot_to([&](const std::uint8_t* data,std::size_t size){require(at<=input.size()&&size<=input.size()-at&&std::equal(data,data+size,input.begin()+at),"Diagnostic source restore changed bytes");at+=size;});
 require(at==input.size(),"Diagnostic restore extent changed");Bytes().swap(input);
 require(guest.logical_ram()==2147483648&&guest.machine().enabled&&guest.machine().counter_latency.instructions_per_tick==64,"Diagnostic board profile changed");
 const auto initial=guest.registers().instructions;const auto start=std::chrono::steady_clock::now();
 while(guest.registers().instructions-initial<budget){
  const auto step=guest.run(std::min<std::uint64_t>(chunk,budget-(guest.registers().instructions-initial)));
  require(!step.halted&&!step.waiting&&step.executed>0,"Diagnostic guest stopped before the budget");
 }
 const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
 require(guest.registers().instructions==initial+budget,"Diagnostic instruction count changed");
 guest_write_snapshot_candidate(guest,argv[3]);struct rusage usage{};require(getrusage(RUSAGE_SELF,&usage)==0,"RSS unavailable");
 std::cout<<"{\"instructions\":"<<budget<<",\"host_chunk\":"<<chunk<<",\"initial_instructions\":"<<initial<<",\"final_instructions\":"<<guest.registers().instructions
  <<",\"execute_us\":"<<elapsed<<",\"peak_rss_kib\":"<<usage.ru_maxrss<<",\"checkpoint_sha256\":"<<quote(guest.snapshot_sha256())
  <<",\"source_sha256\":"<<quote(digest)<<",\"diagnostic_not_resume_source\":true,\"android_boot_verified\":false}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
