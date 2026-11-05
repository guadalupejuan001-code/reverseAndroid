#include "minimal_guest_block.hpp"
#include <filesystem>
#include <iostream>
#include <chrono>
using namespace reverse_pc::guest;
static unsigned checks=0;
static void check(bool ok){++checks;if(!ok)throw std::runtime_error("Host disk SHA regression");}
template<class F>static void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad);}
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("Expected new temporary directory");
 const std::filesystem::path root(std::string(argv[1])+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 check(std::filesystem::create_directory(root));
 check(GuestDiskSha256::hash({})=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
 check(GuestDiskSha256::hash({'a','b','c'})=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
 std::uint64_t seed=48;
 for(auto n:{0u,1u,55u,56u,63u,64u,65u,511u,512u,65535u,65536u,65537u,1048576u}){
  std::vector<std::uint8_t> bytes(n);for(auto& b:bytes){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;b=std::uint8_t(seed);}
  const auto reference=GuestSha256::hash(bytes);check(GuestDiskSha256::hash(bytes)==reference);
  for(auto chunk:{1u,7u,64u,4096u,65536u}){
   GuestDiskSha256 h;for(std::size_t at=0;at<bytes.size();at+=chunk)h.update(bytes.data()+at,std::min<std::size_t>(chunk,bytes.size()-at));check(h.finish()==reference);
  }
 }
 const auto path=root/"fixture.raw";std::vector<std::uint8_t> bytes(3*65536+512);
 for(auto& b:bytes){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;b=std::uint8_t(seed);}
 {std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());check(bool(f));}
 const auto disk=GuestBlockImage::open(path.string());check(disk->size()==bytes.size());check(disk->sha256()==GuestSha256::hash(bytes));
 for(auto at:{0u,17u,65530u,65536u,131067u,196600u}){
  const auto n=std::min<std::size_t>(1000,bytes.size()-at);const auto part=disk->read(at,n);
  check(std::equal(part.begin(),part.end(),bytes.begin()+at));
 }
 rejects([&]{disk->read(bytes.size(),1);});rejects([&]{disk->read(0,1024*1024+1);});
 disk->read(131072,1); // Keep a different block cached before altering block 0.
 {std::fstream f(path,std::ios::binary|std::ios::in|std::ios::out);const char b=char(bytes[0]^1);f.write(&b,1);f.flush();check(bool(f));}
 rejects([&]{disk->read(0,1);});check(disk->read(131072,1)[0]==bytes[131072]);
 {std::filesystem::resize_file(path,65536);}
 rejects([&]{disk->read(196608,512);});
 // Compare both algorithms over the same sizeable transport buffer.
 bytes.resize(64*1024*1024,0x5a);auto start=std::chrono::steady_clock::now();const auto baseline=GuestSha256::hash(bytes);
 const auto slow=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
 start=std::chrono::steady_clock::now();const auto fast=GuestDiskSha256::hash(bytes);
 const auto quick=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();check(baseline==fast);
 std::filesystem::remove_all(root);
 std::cout<<"{\"checks\":"<<checks<<",\"portable_us\":"<<slow<<",\"host_us\":"<<quick<<",\"digest_identical\":true,\"changed_blocks_rejected\":true,\"guest_instructions_executed\":0}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
