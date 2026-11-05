#include "minimal_guest.hpp"
#include "minimal_guest_system_demo.hpp"
#include <iostream>
using namespace reverse_pc::guest;
static unsigned checks=0;
static void check(bool ok,const char* why){++checks;require(ok,why);}
static MinimalGuest user(std::uint64_t sctlr,std::uint32_t opcode,std::uint64_t pointer=0x4008400d){
  auto image=system_demo_elf();image.resize(256);
  auto emit=[&](std::uint32_t word){put(image,word,4);};
  auto mov=[&](unsigned rt,std::uint64_t n){for(unsigned i=0;i<4;i++)emit((i?0xf2800000u:0xd2800000u)|(i<<21)|(unsigned((n>>(16*i))&65535)<<5)|rt);};
  auto msr=[&](unsigned id,unsigned rt){emit(0xd5100000u|(id<<5)|rt);};
  mov(0,sctlr);msr(guest_sysreg(3,0,1,0,0),0);
  mov(0,0x40080200);msr(guest_sysreg(3,0,4,0,1),0);
  msr(guest_sysreg(3,0,4,0,0),31);mov(0,pointer);emit(0xd69f03e0);
  image.resize(256+0x200);emit(opcode);
  for(unsigned i=0;i<8;i++){image[96+i]=std::uint8_t(std::uint64_t(image.size()-256)>>(8*i));image[104+i]=image[96+i];}
  MinimalGuest g(128*1024*1024,0);g.load_elf(image,true);g.run(1000,true);
  require(!g.machine().el&&g.registers().pc==0x40080200,"Cache fixture missed the real ERET boundary");return g;
}
int main(){try{
  for(bool enabled:{false,true}){
    auto g=user(enabled?0x4000:0,0xd53b00e5);const auto original=g.snapshot();
    auto restored=MinimalGuest::from_snapshot(original);g.run(1);restored.run(1);
    check(!g.machine().el&&g.registers().x[5]==(enabled?4:20),"EL0 DCZID did not report its native DZE policy");
    check(g.registers().pc==0x40080204,"DCZID failed to execute its original instruction");
    check(restored.snapshot()==g.snapshot(),"DCZID checkpoint continuation differs");
    check(g.runtime().core_snapshot()==restored.runtime().core_snapshot(),"DCZID changed the original quantum authority");
  }
  auto zero=user(0x4000,0xd50b7420);const auto block=UINT64_C(0x40084000);
  for(unsigned i=0;i<64;i++)zero.write(block+i,0xa5,1);
  zero.write(block-1,0x77,1);zero.write(block+64,0x88,1);
  const auto before=zero.snapshot();auto resume=MinimalGuest::from_snapshot(before);
  const auto steps=zero.registers().instructions;zero.run(1);resume.run(1);
  check(!zero.machine().el&&zero.registers().pc==0x40080204&&zero.registers().instructions==steps+1,"Allowed EL0 DC ZVA did not execute exactly once");
  for(unsigned i=0;i<64;i++)check(zero.read(block+i,1)==0,"DC ZVA did not zero every original XI/Ether byte");
  check(zero.read(block-1,1)==0x77&&zero.read(block+64,1)==0x88,"DC ZVA wrote outside its aligned 64-byte block");
  check(zero.snapshot()==resume.snapshot(),"DC ZVA direct and resumed complete snapshots differ");
  auto denied=user(0,0xd50b7420);denied.write(block,0x42,1);const auto denied_count=denied.machine().exceptions;
  denied.run(1);
  check(denied.machine().el==1&&denied.machine().exceptions==denied_count+1&&denied.machine().elr==0x40080200,"DZE denial did not reach the actual EL1 exception vector");
  check(denied.machine().esr==UINT64_C(0x6212dc08)&&denied.read(block,1)==0x42,"DZE trap lost SYS syndrome fields or modified memory");
  auto ctr=user(0x8000,0xd53b0028);ctr.run(1);
  check(!ctr.machine().el&&ctr.registers().x[8]==0x8444c004,"Allowed user CTR did not preserve this CPU's cache profile");
  auto ctr_denied=user(0,0xd53b0028);ctr_denied.run(1);
  check(ctr_denied.machine().el==1&&ctr_denied.machine().esr==UINT64_C(0x6232c101)&&ctr_denied.machine().elr==0x40080200,"UCT trap lost MRS identity, Rt or direction");
  auto midr=user(0xc000,0xd5380008);const auto x8=midr.registers().x[8];midr.run(1);
  check(midr.machine().el==1&&midr.machine().esr==(1u<<25)&&midr.registers().x[8]==x8,"Cache access gave EL0 direct privileged MIDR access");
  auto invalid=user(0x4000,0xd50b7420,0x80000000);invalid.write(block,0x93,1);invalid.run(1);
  check(invalid.machine().el==1&&invalid.machine().esr==UINT64_C(0x92000050)&&invalid.read(block,1)==0x93,"DC ZVA ignored native address faults or modified an unrelated block");
  auto device=user(0x4000,0xd50b7420,MachineState::uart_base);const auto output=device.run(1);
  check(device.machine().el==1&&device.machine().esr==UINT64_C(0x92000061)&&output.output.empty(),"DC ZVA zeroed device memory instead of taking an alignment fault");
  std::cout<<"{\"checks\":"<<checks<<",\"el0_dczid\":true,\"el0_dc_zva\":true,\"kernel_permissions_preserved\":true,\"native_system_trap_fields\":true,\"full_state_resume\":true}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
