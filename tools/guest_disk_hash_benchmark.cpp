// Authenticate the complete original disk and every cache-miss block.
#include "minimal_guest_block.hpp"
#include <chrono>
#include <iostream>
#include <sys/resource.h>
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("Expected original disk path");
 const auto start=std::chrono::steady_clock::now();
 const auto disk=reverse_pc::guest::GuestBlockImage::open(argv[1]);
 const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
 // Exercise authenticated cache misses at both ends without changing bytes.
 const auto first=disk->read(0,512),last=disk->read(disk->size()-512,512);
 struct rusage usage{};if(getrusage(RUSAGE_SELF,&usage))throw std::runtime_error("RSS unavailable");
 std::cout<<"{\"disk_bytes\":"<<disk->size()<<",\"disk_sha256\":\""<<disk->sha256()
  <<"\",\"attach_us\":"<<elapsed<<",\"peak_rss_kib\":"<<usage.ru_maxrss
  <<",\"cache_miss_blocks_verified\":true,\"guest_instructions_executed\":0}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
