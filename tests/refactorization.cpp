// Dynamic factorization regressions. Dense gates are independently coded in
// factorization_benchmark.cpp; that existing suite remains mandatory as well.
#include "adaptive_reduced.hpp"
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
using namespace reverse_pc;
void need(bool yes, const char* why) { if (!yes) throw std::runtime_error(why); }
void apply_oracle(std::vector<Complex>& v, Gate g) {
  const auto a = std::size_t{1} << g.a, b = std::size_t{1} << g.b;
  const Complex t = std::polar(1., std::acos(-1.)/4.);
  for (std::size_t i=0;i<v.size();++i) {
    if (g.operation==Operation::CX) {
      if ((i&a) && !(i&b)) std::swap(v[i],v[i|b]);
    } else if (g.operation==Operation::CZ) {
      if ((i&a) && (i&b)) v[i]=-v[i];
    } else if (!(i&a)) {
      auto x=v[i], y=v[i|a];
      switch(g.operation) {
        case Operation::H: v[i]=(x+y)/std::sqrt(2.); v[i|a]=(x-y)/std::sqrt(2.); break;
        case Operation::X: v[i]=y; v[i|a]=x; break;
        case Operation::Z: v[i|a]=-y; break;
        case Operation::S: v[i|a]=y*Complex{0,1}; break;
        case Operation::T: v[i|a]=y*t; break;
        case Operation::TDag: v[i|a]=y*std::conj(t); break;
        default: break;
      }
    }
  }
}
int main() {
  try {
    for (auto op : {Operation::CX,Operation::CZ}) {
      AdaptiveReduced q(1000,2);
      q.apply(Operation::H,0); q.apply(Operation::T,0);
      for (unsigned b=1;b<1000;++b) {
        if(op==Operation::CZ) q.apply(Operation::H,b);
        q.apply(op,0,b);
        need(q.block_count()==999,"genuine entanglement lost");
        q.apply(op,0,b);
        need(q.block_count()==1000 && q.max_block_width()==1,
             "cancelled gate left a joined block");
      }
      need(!q.uses_original_reduced(),"avoidable fallback after uncompute");
      need(std::abs(q.norm()-1.)<1e-10,"large uncompute norm");
    }
    // Undo a bridge between two internally entangled blocks. Neither factor
    // has a separable singleton: this requires retaining the merge partition.
    AdaptiveReduced bells(4,4);
    bells.apply(Operation::H,2); bells.apply(Operation::CX,2,0);
    bells.apply(Operation::H,3); bells.apply(Operation::CX,3,1);
    auto saved=bells.dense();
    bells.apply(Operation::CX,0,3);
    need(bells.block_count()==1,"Bell bridge did not merge");
    bells.apply(Operation::CX,0,3);
    need(bells.block_count()==2 && bells.max_block_width()==2,"Bell pair partition not recovered");
    need(bells.dense()==saved,"split changed stored amplitudes or phase");
    std::mt19937 random(20261025);
    double worst=0.;
    for(unsigned trial=0;trial<400;++trial) {
      const unsigned n=2+trial%5;
      AdaptiveReduced q(n,1+trial%6);
      std::vector<Complex> oracle(std::size_t{1}<<n); oracle[0]=1.;
      std::vector<Gate> sequence;
      auto step=[&](Gate g) {
        q.apply(g.operation,g.a,g.b); apply_oracle(oracle,g);
        auto dense=q.dense();
        for(std::size_t i=0;i<dense.size();++i) worst=std::max(worst,std::abs(dense[i]-oracle[i]));
        need(worst<2e-10,"per-gate amplitude/phase oracle mismatch");
        need(std::abs(q.norm()-1.)<2e-10,"random norm");
      };
      for(unsigned j=0;j<50;++j) {
        unsigned a=random()%n,b=(a+1+random()%(n-1))%n;
        Gate g{static_cast<Operation>(random()%8),a,b}; sequence.push_back(g); step(g);
      }
      for(auto it=sequence.rbegin();it!=sequence.rend();++it) {
        auto g=*it;
        if(g.operation==Operation::T) g.operation=Operation::TDag;
        else if(g.operation==Operation::TDag) g.operation=Operation::T;
        if(g.operation==Operation::S) {step(g);step(g);}
        step(g);
      }
      need(std::abs(q.dense()[0]-Complex{1,0})<2e-10,"inverse failed");
    }
    // Generate a roundoff-sized residual by reversing non-Clifford gates.
    // It must remain nonzero and become entangled under a subsequent CX.
    unsigned tiny_cases = 0;
    for (unsigned trial = 0; trial < 100; ++trial) {
      AdaptiveReduced q(2,2);
      std::vector<Operation> ops;
      for (unsigned j = 0; j < 20; ++j) {
        auto op = static_cast<Operation>(random()%6);
        ops.push_back(op); q.apply(op,0);
      }
      for (auto it=ops.rbegin();it!=ops.rend();++it) {
        auto op=*it;
        if(op==Operation::T)op=Operation::TDag;
        else if(op==Operation::TDag)op=Operation::T;
        if(op==Operation::S){q.apply(op,0);q.apply(op,0);}
        q.apply(op,0);
      }
      auto before=q.dense();
      if(before[1]==Complex{} || std::abs(before[1])>1e-12) continue;
      q.apply(Operation::CX,0,1);
      auto after=q.dense();
      need(q.block_count()==1 && after[3]==before[1] && after[1]==Complex{},
           "tiny residual was pruned instead of entangled");
      ++tiny_cases;
    }
    need(tiny_cases>0,"tiny residual fixture not exercised");
    AdaptiveReduced invalid(2);
    auto saved_invalid=invalid.dense();
    bool rejected=false;
    try {invalid.apply(static_cast<Operation>(255),0);} catch(const std::invalid_argument&) {rejected=true;}
    need(rejected && invalid.dense()==saved_invalid && invalid.gate_count()==0,"invalid opcode mutated state");
    std::cout<<"{\"suite\":\"dynamic_refactorization\",\"random_circuits\":400,\"seed\":20261025,\"max_amplitude_error\":"<<worst<<",\"large_uncompute_qubits\":1000,\"bell_partitions\":true,\"tiny_residual_cases\":"<<tiny_cases<<"}\n";
  } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
