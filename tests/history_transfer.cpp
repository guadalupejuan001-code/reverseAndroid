#include "adaptive_reduced.hpp"
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace reverse_pc;
void need(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
int main(){try{
  for(auto op:{Operation::X,Operation::Z,Operation::CX,Operation::CZ}) {
    AdaptiveReduced q(4);
    q.apply(Operation::H,0);q.apply(Operation::T,0);
    auto before=q.dense();
    for(unsigned i=0;i<25000;++i){q.apply(op,0,1);q.apply(op,0,1);}
    need(q.history_size()==2 && q.history_capacity_bytes()<1024,"identity history grew");
    need(q.gate_count()==50002,"gate count lost cancelled instructions");
    auto after=q.dense();for(unsigned i=0;i<after.size();++i)need(std::abs(after[i]-before[i])<1e-13,"identity changed state");
  }
  // H/H and T/Tdg are not removed: they can round in double arithmetic.
  AdaptiveReduced rounded(2);rounded.apply(Operation::H,0);rounded.apply(Operation::H,0);
  rounded.apply(Operation::T,0);rounded.apply(Operation::TDag,0);
  need(rounded.history_size()==4,"rounded inverse pair erased");
  // Force fallback after a LONG noncancelling product history.
  AdaptiveReduced transfer(3,1);
  for(unsigned i=0;i<20000;++i)transfer.apply(Operation::S,0);
  transfer.apply(Operation::H,0);auto before=transfer.dense();
  transfer.apply(Operation::CX,0,1);
  need(transfer.imported_product() && transfer.replayed_gate_count()==0,"product replayed history");
  need(transfer.history_size()==0 && transfer.history_capacity_bytes()==0,"obsolete history capacity retained");
  auto after=transfer.dense();
  need(std::abs(after[0]-before[0])<1e-13 && std::abs(after[3]-before[1])<1e-13,"transfer lost phase/norm");
  // General entangled blocks still use the old replay path.
  AdaptiveReduced entangled(4,2);entangled.apply(Operation::H,0);entangled.apply(Operation::CX,0,1);
  entangled.apply(Operation::T,0);entangled.apply(Operation::CX,1,2);
  need(!entangled.imported_product() && entangled.replayed_gate_count()==3,"entangled transfer contract");
  // Regression for the pinned import factory: its returned object's FUTURE
  // gates must respect the supplied cap as well as the initial import.
  auto limited=Reduced::from_product({{{1.,0.}},{{1.,0.}}},1);
  limited.hadamard(0);auto saved=limited.dense_from_reduced();bool rejected=false;
  try{limited.phase_t(0);}catch(const std::runtime_error&){rejected=true;}
  need(rejected && limited.dense_from_reduced()==saved,"import lost future component cap");
  AdaptiveReduced q(3,1,1);q.apply(Operation::H,0);q.apply(Operation::T,0);
  auto saved_state=q.dense();auto gates=q.gate_count(),history=q.history_size();rejected=false;
  try{q.apply(Operation::CX,0,1);}catch(const std::exception&){rejected=true;}
  need(rejected && !q.uses_original_reduced() && q.dense()==saved_state && q.gate_count()==gates && q.history_size()==history,"failed transfer committed partial state");
  std::cout<<"{\"suite\":\"history_product_transfer\",\"identity_gates\":200000,\"noncancelling_history\":20000,\"direct_product_transfer\":true,\"component_cap_rollback\":true}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
