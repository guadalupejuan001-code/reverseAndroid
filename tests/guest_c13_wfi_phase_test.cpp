#include "minimal_guest_system_demo.hpp"
#include "minimal_guest_counter_demo.hpp"
#include <iostream>
using namespace reverse_pc::guest;
// Bare-metal fixture, not an Android execution. Proves why the persisted
// counter phase must not be inferred as instructions modulo 64 after WFI.
int main(){try{
 MinimalGuest g(128*1024*1024,0);g.load_elf(system_demo_elf(),true);g.enable_counter_latency(counter_test_seed());g.configure_counter_rate(64);
 const auto run=g.run(10000);const auto phase=g.machine().counter_latency.subtick,n=g.registers().instructions;
 require(run.halted,"Fixture didn't finish");require(phase!=n%64,"Fixture didn't expose WFI clock phase reset");
 require(MinimalGuest::from_snapshot(g.snapshot()).snapshot()==g.snapshot(),"State changed on restore");
 std::cout<<"{\"fixture\":\"native_wfi_timer_irq\",\"instructions\":"<<n<<",\"actual_subtick\":"<<phase<<",\"instruction_remainder\":"<<(n%64)<<",\"state_roundtrip\":true,\"android_boot\":false}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
