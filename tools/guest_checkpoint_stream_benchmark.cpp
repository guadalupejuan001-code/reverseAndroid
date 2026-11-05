// Host I/O benchmark. Restores through the unchanged decoder and executes no CPU.
#include "guest_file_session.hpp"
#include <sys/resource.h>
#include <sys/wait.h>
#include <malloc.h>
#include <unistd.h>
#include <iostream>
using namespace reverse_pc::guest;
int main(int argc,char** argv){try{
 require(argc==4||argc==5,"Expected legacy|stream checkpoint output [disk]");const std::string mode=argv[1];
 require(mode=="legacy"||mode=="stream","Unknown benchmark mode");
 auto input=guest_read_file(argv[2],MinimalGuest::max_explicit_adaptive_snapshot);const auto input_sha=GuestSha256::hash(input);
 const auto disk=argc==5?GuestBlockImage::open(argv[4]):nullptr;
 auto guest=MinimalGuest::from_snapshot(input,disk,MinimalGuest::max_explicit_adaptive_snapshot);
 const auto instructions=guest.registers().instructions;const auto pc=guest.registers().pc;
 // A fresh child isolates serializer RSS from the unchanged decoder's earlier
 // peak. Both modes inherit the same fully restored guest and no input buffer.
 input.clear();input.shrink_to_fit();::malloc_trim(0);
 const auto child=::fork();require(child>=0,"Cannot isolate benchmark process");
 if(child){int status=0;require(::waitpid(child,&status,0)==child,"Cannot wait for benchmark");return WIFEXITED(status)?WEXITSTATUS(status):1;}
 std::ofstream out(argv[3],std::ios::binary|std::ios::trunc);require(bool(out),"Cannot open benchmark output");
 GuestSha256 sha;std::size_t size=0,largest=0;const auto start=std::chrono::steady_clock::now();
 auto sink=[&](const std::uint8_t* data,std::size_t length){out.write(reinterpret_cast<const char*>(data),std::streamsize(length));require(bool(out),"Benchmark write failed");sha.update(data,length);size+=length;largest=std::max(largest,length);};
 if(mode=="legacy"){const auto saved=guest.snapshot();sink(saved.data(),saved.size());}else guest.snapshot_to(sink);
 out.close();require(bool(out),"Benchmark close failed");const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
 const auto saved_sha=sha.finish();require(saved_sha==input_sha,"Round trip changed RVG SHA-256");
 require(instructions==guest.registers().instructions&&pc==guest.registers().pc,"Serialization advanced guest CPU");
 struct rusage usage{};require(::getrusage(RUSAGE_SELF,&usage)==0,"Cannot read peak RSS");
 std::cout<<"{\"mode\":"<<quote(mode)<<",\"bytes\":"<<size<<",\"sha256\":"<<quote(saved_sha)<<",\"serialization_us\":"<<elapsed<<",\"peak_rss_kib\":"<<usage.ru_maxrss<<",\"rss_scope\":\"post-restore-child\",\"largest_sink_chunk\":"<<largest<<",\"instructions\":"<<instructions<<",\"cpu_instructions_executed\":0,\"decoder_unchanged\":true,\"android_boot\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
