// C44: equal CPU requests with reload/save per block versus one resident guest.
// Fixture mode creates an explicit synthetic Linux Image loop, not Android.
#include "guest_resident_session.hpp"
#include <sys/resource.h>
#include <iostream>
using namespace reverse_pc::guest;
std::uint64_t number(const char* text,std::uint64_t max,bool zero=false){const std::string s=text;require(!s.empty()&&s.find_first_not_of("0123456789")==std::string::npos,"Expected decimal count");const auto n=std::stoull(s);require((zero||n)&&n<=max,"Count out of range");return n;}
std::uint64_t field(const std::string& s,const std::string& key){const auto needle="\""+key+"\":";const auto at=s.find(needle);require(at!=std::string::npos,"Missing benchmark field");return std::stoull(s.substr(at+needle.size()));}
std::string file_sha(const std::string& path){std::ifstream in(path,std::ios::binary);require(bool(in),"Cannot hash benchmark file");std::array<std::uint8_t,65536> b{};GuestSha256 sha;while(in){in.read(reinterpret_cast<char*>(b.data()),b.size());sha.update(b.data(),std::size_t(in.gcount()));}require(in.eof(),"Benchmark hash read failed");return sha.finish();}
int main(int argc,char** argv){try{
 require(argc>=2,"Expected fixture|reload|resident");const std::string mode=argv[1];
 if(mode=="fixture"){
  require(argc==4,"Expected fixture output instructions");auto count=number(argv[3],2000000,true);
  Bytes image(42031616);auto set=[&](unsigned at,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)image[at+i]=std::uint8_t(v>>(8*i));};
  set(0,0x14000000,4);set(16,image.size(),8);set(24,2,8);set(56,0x644d5241,4);
  MinimalGuest guest(128*1024*1024,0);guest.load_linux_image(image);image.clear();image.shrink_to_fit();
  while(count){const auto n=std::min<std::uint64_t>(count,250000);const auto run=guest.run(n);require(run.executed==n,"Fixture did not execute requested instructions");count-=n;}
  const auto bytes=guest_write_snapshot_candidate(guest,argv[2]);
  std::cout<<"{\"fixture\":true,\"bytes\":"<<bytes<<",\"instructions\":"<<guest.registers().instructions<<",\"sha256\":"<<quote(file_sha(argv[2]))<<",\"android_boot\":false}\n";return 0;
 }
 require((mode=="reload"||mode=="resident")&&(argc==6||argc==7),"Expected reload|resident input output blocks steps [disk]");
 const std::string source=argv[2],output=argv[3];require(std::filesystem::weakly_canonical(source)!=std::filesystem::weakly_canonical(output),"Output aliases input");
 const auto blocks=number(argv[4],8),steps=number(argv[5],250000);const auto original=file_sha(source);
 std::cerr<<"C44 phase=verify_disk mode="<<mode<<'\n';
 const auto disk=argc==7?GuestBlockImage::open(argv[6]):nullptr;
 std::cerr<<"C44 phase=execute_equal_requests mode="<<mode<<'\n';
 std::uint64_t initial=0,instructions=0,restores=0,writes=0,advanced=0;const auto begin=std::chrono::steady_clock::now();
 if(mode=="reload"){
  std::string current=source;
  for(unsigned i=0;i<blocks;++i){
   auto guest=MinimalGuest::from_snapshot(guest_read_file(current,MinimalGuest::max_explicit_adaptive_snapshot),disk,MinimalGuest::max_explicit_adaptive_snapshot);++restores;
   if(!i)initial=guest.registers().instructions;
   const auto run=guest.run(steps);advanced+=run.executed;instructions=guest.registers().instructions;
   const auto next=output+".block-"+std::to_string(i);guest_write_snapshot_candidate(guest,next);++writes;
   if(current!=source)std::filesystem::remove(current);current=next;
  }
  std::filesystem::rename(current,output);
 }else{
  auto guest=GuestResidentSession::open(source,disk,MinimalGuest::max_explicit_adaptive_snapshot);++restores;initial=field(guest->inspect(),"instructions");
  for(unsigned i=0;i<blocks;++i){const auto report=guest->run(steps);advanced+=field(report,"executed_this_run");instructions=field(report,"instructions");}
  guest->stage(output);++writes;guest->acknowledge(guest->revision());require(!guest->dirty(),"Benchmark failed to acknowledge saved state");
 }
 const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count();
 require(initial+advanced==instructions,"Instruction accounting differs");require(file_sha(source)==original,"Benchmark changed original checkpoint");
 struct rusage usage{};require(::getrusage(RUSAGE_SELF,&usage)==0,"Cannot measure benchmark RSS");
 std::cout<<"{\"mode\":"<<quote(mode)<<",\"blocks\":"<<blocks<<",\"steps_per_request\":"<<steps<<",\"advanced_instructions\":"<<advanced<<",\"initial_instructions\":"<<initial<<",\"instructions\":"<<instructions<<",\"restores\":"<<restores<<",\"checkpoint_writes\":"<<writes<<",\"elapsed_us\":"<<elapsed<<",\"peak_rss_kib\":"<<usage.ru_maxrss<<",\"rss_scope\":\"whole-process-including-original-decoder\",\"sha256\":"<<quote(file_sha(output))<<",\"source_unchanged\":true,\"android_boot\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
