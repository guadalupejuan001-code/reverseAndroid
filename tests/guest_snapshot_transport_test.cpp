#include "minimal_guest.hpp"
#include "minimal_guest_pci_demo.hpp"
#include <iostream>
using namespace reverse_pc::guest;
int main(){try{
 unsigned checks=0;auto check=[&](bool b,const char* why){++checks;require(b,why);};
 MinimalGuest base(128*1024*1024,0);base.load_linux_image(pci_graphics_witness_image(),{},"",{},true);const auto input=base.snapshot();
 check(base.host_snapshot_transport_capacity()==256*1024*1024,"Default transport changed");
 auto explicit_guest=MinimalGuest::from_snapshot(input,{},MinimalGuest::max_explicit_adaptive_snapshot);
 check(explicit_guest.host_snapshot_transport_capacity()==1024ull*1024*1024,"Explicit bound lost during restore");
 check(explicit_guest.snapshot()==input,"Host policy changed serialized guest bytes");
 auto tiny=MinimalGuest::from_snapshot(input,{},input.size());check(tiny.snapshot()==input,"Exact transport boundary rejected");
 base.run(300);explicit_guest.run(300);tiny.run(300);
 check(base.snapshot()==explicit_guest.snapshot(),"Host transport policy changed CPU, memory or devices");
 bool failed=false;try{tiny.snapshot();}catch(const std::exception& e){failed=std::string(e.what()).find("configured host transport capacity")!=std::string::npos;}
 check(failed&&tiny.registers().instructions==300,"Transport failure changed the running guest or exceeded its bound");
 tiny.set_host_snapshot_transport_capacity(MinimalGuest::max_explicit_adaptive_snapshot);
 check(tiny.snapshot()==base.snapshot(),"Increasing only host transport changed actual guest state");
 auto ordinary=MinimalGuest::from_snapshot(explicit_guest.snapshot());
 check(ordinary.host_snapshot_transport_capacity()==MinimalGuest::max_adaptive_snapshot,"Host policy leaked into RVG");
 check(ordinary.snapshot()==base.snapshot(),"Default restore changed the guest");
 for(const auto capacity:{std::size_t(0),std::size_t(4095),MinimalGuest::max_explicit_adaptive_snapshot+1}){
  failed=false;try{ordinary.set_host_snapshot_transport_capacity(capacity);}catch(const std::exception&){failed=true;}check(failed,"Invalid transport setter accepted");
  failed=false;try{MinimalGuest::from_snapshot(input,{},capacity);}catch(const std::exception&){failed=true;}check(failed,"Invalid restore transport accepted");
 }
 failed=false;try{MinimalGuest::from_snapshot(input,{},input.size()-1);}catch(const std::exception&){failed=true;}check(failed,"Oversized transport input accepted");
 check(ordinary.snapshot()==base.snapshot(),"Rejected policy modified existing state");
 std::cout<<"{\"checks\":"<<checks<<",\"default_256m_preserved\":true,\"host_policy_not_serialized\":true,\"guest_state_exact\":true}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
