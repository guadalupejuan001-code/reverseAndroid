// The same source can be compiled against an older adaptive_reduced.hpp.
// Run each case in its own process: RSS is physical OS accounting, not payload.
#include "adaptive_reduced.hpp"
#include <sys/resource.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace reverse_pc;
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
int main(int argc,char** argv){try{
  std::string mode=argc>1?argv[1]:"uncompute";
  std::size_t n=mode=="uncompute"?32:mode=="history"?4:(mode=="replay" || mode=="replay_noncancelling")?3:mode=="entangled"?14:mode=="random"?8:1000;
  auto start=Clock::now();
  AdaptiveReduced q(n,(mode=="replay" || mode=="replay_noncancelling")?1:14);
  double trigger=0;
  if(mode=="uncompute"){
    q.apply(Operation::H,0);
    for(unsigned b=1;b<=20;++b){q.apply(Operation::CX,0,b);q.apply(Operation::CX,0,b);}
  }else if(mode=="history"){
    for(unsigned i=0;i<50000;++i)q.apply(Operation::X,0);
  }else if(mode=="replay" || mode=="replay_noncancelling"){
    for(unsigned i=0;i<20000;++i)q.apply(mode=="replay"?Operation::X:Operation::S,0);
    q.apply(Operation::H,0);auto t=Clock::now();q.apply(Operation::CX,0,1);trigger=ms(t);
  }else if(mode=="entangled"){
    q.apply(Operation::H,0);
    for(unsigned b=1;b<n;++b)q.apply(Operation::CX,b-1,b);
    for(unsigned b=0;b<n;++b)q.apply(Operation::T,b);
    for(unsigned b=1;b<n;++b){q.apply(Operation::CX,b-1,b);q.apply(Operation::CX,b-1,b);}
  }else if(mode=="random"){
    std::mt19937 rng(20261025);
    for(unsigned i=0;i<100;++i){unsigned a=rng()%n,b=(a+1+rng()%(n-1))%n;q.apply(static_cast<Operation>(rng()%8),a,b);}
  }else if(mode=="product"){
    for(unsigned b=0;b<n;++b)q.apply(Operation::H,b);
    for(unsigned b=1;b<n;++b)q.apply(Operation::CX,b-1,b);
    for(unsigned b=0;b<n;++b)q.apply(Operation::T,b);
  }else throw std::invalid_argument("unknown measurement case");
  double elapsed=ms(start);
  if(std::abs(q.norm()-1.)>1e-9)throw std::runtime_error("measurement norm");
  rusage usage{};getrusage(RUSAGE_SELF,&usage);
  std::ifstream status("/proc/self/status");std::string key;std::size_t resident=0, high_water=0;
  while(status>>key){if(key=="VmRSS:")status>>resident;else if(key=="VmHWM:")status>>high_water;std::string rest;std::getline(status,rest);}
  std::cout<<std::setprecision(14)<<"{\"case\":\""<<mode<<"\",\"qubits\":"<<n
    <<",\"ms\":"<<elapsed<<",\"fallback_ms\":"<<trigger<<",\"payload_bytes_estimate\":"<<q.estimated_payload_bytes()
    <<",\"rss_kib\":"<<resident<<",\"peak_rss_kib\":"<<high_water<<",\"lifetime_ru_maxrss_kib\":"<<usage.ru_maxrss
    <<",\"fallback\":"<<(q.uses_original_reduced()?"true":"false")<<",\"blocks\":"<<q.block_count()
    <<",\"max_block_width\":"<<q.max_block_width()<<",\"norm\":"<<q.norm()<<",\"z0\":"<<q.expectation('Z',0)<<"}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
