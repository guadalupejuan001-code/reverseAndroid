// C45: equal CPU work and four equal durable checkpoint boundaries.
// Both paths then close, restore without CPU replay, and export canonical RVG.
#include "guest_resident_session.hpp"
#include <sys/resource.h>
#include <iostream>
using namespace reverse_pc::guest;
std::uint64_t count(const char* text,std::uint64_t maximum){const std::string s=text;require(!s.empty()&&s.find_first_not_of("0123456789")==std::string::npos,"Expected decimal count");const auto n=std::stoull(s);require(n&&n<=maximum,"Count out of range");return n;}
std::uint64_t field(const std::string& s,const std::string& key){const auto needle="\""+key+"\":";const auto at=s.find(needle);require(at!=std::string::npos,"Missing benchmark field");return std::stoull(s.substr(at+needle.size()));}
std::string file_sha(const std::string& path){std::ifstream in(path,std::ios::binary);require(bool(in),"Missing benchmark file");GuestSha256 sha;std::array<std::uint8_t,65536> b{};while(in){in.read(reinterpret_cast<char*>(b.data()),b.size());sha.update(b.data(),std::size_t(in.gcount()));}require(in.eof(),"Benchmark hash failed");return sha.finish();}
int main(int argc,char** argv){try{
 require(argc==6||argc==7,"Expected full|journal input output blocks steps [disk]");const std::string mode=argv[1],source=argv[2],output=argv[3];
 require(mode=="full"||mode=="journal","Invalid benchmark mode");require(std::filesystem::weakly_canonical(source)!=std::filesystem::weakly_canonical(output),"Output aliases original source");
 const auto blocks=count(argv[4],8),steps=count(argv[5],250000);const auto original=file_sha(source);
 require(!std::filesystem::exists(output)&&!std::filesystem::exists(output+".journal"),"Benchmark output must be new");
 std::cerr<<"C45 phase=verify_disk mode="<<mode<<'\n';const auto disk=argc==7?GuestBlockImage::open(argv[6]):nullptr;
 std::filesystem::copy_file(source,output);const auto now=[](){return std::chrono::steady_clock::now();};
 const auto us=[](auto begin,auto end){return std::chrono::duration_cast<std::chrono::microseconds>(end-begin).count();};
 std::cerr<<"C45 phase=restore mode="<<mode<<'\n';const auto begin=now();auto guest=GuestResidentSession::open(output,disk,MinimalGuest::max_explicit_adaptive_snapshot);const auto loaded=now();
 const auto initial=field(guest->inspect(),"instructions");std::uint64_t instructions=initial,advanced=0,cpu_us=0,save_us=0,journal_bytes=0,journal_entries=0,full_saves=0;
 std::vector<std::uint64_t> saves;
 auto full_save=[&]{const auto candidate=output+".candidate";guest->stage(candidate);std::filesystem::rename(candidate,output);guest->acknowledge(guest->revision());};
 for(unsigned i=0;i<blocks;++i){
  std::cerr<<"C45 phase=cpu mode="<<mode<<" block="<<i<<'\n';auto phase=now();const auto run=guest->run(steps);cpu_us+=us(phase,now());advanced+=field(run,"executed_this_run");instructions=field(run,"instructions");
  phase=now();
  if(mode=="full"){full_save();++full_saves;}
  else{const auto saved=guest->checkpoint();require(saved.find("\"journal_compaction_required\":true")==std::string::npos,"Benchmark journal exceeded its bound");journal_bytes=field(saved,"journal_bytes");journal_entries=field(saved,"journal_entries");}
  const auto elapsed=std::uint64_t(us(phase,now()));saves.push_back(elapsed);save_us+=elapsed;require(!guest->dirty(),"Benchmark acknowledged no durable checkpoint");
 }
 std::cerr<<"C45 phase=cold_recovery mode="<<mode<<'\n';guest.reset();const auto recovery_begin=now();guest=GuestResidentSession::open(output,disk,MinimalGuest::max_explicit_adaptive_snapshot);const auto recovery_us=us(recovery_begin,now());
 const auto recovered=guest->inspect();require(field(recovered,"instructions")==instructions,"Recovery repeated or lost guest instructions");
 if(mode=="journal")require(field(recovered,"journal_recovered_entries")==blocks,"Recovery skipped committed journal entries");
 std::cerr<<"C45 phase=canonical_export mode="<<mode<<'\n';const auto export_begin=now();full_save();const auto export_us=us(export_begin,now());const auto end=now();
 require(initial+advanced==instructions&&file_sha(source)==original,"Benchmark changed original input or CPU accounting");
 require(!std::filesystem::exists(output+".journal"),"Compaction retained an active old journal");struct rusage rss{};require(::getrusage(RUSAGE_SELF,&rss)==0,"Cannot read RSS");
 std::cout<<"{\"mode\":"<<quote(mode)<<",\"checkpoint_points\":"<<blocks<<",\"steps_per_request\":"<<steps<<",\"initial_instructions\":"<<initial<<",\"instructions\":"<<instructions<<",\"advanced_instructions\":"<<advanced
  <<",\"restore_us\":"<<us(begin,loaded)<<",\"cpu_us\":"<<cpu_us<<",\"checkpoint_total_us\":"<<save_us<<",\"checkpoint_us\":[";
 for(unsigned i=0;i<saves.size();++i){if(i)std::cout<<',';std::cout<<saves[i];}
 std::cout<<"],\"periodic_full_saves\":"<<full_saves<<",\"journal_entries\":"<<journal_entries<<",\"journal_payload_bytes\":"<<journal_bytes<<",\"cold_recovery_us\":"<<recovery_us<<",\"canonical_export_us\":"<<export_us
  <<",\"canonical_export_full_saves\":1,\"elapsed_us\":"<<us(begin,end)<<",\"peak_rss_kib\":"<<rss.ru_maxrss<<",\"sha256\":"<<quote(file_sha(output))<<",\"source_unchanged\":true,\"cpu_replay_on_recovery\":false,\"android_boot\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
