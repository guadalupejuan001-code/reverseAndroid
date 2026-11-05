#pragma once
#include "guest_boot_cycle.hpp"
namespace reverse_pc::guest {
// Host scheduling only: retain the same CPU, memory, devices and clock between budgets.
inline BootCycleReport run_boot_budget_cycles(MinimalGuest& guest,const BootCycleLimits& limits,unsigned cycles,
    const CycleSave& save={},const std::function<void(const MinimalGuest&,const BootCycleReport&)>& progress={},
    const std::function<bool()>& interrupted={},unsigned* completed=nullptr){
  require(cycles&&cycles<=4,"Expected one to four bounded cycles");
  require(!limits.stop_after_first_user_instruction,"A continuous budget cannot fabricate an entry observation");
  const auto initial=guest.registers().instructions;
  require(limits.instructions<=(UINT64_MAX-initial)/cycles,"Combined instruction budget overflows");
  const auto started=std::chrono::steady_clock::now();std::string previous_console;std::uint64_t previous_chunks=0;
  BootCycleReport total;if(completed)*completed=0;
  for(unsigned cycle=0;cycle<cycles;cycle++){
    const auto aggregate=[&](const BootCycleReport& r){
      require(r.console.size()<=4*1024*1024-previous_console.size(),"Combined UART exceeds evidence bound");
      auto full=r;full.initial_instructions=initial;full.console=previous_console+r.console;full.chunks+=previous_chunks;
      full.elapsed_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();return full;
    };
    const auto saver=[&](const MinimalGuest& g,const BootCycleReport& r){if(save)save(g,aggregate(r));};
    const auto observer=[&](const MinimalGuest& g,const BootCycleReport& r){if(progress)progress(g,aggregate(r));};
    const auto r=run_boot_cycle(guest,limits,saver,{},observer,interrupted);total=aggregate(r);if(completed)++*completed;
    const bool budget=r.reason=="instruction_budget"||r.reason=="host_wall_budget";
    if(!budget||!r.error.empty()||guest.registers().halted||guest.machine().waiting||(interrupted&&interrupted()))break;
    previous_console=total.console;previous_chunks=total.chunks;
  }
  return total;
}
}
