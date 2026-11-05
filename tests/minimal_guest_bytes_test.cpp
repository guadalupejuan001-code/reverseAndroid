#include "minimal_guest.hpp"
#include <chrono>
#include <iostream>
using namespace reverse_pc::guest;
using Runtime=mundo_x::QuantumRuntime;
namespace {
unsigned checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
template<class Action>void fails(Action action,const char* why){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,why);}
void equal(const Runtime& a,const Runtime& b){check(a.core_snapshot()==b.core_snapshot(),"Native-byte core differs from XI");check(a.memory_persistent_snapshot()==b.memory_persistent_snapshot(),"Native-byte memory or revision differs from XI");}
std::uint64_t ns(std::chrono::steady_clock::time_point before){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-before).count();}
}
int main(){try{
  Runtime reference,typed;reference.execute("h 4095\nmem_bind 99 100\n");typed=reference;
  // A bound deterministic byte updates the original core; later reads in the
  // same transaction must see those flips, not a stale published authority.
  typed.transact_bytes([&](auto& bytes){bytes.store("99",173);check(bytes.read("99").value==173,"Transaction read does not see its own quantum byte store");});
  reference.execute("mem_store 99 173\n");equal(reference,typed);
  const auto core=typed.core_snapshot(),memory=typed.memory_persistent_snapshot();
  fails([&]{typed.transact_bytes([&](auto& bytes){bytes.store("99",44);bytes.store("123",22);throw std::runtime_error("reject");});},"Throwing transaction accepted");
  check(typed.core_snapshot()==core&&typed.memory_persistent_snapshot()==memory,"Failed transaction leaked classical bytes or quantum flips");
  typed.execute("mem_bind 50 4095\n");reference=typed;
  fails([&]{typed.transact_bytes([&](auto& bytes){bytes.store("123",22);bytes.store("50",0);});},"Superposed quantum byte was implicitly collapsed");equal(reference,typed);
  mundo_x::InverseMemory::Limits limits;limits.max_overrides=2;Runtime bounded(Runtime::Background::Zero,Runtime::CoreLimits{},limits);
  fails([&]{bounded.transact_bytes([](auto& bytes){bytes.store("1",1);bytes.store("2",2);bytes.store("3",3);});},"Override budget ignored");
  check(bounded.memory().stats().overrides==0&&bounded.memory().stats().revision==0,"Rejected budget transaction left partial state");
  std::uint64_t seed=9;auto random=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
  for(unsigned block=0;block<100;++block){std::vector<std::pair<std::string,std::uint8_t>> stores;std::string program;
    for(unsigned i=0;i<17;++i){const auto address=std::to_string(1000+random()%700),value=std::to_string(random()%256);stores.emplace_back(address,std::uint8_t(std::stoi(value)));program+="mem_store "+address+" "+value+"\n";}
    typed.transact_bytes([&](auto& bytes){for(const auto& p:stores)bytes.store(p.first,p.second);});reference.execute(program);equal(reference,typed);
  }
  Runtime old_path,new_path;constexpr unsigned count=12000;
  auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<count;++i)old_path.execute("mem_store "+std::to_string(i)+" "+std::to_string(i%255+1)+"\n");const auto old_ns=ns(start);
  start=std::chrono::steady_clock::now();new_path.transact_bytes([](auto& bytes){for(unsigned i=0;i<count;++i)bytes.store(std::to_string(i),std::uint8_t(i%255+1));});const auto new_ns=ns(start);equal(old_path,new_path);
  new_path.transact_bytes([](auto& bytes){for(unsigned i=0;i<count;++i)bytes.store(std::to_string(i),0);});
  check(new_path.memory().stats().overrides==0,"Restoring the zero background retained redundant differences");
  std::cout<<"{\"checks\":"<<checks<<",\"compared_stores\":13700,\"benchmark_stores\":"<<count<<",\"xi_per_store_ns\":"<<old_ns<<",\"native_block_ns\":"<<new_ns<<",\"speedup_this_host\":"<<double(old_ns)/new_ns<<",\"authority\":\"original QuantumRuntime/InverseMemory\",\"snapshot_and_revision\":\"identical\"}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
