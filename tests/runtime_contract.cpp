// Compile this exact probe against both upstream and the reviewed extension.
#include "mundo_x/quantum_runtime.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
using Runtime=mundo_x::QuantumRuntime;
// Compare both implementations under the same historical memory budget.
// E025's intentional 524288 default is tested separately by its release suite.
Runtime::MemoryLimits historical_memory(){Runtime::MemoryLimits limits;limits.max_overrides=65536;return limits;}
void need(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
std::string init(unsigned n){std::ostringstream p;p<<"begin\n";for(unsigned i=0;i<n;++i)p<<"x "<<i<<'\n';return p.str()+"end\n";}
void emit(unsigned id,const Runtime& r,const std::string& result){
  std::cout<<"CASE "<<id<<'\n'<<result.size()<<':'<<result<<'\n'<<r.core_snapshot()<<'\n'<<r.memory_snapshot()<<'\n';
}
void failure(Runtime& r,const std::string& program,unsigned id){
  const auto core=r.core_snapshot(),mem=r.memory_snapshot();bool rejected=false;std::string error;
  try{r.execute(program);}catch(const std::exception& e){rejected=true;error=e.what();}
  need(rejected && r.core_snapshot()==core && r.memory_snapshot()==mem,"program failure leaked state/RNG/memory");
  emit(id,r,error);
}
void contracts(){
  Runtime baseline(Runtime::Background::Zero,{},historical_memory());baseline.execute(init(256));baseline.execute("mem_store 7 5\nmem_bind 9 100\nseed 42\n");
  const auto original=baseline.core_snapshot();unsigned id=0;
  Runtime serial=baseline;std::string xs;for(unsigned i=0;i<128;++i)xs+="x 0\n";
  auto result=serial.execute(xs+"stats\nmem_stats\n");
  need(serial.core().stats().revision==129,"individual revisions grouped");emit(id++,serial,result);
  need(baseline.core_snapshot()==original,"branch modified source");
  Runtime batch=baseline;emit(id++,batch,batch.execute("begin\n"+xs+"end\nstats\n"));
  need(batch.core().stats().revision==2,"explicit batch revisions changed");
  failure(serial,"mem_store 7 77\nx 0\nh 1\nmeasure 1\nmissing_instruction\n",id++);
  emit(id++,serial,serial.execute("h 1\nmeasure 1\nmeasure 1\nstats\n"));
  Runtime restored(Runtime::Background::Zero,{},historical_memory());restored.restore(serial.core_snapshot(),serial.memory_snapshot());
  need(restored.core_snapshot()==serial.core_snapshot() && restored.memory_snapshot()==serial.memory_snapshot(),"restore changed snapshot");
  emit(id++,restored,restored.execute("h 2\nmeasure 2\nstats\n"));
  std::mt19937 rng(20261025);
  for(unsigned trial=0;trial<160;++trial){
    Runtime r=baseline;std::ostringstream p;
    for(unsigned j=0;j<40;++j){
      unsigned a=rng()%32,b=(a+1+rng()%31)%32,op=rng()%7;
      const char* names[]={"h","s","x","y","z","cx","cz"};
      p<<names[op]<<' '<<a;if(op>4)p<<' '<<b;p<<'\n';
      if(j%11==0)p<<"stats\nprobz "<<a<<"\n";
    }
    p<<"mem_read 9\nmem_read 7\nmeasure 0\nmem_stats\n";
    emit(id++,r,r.execute(p.str()));
    need(baseline.core_snapshot()==original,"random branch isolation");
  }
  Runtime globals(Runtime::Background::Zero,{},historical_memory());emit(id++,globals,globals.execute("x 10\nh 11\nh_all\nx 0\ncz_neighbors\nh 2\nx 1\nprobz 12\nstats\n"));
  Runtime bell(Runtime::Background::BellPairs,{},historical_memory());
  emit(id++,bell,bell.execute("seed 2026\nx 0\nh 2\ncx 1 3\nmeasure 0\nstats\n"));
  Runtime::CoreLimits cap;cap.max_active_qubits=1;
  Runtime limited(Runtime::Background::Zero,cap,historical_memory());limited.execute("x 0\nseed 19\n");
  failure(limited,"x 0\nh 0\nx 1\n",id++);
  failure(limited,"h 0\nmeasure 0\nx 1\n",id++);
  failure(limited,"x 0\nbegin\nh 0\ncx 0 1\nend\n",id++);
  failure(limited,"x 0\ncx 0 0\n",id++);
  failure(limited,"x 0\nmem_store 7 22\nmem_bind 8 nope\n",id++);
  Runtime::MemoryLimits ml;ml.max_overrides=1;
  Runtime memory_cap(Runtime::Background::Zero,{},ml);
  failure(memory_cap,"mem_store 1 44\nx 0\nmem_store 2 99\n",id++);
  Runtime full(Runtime::Background::Zero,{},historical_memory());
  full.execute(init(4096));
  failure(full,"x 0\nx 4096\n",id++);
  Runtime revision_cap(Runtime::Background::Zero,{},historical_memory());
  auto maximal=revision_cap.core_snapshot();
  need(maximal.substr(0,5)=="MXI1:","revision fixture envelope");
  maximal.replace(7,16,"feffffffffffffff"); // UINT64_MAX-1, little endian
  revision_cap.restore(maximal,revision_cap.memory_snapshot());
  failure(revision_cap,"x 0\nx 0\n",id++);
  // Direct InfiniteCore callers retain per-operation failure atomicity.
  mundo_x::InfiniteCore core(mundo_x::InfiniteCore::Background::Zero,cap);
  auto saved=core.snapshot();bool rejected=false;
  try{core.apply({{'X',"0",{}},{'X',"1",{}}});}catch(const std::exception&){rejected=true;}
  need(rejected && core.snapshot()==saved,"standalone core lost atomicity");
  std::cout<<"CONTRACT_CASES "<<id<<'\n';
}
void bench(unsigned active){
  Runtime r(Runtime::Background::Zero,{},historical_memory());r.execute(init(active));std::string program;for(unsigned i=0;i<128;++i)program+="x 0\n";
  auto t=std::chrono::steady_clock::now();r.execute(program);
  auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
  need(r.core().stats().revision==129 && r.core().probability_one("0")==1.,"bench state/revision");
  std::ifstream status("/proc/self/status");std::string key;unsigned rss=0,peak=0;
  while(status>>key){if(key=="VmRSS:")status>>rss;else if(key=="VmHWM:")status>>peak;std::string rest;std::getline(status,rest);}
  std::cout<<std::setprecision(14)<<"{\"active\":"<<active<<",\"gates\":128,\"revision\":129,\"ms\":"<<ms<<",\"rss_kib\":"<<rss<<",\"peak_rss_kib\":"<<peak<<",\"tableau_bytes\":"<<r.core().stats().tableau_bytes<<"}\n";
}
int main(int argc,char** argv){try{if(argc==3 && std::string(argv[1])=="--bench")bench(std::stoul(argv[2]));else contracts();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
