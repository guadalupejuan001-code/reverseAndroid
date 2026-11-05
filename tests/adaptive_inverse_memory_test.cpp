// Exact semantics for REVERSE's opt-in adaptive InverseMemory authority.
#include "mundo_x/quantum_runtime.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
using mundo_x::QuantumRuntime;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F>void fails(F&& operation,const char* message){
  bool rejected=false;
  try{operation();}catch(const std::exception&){rejected=true;}
  check(rejected,message);
}
void numeric_read_semantics(){
  QuantumRuntime::MemoryLimits limits;limits.max_overrides=0;
  QuantumRuntime runtime(QuantumRuntime::Background::Zero,QuantumRuntime::CoreLimits{},limits);
  runtime.transact_bytes([](auto& bytes){
    bytes.store("0",17);bytes.store("4095",19);bytes.store("4096",23);
    bytes.store("18446744073709551615",29);
    bytes.store("18446744073709551616",31);
    for(std::uint64_t address:{UINT64_C(0),UINT64_C(1),UINT64_C(4095),UINT64_C(4096),UINT64_MAX}){
      const auto text=bytes.read(std::to_string(address)),numeric=bytes.read(address);
      check(text.deterministic==numeric.deterministic&&text.value==numeric.value&&text.handle==numeric.handle,
            "Numeric transaction read differs from the original canonical address");
    }
  });
  const auto saved=runtime.memory_persistent_snapshot(),core=runtime.core_snapshot();
  for(std::uint64_t address:{UINT64_C(0),UINT64_C(1),UINT64_C(4095),UINT64_C(4096),UINT64_MAX}){
    const auto a=runtime.memory().read(std::to_string(address),runtime.core());
    const auto b=runtime.memory().read_numeric(address,runtime.core());
    check(a.deterministic==b.deterministic&&a.value==b.value&&a.handle==b.handle,
          "Numeric memory read differs at a page or uint64 boundary");
  }
  check(runtime.memory().read("18446744073709551616",runtime.core()).value==31,
        "Numeric lookup altered an arbitrary-precision address");
  check(runtime.memory_persistent_snapshot()==saved&&runtime.core_snapshot()==core,
        "Numeric reads changed the original core or memory authority");
  const auto background=mundo_x::InverseMemory::from_snapshot("MXIM1 165\nO 3 0\n",limits);
  check(background.read_numeric(3,runtime.core()).value==0&&
        background.read_numeric(4,runtime.core()).value==165,
        "Numeric read confused a zero override and a nonzero global rule");
  runtime.execute("mem_bind 42 900\nh 900\n");
  const auto quantum_core=runtime.core_snapshot(),quantum_memory=runtime.memory_persistent_snapshot();
  const auto qtext=runtime.memory().read("42",runtime.core());
  const auto qnumeric=runtime.memory().read_numeric(42,runtime.core());
  check(!qtext.deterministic&&!qnumeric.deterministic&&qtext.handle==qnumeric.handle,
        "Numeric read invented a classical byte for an unresolved quantum binding");
  check(runtime.memory().read_numeric(4096,runtime.core()).value==23,
        "An unrelated quantum binding changed a classical numeric read");
  check(runtime.core_snapshot()==quantum_core&&runtime.memory_persistent_snapshot()==quantum_memory,
        "Numeric read collapsed or changed an original quantum binding");
  runtime.execute("h 900\nx 901\n");
  check(runtime.memory().read_numeric(42,runtime.core()).deterministic&&
        runtime.memory().read_numeric(42,runtime.core()).value==2,
        "Numeric read lost a deterministic original quantum byte");
  limits.max_address_digits=1;
  QuantumRuntime narrow(QuantumRuntime::Background::Zero,QuantumRuntime::CoreLimits{},limits);
  check(narrow.memory().read_numeric(9,narrow.core()).value==0,
        "Numeric read rejected a permitted small canonical address");
  fails([&]{narrow.memory().read_numeric(10,narrow.core());},
        "Numeric read bypassed the original logical-address digit budget");
}
}
int main(){try{
  numeric_read_semantics();
  using Runtime=QuantumRuntime;
  // The old bounded MXIM1 representation is stable even though its private
  // index now uses sparse/dense pages; no historical byte or revision changes.
  Runtime legacy;
  legacy.execute("mem_store 3 1\nmem_store 8 9\n");
  check(legacy.memory_snapshot()=="MXIM1 0\nO 3 1\nO 8 9\n",
        "Bounded legacy InverseMemory snapshot bytes changed");
  auto legacy_saved=legacy.memory_persistent_snapshot();
  Runtime legacy_copy;legacy_copy.restore(legacy.core_snapshot(),legacy_saved);
  check(legacy_copy.memory_persistent_snapshot()==legacy_saved,
        "Legacy snapshot roundtrip is not identical");
  Runtime::MemoryLimits bounded;
  bounded.max_overrides=2;
  Runtime quota(Runtime::Background::Zero,Runtime::CoreLimits{},bounded);
  fails([&]{quota.transact_bytes([](auto& access){
    access.store("1",1);access.store("2",2);access.store("3",3);
  });},"Explicit legacy override quotas must still reject excess writes");
  check(quota.memory().stats().overrides==0,"Failed limited transaction mutated state");

  Runtime::MemoryLimits limits;
  limits.max_overrides=0; // 0 means no arbitrary byte-count ceiling.
  Runtime guest(Runtime::Background::Zero,Runtime::CoreLimits{},limits);
  constexpr std::size_t count=1080000;
  guest.transact_bytes([&](auto& access){
    for(std::size_t i=0;i<count;++i)
      access.store(std::to_string(UINT64_C(0x40000000)+i),
                   std::uint8_t(i%251+1));
    access.store("18446744073709551616",173); // >uint64 canonical address
  });
  check(guest.memory().stats().overrides==count+1,
        "Adaptive InverseMemory did not pass the old 1,048,576 ceiling");
  const auto saved=guest.memory_persistent_snapshot();
  check(saved.find("\nMXIM3 0\n")!=std::string::npos,
        "Unbounded InverseMemory did not select its compact MXIM3 format");
  check(saved.size()<8000000,"Compact 1.08M-byte memory snapshot is too large");
  const auto core=guest.core_snapshot();
  Runtime restored(Runtime::Background::Zero,Runtime::CoreLimits{},limits);
  restored.restore(core,saved);
  check(restored.memory_persistent_snapshot()==saved,
        "Unbounded MXIM3 snapshot bytes/revision changed after restoration");
  for(auto i:{0u,1u,127u,128u,4095u,4096u,1048575u,1079999u}){
    const auto v=restored.memory().read(std::to_string(UINT64_C(0x40000000)+i),restored.core());
    check(v.deterministic&&v.value==std::uint8_t(i%251+1),
          "Adaptive page byte differs at a page boundary");
  }
  check(restored.memory().read("18446744073709551616",restored.core()).value==173,
        "Arbitrary-precision logical addresses were lost");
  const auto reference=restored.memory_persistent_snapshot();
  fails([&]{restored.transact_bytes([](auto& access){
    access.store("1073741824",0);access.store("999999999999999999999999",4);
    throw std::runtime_error("intentional rollback");
  });},"Failed adaptive transaction was committed");
  check(restored.memory_persistent_snapshot()==reference,
        "Failed transaction changed exact adaptive memory state");
  restored.execute("mem_bind 42424242 900\n");
  check(restored.memory().stats().quantum_bindings==1,
        "Explicit original quantum binding was not preserved");
  const auto quantum_saved=restored.memory_persistent_snapshot();
  Runtime reloaded(Runtime::Background::Zero,Runtime::CoreLimits{},limits);
  reloaded.restore(restored.core_snapshot(),quantum_saved);
  check(reloaded.memory_persistent_snapshot()==quantum_saved &&
        reloaded.memory().stats().quantum_bindings==1,
        "Adaptive memory lost quantum binding metadata");
  auto invalid=quantum_saved;
  const auto at=invalid.find("\nP ");
  check(at!=std::string::npos,"Expected an actual adaptive page record");
  const auto mask=invalid.find(' ',invalid.find(' ',at+3)+1);
  check(mask!=std::string::npos,"Expected adaptive bitmap");
  invalid[mask-1]='z';
  const auto before=reloaded.memory_persistent_snapshot();
  fails([&]{reloaded.restore(reloaded.core_snapshot(),invalid);},
        "Corrupted compact adaptive page record was accepted");
  check(reloaded.memory_persistent_snapshot()==before,
        "Corrupted compact snapshot mutated runtime");
  std::cout<<"{\"adaptive\":true,\"exact_overrides\":"<<count+1
           <<",\"memory_snapshot_bytes\":"<<saved.size()
           <<",\"legacy_snapshot_identical\":true,\"quantum_bindings_preserved\":true"
           <<",\"rollback\":true,\"arbitrary_precision_addresses\":true}\n";
  return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
