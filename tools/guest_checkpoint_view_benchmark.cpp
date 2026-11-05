// Isolated original QuantumRuntime restore from real RVG memory components.
// No disk is invented or attached and no guest instructions are executed.
#include "guest_file_session.hpp"
#include <sys/resource.h>
#include <iostream>
using namespace reverse_pc::guest;
int main(int argc,char** argv){try{
 require(argc==3,"Expected legacy|view checkpoint.rvg");const std::string mode=argv[1];
 require(mode=="legacy"||mode=="view","Invalid restore benchmark mode");
 const auto bytes=guest_read_file(argv[2],MinimalGuest::max_view_snapshot);
 require(bytes.size()>=360&&bytes[0]=='R'&&bytes[1]=='V'&&bytes[2]=='G'&&bytes[3]=='3',"Expected original RVG3");
 require(get(bytes,bytes.size()-8,8)==digest(bytes,bytes.size()-8),"Input checksum mismatch");
 const auto image=get(bytes,320,8),core_size=get(bytes,328,8),memory_size=get(bytes,336,8);
 const auto memory_at=360+image+core_size;
 require(memory_at<=bytes.size()-8&&memory_size<=bytes.size()-8-memory_at,"Invalid memory extent");
 const std::string core(bytes.begin()+360+std::size_t(image),bytes.begin()+std::size_t(memory_at));
 const std::string_view memory(reinterpret_cast<const char*>(bytes.data()+memory_at),std::size_t(memory_size));
 mundo_x::InverseMemory::Limits limits;limits.max_overrides=0;
 mundo_x::QuantumRuntime runtime(mundo_x::InfiniteCore::Background::Zero,mundo_x::InfiniteCore::Limits{},limits);
 const auto start=std::chrono::steady_clock::now();
 if(mode=="legacy")runtime.restore(core,std::string(memory));else runtime.restore_view(core,memory);
 const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
 require(runtime.core_snapshot()==core,"Core restore differs");std::size_t at=0;GuestSha256 sha;
 runtime.memory().persistent_snapshot_to([&](const char* data,std::size_t size){
  require(at<=memory.size()&&size<=memory.size()-at&&std::equal(data,data+size,memory.begin()+at),"Original memory round trip differs");
  sha.update(reinterpret_cast<const std::uint8_t*>(data),size);at+=size;
 });
 require(at==memory.size(),"Restored memory size differs");struct rusage usage{};require(::getrusage(RUSAGE_SELF,&usage)==0,"Cannot measure RSS");
 std::cout<<"{\"mode\":"<<quote(mode)<<",\"scope\":\"original_quantum_runtime_memory_transport\",\"input_bytes\":"<<bytes.size()
  <<",\"memory_bytes\":"<<memory.size()<<",\"restore_us\":"<<elapsed<<",\"peak_rss_kib\":"<<usage.ru_maxrss
  <<",\"memory_sha256\":"<<quote(sha.finish())<<",\"memory_overrides\":"<<runtime.memory().stats().overrides
  <<",\"round_trip_exact\":true,\"guest_instructions_executed\":0,\"android_boot\":false}\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
