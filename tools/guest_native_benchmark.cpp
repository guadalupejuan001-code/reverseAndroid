// Bounded diagnostic of a host build. Never publishes a continuation checkpoint.
// Compare the final complete-state hash across builds before using a build flag.
#include "guest_file_session.hpp"
#include <chrono>
#include <iostream>
using namespace reverse_pc::guest;
static bool sha_text(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
static std::uint64_t decimal(const char* text,std::uint64_t max){const std::string s(text);require(!s.empty()&&s.find_first_not_of("0123456789")==std::string::npos,"Expected decimal bound");const auto n=std::stoull(s);require(n&&n<=max,"Diagnostic bound exceeded");return n;}
int main(int argc,char** argv){try{
 require(argc==7,"Expected state.rvg expected-state-sha disk.raw expected-disk-sha disk-bytes instructions");
 const std::string state_sha=argv[2],disk_sha=argv[4];require(sha_text(state_sha)&&sha_text(disk_sha),"Invalid SHA-256 pins");
 const auto disk_bytes=decimal(argv[5],UINT64_C(1)<<40),steps=decimal(argv[6],5000000);
 const auto input=guest_read_file(argv[1],MinimalGuest::max_explicit_adaptive_snapshot);require(GuestSha256::hash(input)==state_sha,"Wrong diagnostic source");
 const auto disk=GuestBlockImage::open(argv[3]);require(disk->size()==disk_bytes&&disk->sha256()==disk_sha,"Wrong disk identity");
 auto guest=MinimalGuest::from_snapshot(input,disk,MinimalGuest::max_explicit_adaptive_snapshot);require(guest.snapshot()==input,"Restore changed diagnostic source");
 const auto initial=guest.registers().instructions;require(initial<=UINT64_MAX-steps&&!guest.registers().halted&&!guest.machine().waiting,"Source cannot be benchmarked");
 const auto start=std::chrono::steady_clock::now();
 while(guest.registers().instructions-initial<steps){
  const auto n=std::min<std::uint64_t>(50000,steps-(guest.registers().instructions-initial));
  const auto r=guest.run(n);require(r.executed&&!guest.registers().halted,"Diagnostic source stopped executing");
 }
 const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
 const auto final=guest.snapshot();require(GuestSha256::hash(guest_read_file(argv[1],MinimalGuest::max_explicit_adaptive_snapshot))==state_sha,"Diagnostic changed the source file");
 std::cout<<"{\"schema\":\"reverse-native-build-benchmark-v1\",\"benchmark_only\":true,\"source_checkpoint_sha256\":"<<quote(state_sha)
  <<",\"instructions_executed\":"<<steps<<",\"initial_instructions\":"<<initial<<",\"final_instructions\":"<<guest.registers().instructions<<",\"final_pc\":"<<quote(hex(guest.registers().pc))
  <<",\"execution_elapsed_ns\":"<<elapsed<<",\"final_snapshot_sha256\":"<<quote(GuestSha256::hash(final))
  <<",\"original_source_file_unchanged\":true,\"execution_progress_accepted\":false,\"android_boot_verified\":false}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
