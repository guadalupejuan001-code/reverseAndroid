// Opt-in numeric stores must produce the exact original string-API authorities.
#include "mundo_x/quantum_runtime.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <stdexcept>
using R=mundo_x::QuantumRuntime;using M=mundo_x::InverseMemory;
namespace {
unsigned checks=0,operations=0;
void check(bool v){++checks;if(!v)throw std::runtime_error("Numeric store differs from original canonical-address semantics");}
void equal(const R&a,const R&b){check(a.core_snapshot()==b.core_snapshot());check(a.memory_persistent_snapshot()==b.memory_persistent_snapshot());check(a.memory().stats().revision==b.memory().stats().revision);}
template<class F>std::string error(F f){try{f();return {};}catch(const std::exception&x){return x.what();}}
R::MemoryLimits limits(){R::MemoryLimits x;x.max_overrides=0;return x;}
void sweeps(){
 for(unsigned quota:{0u,65536u}){auto l=limits();l.max_overrides=quota;R a(R::Background::Zero,R::CoreLimits{},l),b(R::Background::Zero,R::CoreLimits{},l);
  a.execute("mem_store 18446744073709551616 173\n");b.execute("mem_store 18446744073709551616 173\n");
  std::uint64_t random=0x71c5418600;auto next=[&](){random^=random<<13;random^=random>>7;random^=random<<17;return random;};
  for(unsigned epoch=0;epoch<64;++epoch){std::vector<std::pair<std::uint64_t,std::uint8_t>> ops;
   for(unsigned i=0;i<2048;++i){auto r=next();std::uint64_t address=i%3==0?(UINT64_C(2)<<60)+(r&8191):UINT64_C(0x40000000)+(r&8191);if(i%128==0)address=UINT64_MAX-(i%64);ops.push_back({address,std::uint8_t(i%5?r&255:0)});}
   auto apply=[&](R&x,bool numeric){return error([&]{x.transact_bytes([&](auto&bytes){for(auto [address,value]:ops){if(numeric)bytes.store(address,value);else bytes.store(std::to_string(address),value);}});});};
   check(apply(a,false)==apply(b,true));operations+=ops.size();equal(a,b);
   R copy(R::Background::Zero,R::CoreLimits{},l);copy.restore(b.core_snapshot(),b.memory_persistent_snapshot());equal(a,copy);
  }
  auto saved=a.memory_persistent_snapshot(),core=a.core_snapshot();R restored(R::Background::Zero,R::CoreLimits{},l);restored.restore(core,saved);
  restored.transact_bytes([](auto&bytes){for(std::uint64_t at=0;at<12288;++at)bytes.store(UINT64_C(0x40000000)+at,0);});check(a.memory_persistent_snapshot()==saved&&a.core_snapshot()==core);
 }
}
void limits_and_quantum(){
 for(unsigned digits:{1u,10u,19u,20u})for(unsigned quota:{0u,1u,2u}){auto l=limits();l.max_address_digits=digits;l.max_overrides=quota;R a(R::Background::Zero,R::CoreLimits{},l),b(R::Background::Zero,R::CoreLimits{},l);
  for(std::uint64_t at:{UINT64_C(0),UINT64_C(9),UINT64_C(10),UINT64_C(9999999999),UINT64_C(10000000000),UINT64_C(9999999999999999999),UINT64_MAX}){
   const auto x=error([&]{a.transact_bytes([&](auto&v){v.store(std::to_string(at),173);});}),y=error([&]{b.transact_bytes([&](auto&v){v.store(at,173);});});check(x==y);equal(a,b);
  }
 }
 for(bool superposed:{false,true}){auto l=limits();R a(R::Background::Zero,R::CoreLimits{},l),b(R::Background::Zero,R::CoreLimits{},l);const std::string prepare="mem_bind 42 900\nmem_bind 43 908\n"+std::string(superposed?"h 900\n":"x 901\n");a.execute(prepare);b.execute(prepare);
  const auto beforea=a.core_snapshot(),beforeb=a.memory_persistent_snapshot();
  for(std::uint64_t at:{UINT64_C(42),UINT64_C(43),UINT64_C(4096)}){const auto x=error([&]{a.transact_bytes([&](auto&v){v.store(std::to_string(at),173);});}),y=error([&]{b.transact_bytes([&](auto&v){v.store(at,173);});});check(x==y);if(at==42)check(superposed?!x.empty():x.empty());equal(a,b);}
  if(superposed){check(!a.memory().read("42",a.core()).deterministic);check(a.memory_persistent_snapshot()!=beforeb);}
  const auto ca=a.core_snapshot(),ma=a.memory_persistent_snapshot();
  const auto x=error([&]{a.transact_bytes([&](auto&v){v.store("43",99);throw std::runtime_error("rollback");});}),y=error([&]{b.transact_bytes([&](auto&v){v.store(UINT64_C(43),99);throw std::runtime_error("rollback");});});check(x==y);equal(a,b);check(a.core_snapshot()==ca&&a.memory_persistent_snapshot()==ma);
 }
}
void threaded_lookup_coherence(){
 R base;auto core=base.core();M memory(0,limits());std::atomic<unsigned> phase{0},ready{0};std::atomic<bool> ok{true};std::vector<M> parents;
 auto reader=[&](){for(unsigned round=0;round<128;++round){while(phase.load(std::memory_order_acquire)!=round+1)std::this_thread::yield();const auto value=memory.read_numeric(4096,core);if(!value.deterministic||value.value!=(round%2?0:173))ok=false;for(unsigned i=0;i<128;++i)if(memory.read_numeric(4096,core).value!=value.value)ok=false;ready.fetch_add(1,std::memory_order_release);while(phase.load(std::memory_order_acquire)==round+1)std::this_thread::yield();}};
 std::thread one(reader),two(reader);
 for(unsigned round=0;round<128;++round){if(round%3==0)parents.push_back(memory);memory.store_numeric(4096,std::uint8_t(round%2?0:173),core);ready=0;phase.store(round+1,std::memory_order_release);while(ready.load(std::memory_order_acquire)!=2)std::this_thread::yield();phase.store(0,std::memory_order_release);}
 one.join();two.join();check(ok.load());
}
void background_and_revision(){
 R base;auto ca=base.core(),cb=base.core();auto l=limits();M a(165,l),b(165,l);
 for(unsigned epoch=0;epoch<4;++epoch){for(std::uint64_t at=0;at<8192;++at){const auto value=std::uint8_t(epoch%2?165:at%251);a.store(std::to_string(at),value,ca);b.store_numeric(at,value,cb);}check(a.persistent_snapshot()==b.persistent_snapshot());check(ca.snapshot()==cb.snapshot());}
 for(unsigned value:{0u,1u,2u}){const auto text="MXIM2 18446744073709551615\nMXIM1 0\nO 7 1\n";auto x=M::from_snapshot(text,l),y=M::from_snapshot(text,l);const auto initial=x.persistent_snapshot();
  const auto ex=error([&]{x.store("7",std::uint8_t(value),ca);}),ey=error([&]{y.store_numeric(7,std::uint8_t(value),cb);});check(ex==ey);check((value==1)==ex.empty());check(x.persistent_snapshot()==y.persistent_snapshot());check(x.persistent_snapshot()==initial);
 }
}
}
int main(){try{sweeps();limits_and_quantum();threaded_lookup_coherence();background_and_revision();std::cout<<"{\"checks\":"<<checks<<",\"paired_operations\":"<<operations<<",\"original_authority_bytes_equal\":true,\"encoding_and_decoder_changed\":false}\n";}catch(const std::exception&x){std::cerr<<x.what()<<'\n';return 1;}}
