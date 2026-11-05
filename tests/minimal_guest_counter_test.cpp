#include "minimal_guest_counter_demo.hpp"
#include "minimal_guest_pci_demo.hpp"
#include "minimal_guest_system_demo.hpp"
#include "guest_boot_cycle.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
using namespace reverse_pc::guest;
static unsigned checks=0;static void check(bool v,const char* why){++checks;require(v,why);}
static void write(const std::string& path,const Bytes& b){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(b.data()),b.size());require(bool(out),"Cannot save counter test evidence");}
int main(){try{
  // Independent Node crypto.createHmac goldens are checked against this
  // complete emitted stream, not just a test of whether numbers vary.
  GuestCounterLatency c;c.enabled=true;c.seed=counter_test_seed();std::vector<unsigned> stream;
  for(unsigned i=0;i<64;i++)stream.push_back(c.next());
  auto encoded=c.save();check(encoded.size()==48,"Counter finite state extent changed");
  auto roundtrip=GuestCounterLatency::restore(encoded);check(roundtrip.reads==64&&roundtrip.seed==c.seed,"Counter source was not restored");
  check(c.next()==roundtrip.next(),"Counter source changed after cache reconstruction");
  encoded[0]=3;bool rejected=false;try{GuestCounterLatency::restore(encoded);}catch(const std::exception&){rejected=true;}
  check(rejected,"Unknown counter source version was accepted");
  auto legacy=MinimalGuest(128*1024*1024,0);legacy.load_linux_image(counter_witness_image());const auto legacy_start=legacy.snapshot();legacy.run(1000);
  check(legacy.registers().halted&&!legacy.machine().counter_latency.enabled,"Historical counter mode changed");
  for(unsigned i=1;i<64;i++)check(legacy.read(0x44000000+i*8,8)-legacy.read(0x44000000+(i-1)*8,8)==2,"Legacy counter timing is not byte-compatible");
  check(MinimalGuest::from_snapshot(legacy_start).snapshot()==legacy_start,"Old machine state changed on restore");
  auto g=counter_witness();const auto source=g.snapshot();const auto result=g.run(1000);
  check(g.registers().halted&&g.machine().counter_latency.reads==64,"Real counter MRS instructions did not execute");
  check(g.registers().x[2]==1000000,"Architectural counter frequency changed");
  check(result.output=="REVERSE: COUNTER LATENCY OK\n","Counter witness UART changed");
  unsigned varied=0;std::uint64_t expected=6;
  for(unsigned i=0;i<64;i++){
    expected+=stream[i];check(g.read(0x44000000+i*8,8)==expected,"Counter latency differs from its persistent stream");
    if(i&&g.read(0x44000000+i*8,8)-g.read(0x44000000+(i-1)*8,8)!=2)++varied;
    expected+=2;
  }
  check(varied>32,"Counter samples did not contain bounded timing variation");
  auto sliced=MinimalGuest::from_snapshot(source);
  for(unsigned i=0;i<200&&!sliced.registers().halted;i++){
    sliced.run(7);const auto middle=sliced.snapshot();sliced=MinimalGuest::from_snapshot(middle);
  }
  check(sliced.snapshot()==g.snapshot(),"Sliced execution and restored cache changed the full guest state");
  check(MinimalGuest::from_snapshot(g.snapshot()).snapshot()==g.snapshot(),"Machine v6 restore was not byte-identical");
  const auto before=g.snapshot();bool duplicate=false;try{g.enable_counter_latency(counter_test_seed());}catch(const std::exception&){duplicate=true;}
  check(duplicate&&g.snapshot()==before,"A running entropy seed was silently replaced");
  const auto statistics=g.statistics();check(statistics.find("counter_samples\":64")!=std::string::npos,"Counter provenance is absent");
  check(g.snapshot()==before,"Read-only counter inspection mutated the guest");
  MinimalGuest system(128*1024*1024,0);system.load_elf(system_demo_elf(),true);system.enable_counter_latency(counter_test_seed());
  const auto timer=system.run(10000);
  check(timer.halted&&timer.exit_code==0&&timer.output=="REVERSE: EL1 MMU IRQ EL0 SVC FAULT OK\n","Counter latency broke actual MMU/timer/IRQ/ERET execution");
  check(MinimalGuest::from_snapshot(system.snapshot()).snapshot()==system.snapshot(),"Non-PCI v6 did not restore");
  MinimalGuest pci(128*1024*1024,0);pci.load_linux_image(pci_graphics_witness_image(),{},"",{},true);pci.enable_counter_latency(counter_test_seed());
  pci.run(1000);check(pci.display_rgb().size()==96*64*3,"PCI display changed with the new clock state");
  check(MinimalGuest::from_snapshot(pci.snapshot()).snapshot()==pci.snapshot(),"PCI v6 did not restore");
  check(GuestSha256::hash(g.snapshot())=="2f6204418f5c0b40b75c16be93b485067c954c449d796d60846a0c584e0d1fd6","Historical v6 counter witness changed bytes");
  auto upgraded=counter_witness();upgraded.run(11);const auto old_state=upgraded.snapshot();const auto old_pc=upgraded.registers().pc,old_steps=upgraded.registers().instructions;
  const auto old_ticks=upgraded.machine().ticks,old_reads=upgraded.machine().counter_latency.reads;const auto old_seed=upgraded.machine().counter_latency.seed;
  upgraded.configure_counter_rate(64);const auto upgraded_state=upgraded.snapshot();
  check(upgraded.registers().pc==old_pc&&upgraded.registers().instructions==old_steps&&upgraded.machine().ticks==old_ticks&&upgraded.machine().counter_latency.reads==old_reads&&upgraded.machine().counter_latency.seed==old_seed,"Cadence upgrade reset CPU time or the entropy stream");
  const auto header=8+(old_state[3]=='3'?44u:43u)*8;const auto mutable_at=header+get(old_state,8+39*8,8)+get(old_state,8+40*8,8)+get(old_state,8+41*8,8);
  check(std::equal(old_state.begin()+header,old_state.begin()+mutable_at,upgraded_state.begin()+header),"Cadence upgrade changed immutable image, original core or XI memory bytes");
  auto upgraded_restart=MinimalGuest::from_snapshot(upgraded_state);upgraded.run(1000);upgraded_restart.run(1000);
  check(upgraded_restart.snapshot()==upgraded.snapshot(),"Mid-execution cadence upgrade did not restart exactly");
  for(const auto invalid_rate:{1u,4097u}){auto invalid=counter_witness();const auto saved=invalid.snapshot();bool invalid_rejected=false;
    try{invalid.configure_counter_rate(invalid_rate);}catch(const std::exception&){invalid_rejected=true;}check(invalid_rejected&&invalid.snapshot()==saved,"Invalid cadence configuration changed the guest");}
  auto rate=counter_prescaled_witness();const auto rate_source=rate.snapshot();const auto rate_run=rate.run(1000);
  check(rate_run.halted&&rate_run.output==result.output,"Prescaled counter did not execute the actual ARM witness");
  check(rate.registers().x[2]==1000000&&rate.machine().counter_latency.reads==64,"Counter frequency or MRS count changed");
  std::uint64_t latency_sum=0;for(unsigned i=0;i<64;i++){
    latency_sum+=stream[i];check(rate.read(0x44000000+i*8,8)==(6+2*i)/64+latency_sum,"Prescaled counter samples differ from exact instruction cadence");
  }
  check(rate.machine().ticks==rate.registers().instructions/64+latency_sum,"Prescaled total ticks differ");
  check(rate.machine().counter_latency.subtick==rate.registers().instructions%64,"Fractional counter phase was lost");
  check(rate.machine().save()[0]==7&&rate.machine().counter_latency.save().size()==64,"Prescaled clock did not select machine v7");
  auto rate_split=MinimalGuest::from_snapshot(rate_source);
  for(unsigned i=0;i<200&&!rate_split.registers().halted;i++){rate_split.run(7);rate_split=MinimalGuest::from_snapshot(rate_split.snapshot());}
  check(rate_split.snapshot()==rate.snapshot(),"Prescaled restart changed the seed, cursor, phase or complete state");
  auto phase=counter_prescaled_witness();phase.run(11);const auto phase_state=phase.snapshot();
  check(phase.machine().counter_latency.subtick==11,"Counter phase did not follow actual CPU steps");
  check(MinimalGuest::from_snapshot(phase_state).snapshot()==phase_state,"Nonzero v7 phase changed on restore");
  bool rate_rejected=false;try{phase.configure_counter_rate(128);}catch(const std::exception&){rate_rejected=true;}
  check(rate_rejected&&phase.snapshot()==phase_state,"A persisted counter cadence was silently replaced");
  for(unsigned field:{48u,56u}){
    auto invalid=rate.machine().counter_latency.save();for(unsigned i=0;i<8;i++)invalid[field+i]=0;
    if(field==56)invalid[field]=64;
    bool invalid_rejected=false;try{GuestCounterLatency::restore(invalid);}catch(const std::exception&){invalid_rejected=true;}
    check(invalid_rejected,"Invalid v7 cadence or phase was accepted");
  }
  MinimalGuest paced_timer(128*1024*1024,0);paced_timer.load_elf(system_demo_elf(),true);paced_timer.enable_counter_latency(counter_test_seed());paced_timer.configure_counter_rate(64);
  const auto paced_source=paced_timer.snapshot();const auto paced_run=paced_timer.run(10000);
  check(paced_run.halted&&paced_run.output=="REVERSE: EL1 MMU IRQ EL0 SVC FAULT OK\n","Prescaled timer/IRQ/ERET execution failed");
  auto paced_split=MinimalGuest::from_snapshot(paced_source);BootCycleLimits paced_limits;paced_limits.instructions=10000;paced_limits.chunk=1;paced_limits.wall_ms=30000;
  const auto paced_report=run_boot_cycle(paced_split,paced_limits);
  check(paced_report.reason=="halted"&&paced_split.snapshot()==paced_timer.snapshot(),"Prescaled timer wakeup and one-step chunks changed the full state");
  check(paced_report.console==paced_run.output,"Prescaled timer lost real UART output");
  MinimalGuest paced_pci(128*1024*1024,0);paced_pci.load_linux_image(pci_graphics_witness_image(),{},"",{},true);paced_pci.enable_counter_latency(counter_test_seed());paced_pci.configure_counter_rate(64);paced_pci.run(1000);
  check(paced_pci.display_rgb()==pci.display_rgb(),"Counter cadence changed actual PCI display pixels");
  check(MinimalGuest::from_snapshot(paced_pci.snapshot()).snapshot()==paced_pci.snapshot(),"Prescaled PCI v7 did not restore exactly");
  std::filesystem::create_directories("guest-evidence");write("guest-evidence/counter-start.rvg",source);write("guest-evidence/counter-finished.rvg",g.snapshot());
  write("guest-evidence/counter-rate-start.rvg",rate_source);write("guest-evidence/counter-rate-finished.rvg",rate.snapshot());
  std::cout<<"{\"checks\":"<<checks<<",\"mode\":\"test-seed-only\",\"samples\":64,\"legacy_unchanged\":true,\"same_state_after_restart\":true,\"expected_finished_sha256\":"<<quote(GuestSha256::hash(g.snapshot()))<<",\"latencies\":[";
  for(unsigned i=0;i<stream.size();i++){if(i)std::cout<<',';std::cout<<stream[i];}
  std::cout<<"],\"counter_instructions_per_tick\":64,\"expected_prescaled_finished_sha256\":"<<quote(GuestSha256::hash(rate.snapshot()))<<",\"android_boot\":false}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
