#include "guest_console_input.hpp"
#include <iostream>
using namespace reverse_pc::guest;
int main(){try{
  Bytes kernel(4096);const auto field=[&](unsigned at,std::uint64_t n,unsigned size){for(unsigned i=0;i<size;++i)kernel.at(at+i)=std::uint8_t(n>>(i*8));};
  field(0,0xd503201f,4);field(8,0x80000,8);field(16,kernel.size(),8);field(56,0x644d5241,4);
  MinimalGuest guest(2147483648,0);guest.load_linux_image(kernel,{},"console=ttyAMA0",{},true);
  std::array<std::uint8_t,32> seed{};seed[0]=7;guest.enable_counter_latency(seed);guest.configure_counter_rate(64);
  const auto before=guest.snapshot();const auto registers=guest.registers();
  const auto core=guest.runtime().core_snapshot(),memory=guest.runtime().memory_persistent_snapshot();
  require(guest_console_input_event(guest,{}).empty()&&guest.snapshot()==before,"Absent input changed the original state");
  const std::string command="\n/system/bin/input keyevent 3\n";const Bytes data(command.begin(),command.end());
  auto expected=guest.machine();expected.receive+=command;
  const auto event=guest_console_input_event(guest,data);
  require(guest.machine().save()==expected.save(),"Input changed machine state beyond the explicit receive queue");
  require(guest.runtime().core_snapshot()==core&&guest.runtime().memory_persistent_snapshot()==memory,"Input overwrote guest RAM, properties or quantum state");
  const auto& now=guest.registers();require(now.x==registers.x&&now.pc==registers.pc&&now.sp==registers.sp&&now.instructions==registers.instructions&&now.nzcv==registers.nzcv&&now.halted==registers.halted&&now.exit_code==registers.exit_code,"Input fabricated CPU execution");
  require(event.find(GuestSha256::hash(data))!=std::string::npos&&event.find("\"instructions\":0")!=std::string::npos,"Missing actual input byte/instruction binding");
  const auto saved=guest.snapshot();auto restored=MinimalGuest::from_snapshot_view(saved,{},MinimalGuest::max_adaptive_snapshot);
  require(restored.snapshot()==saved&&restored.machine().receive==command,"Original input-state roundtrip failed");
  std::cout<<"{\"absent_input_byte_identical\":true,\"explicit_receive_queue_only\":true,\"RAM_properties_and_quantum_state_unchanged\":true,\"CPU_execution_not_fabricated\":true,\"original_snapshot_roundtrip\":true}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
