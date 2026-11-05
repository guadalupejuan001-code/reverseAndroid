#include "guest_budget_cycles.hpp"
#include "minimal_guest_pci_demo.hpp"
#include <iostream>
using namespace reverse_pc::guest;
static MinimalGuest prepared(){MinimalGuest g(128*1024*1024,0);g.load_linux_image(pci_graphics_witness_image(),{},"",{},true);return g;}
int main(){try{
 unsigned checks=0;auto check=[&](bool yes,const char* why){++checks;require(yes,why);};
 BootCycleLimits limits;limits.instructions=100;limits.wall_ms=30000;limits.chunk=17;limits.checkpoint_instructions=70;
 auto g=prepared(),direct=g;direct.run(300);unsigned cycles=0,saves=0;std::uint64_t last=0;
 auto r=run_boot_budget_cycles(g,limits,3,[&](const MinimalGuest& s,const BootCycleReport& report){
   check(report.initial_instructions==0,"Cycle reset the original counter");check(s.registers().instructions>=last,"Counter regressed");
   check(report.console.size()<=4*1024*1024,"Console bound lost");last=s.registers().instructions;++saves;
 },{}, {},&cycles);
 check(cycles==3&&r.reason=="instruction_budget"&&g.snapshot()==direct.snapshot(),"Separate budgets changed CPU, devices or XI state");
 check(g.registers().instructions==300&&r.initial_instructions==0&&saves>=6,"Combined progress is inaccurate");
 auto halt=prepared(),halt_reference=halt;limits.instructions=1000;halt_reference.run(4000);
 r=run_boot_budget_cycles(halt,limits,4,{},{},{},&cycles);
 check(cycles==1&&r.reason=="halted"&&halt.snapshot()==halt_reference.snapshot(),"A terminal state was resumed");
 check(r.console=="REVERSE: PCI GPU 2D OK\n","Combined UART lost or duplicated real output");
 auto bad_image=pci_graphics_witness_image();for(unsigned n=0;n<4;n++)bad_image[256+13*4+n]=255;
 MinimalGuest bad(128*1024*1024,0);bad.load_linux_image(bad_image,{},"",{},true);auto before=bad;before.run(14);
 r=run_boot_budget_cycles(bad,limits,3,{},{},{},&cycles);
 check(cycles==1&&r.reason=="architectural_stop"&&bad.snapshot()==before.snapshot(),"An architectural failure was restarted or modified");
 auto stopped=prepared();const auto input=stopped.snapshot();
 r=run_boot_budget_cycles(stopped,limits,3,{},{},[]{return true;},&cycles);
 check(cycles==1&&r.reason=="host_interrupted"&&stopped.snapshot()==input,"A host interruption changed or restarted the guest");
 bool rejected=false;try{run_boot_budget_cycles(stopped,limits,0);}catch(const std::exception&){rejected=true;}check(rejected&&stopped.snapshot()==input,"Zero-cycle request was accepted");
 std::cout<<"{\"checks\":"<<checks<<",\"continuous_state_exact\":true,\"terminal_states_not_resumed\":true,\"android_boot\":false}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
