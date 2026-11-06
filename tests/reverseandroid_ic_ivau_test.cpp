#include "minimal_guest.hpp"
#include "minimal_guest_system_demo.hpp"
#include <iostream>
#ifdef __aarch64__
#include <sys/mman.h>
#include <unistd.h>
#endif
using namespace reverse_pc::guest;
namespace {
unsigned checks=0,cases=0,hardware_cases=0;
void check(bool ok,const char* why){++checks;require(ok,why);}
MinimalGuest user(std::uint64_t sctlr,unsigned rt,std::uint64_t pointer,std::uint32_t opcode=0){
  auto image=system_demo_elf();image.resize(256);
  auto emit=[&](std::uint32_t w){put(image,w,4);};
  auto mov=[&](unsigned r,std::uint64_t n){for(unsigned i=0;i<4;++i)emit((i?0xf2800000u:0xd2800000u)|(i<<21)|(unsigned((n>>(i*16))&65535)<<5)|r);};
  auto msr=[&](unsigned id,unsigned r){emit(0xd5100000u|(id<<5)|r);};
  mov(0,sctlr);msr(guest_sysreg(3,0,1,0,0),0);
  mov(0,0x40080200);msr(guest_sysreg(3,0,4,0,1),0);
  mov(0,0xa0001000);msr(guest_sysreg(3,0,4,0,0),0);
  if(rt<31)mov(rt,pointer);
  emit(0xd69f03e0);image.resize(256+0x200);emit(opcode?opcode:0xd50b7520u|rt);
  for(unsigned i=0;i<8;++i){image[96+i]=std::uint8_t(std::uint64_t(image.size()-256)>>(8*i));image[104+i]=image[96+i];}
  MinimalGuest g(128*1024*1024,0);g.load_elf(image,true);g.run(1000,true);
  check(!g.machine().el&&g.registers().pc==0x40080200,"Real ERET boundary missing");return g;
}
void permitted(unsigned rt,unsigned offset){
  auto g=user(UINT64_C(1)<<26,rt,0x40084000+offset);
  const auto cpu=g.registers();const auto fp=g.floating();const auto machine=g.machine();
  const auto mem=g.runtime().memory_persistent_snapshot(),core=g.runtime().core_snapshot();
  auto resumed=MinimalGuest::from_snapshot(g.snapshot());g.run(1);resumed.run(1);
  check(!g.machine().el&&g.machine().exceptions==machine.exceptions,"Allowed IC IVAU trapped");
  check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"IC IVAU retirement changed");
  check(g.registers().x==cpu.x&&g.registers().sp==cpu.sp&&g.registers().nzcv==cpu.nzcv,"IC IVAU changed GPR/flags");
  check(g.runtime().memory_persistent_snapshot()==mem&&g.runtime().core_snapshot()==core,"IC IVAU wrote XI/Ether");
  check(g.floating().fpcr==fp.fpcr&&g.floating().fpsr==fp.fpsr,"IC IVAU changed FP state");
  for(unsigned i=0;i<32;++i)check(g.floating().v[i].low==fp.v[i].low&&g.floating().v[i].high==fp.v[i].high,"IC IVAU changed SIMD");
  check(g.snapshot()==resumed.snapshot(),"Original full-state restore changed IC IVAU");++cases;
}
void hardware(){
#ifdef __aarch64__
  const auto n=std::size_t(sysconf(_SC_PAGESIZE));
  auto* p=static_cast<std::uint32_t*>(mmap(nullptr,n,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
  check(p!=MAP_FAILED,"Native ARM JIT page allocation failed");
  p[0]=0x528000e0u;p[1]=0xd65f03c0u;
  auto sync=[&]{asm volatile("dc cvau, %0\n\tdsb ish\n\tmov x9, %0\n\t.inst 0xd50b7529\n\tdsb ish\n\tisb"::"r"(p):"x9","memory");};
  sync();auto fn=reinterpret_cast<int(*)()>(p);check(fn()==7,"Native ARM initial JIT code failed");
  p[0]=0x52800540u;sync();check(fn()==42,"Native exact IC IVAU did not publish replacement code");
  check(munmap(p,n)==0,"Native ARM page release failed");++hardware_cases;
#endif
}
}
int main(int argc,char** argv){try{
  if(argc==2&&std::string(argv[1])=="--baseline"){
    auto g=user(UINT64_C(1)<<26,9,0x4008400d);g.run(1);
    check(g.machine().el==1&&g.machine().esr==(1u<<25)&&g.machine().elr==0x40080200,"Baseline did not reproduce exact erroneous EL0 undefined trap");
    std::cout<<"{\"actual_exact_IC_IVAU_erroneously_undefined_with_UCI_enabled\":true}\n";return 0;
  }
  for(unsigned rt=0;rt<32;++rt)for(unsigned offset=0;offset<64;++offset)permitted(rt,offset);
  for(unsigned rt=0;rt<32;++rt){
    auto g=user(0,rt,0x4008400d);const auto before=g.machine().exceptions;g.run(1);
    const auto expected=UINT64_C(0x6212dc0a)|(std::uint64_t(rt)<<5); // SYS ISS: op0=1, op1=3, CRn=7, CRm=5, op2=1, direction=write.
    check(g.machine().el==1&&g.machine().exceptions==before+1&&g.machine().elr==0x40080200,"UCI denial missed actual EL1 vector");
    check(g.machine().esr==expected,"UCI denial lost exact SYS syndrome/Rt");
  }
  for(unsigned word:{0xd50b7500u,0xd50b7569u,0xd50b7b29u,0xd508751fu}){
    auto g=user(UINT64_C(1)<<26,0,0x4008400d,word);g.run(1);
    check(g.machine().el==1&&g.machine().esr==(1u<<25),"Exact IC fix enabled another EL0 SYS operation");
  }
  // Real guest store, cache operation and subsequent fetch of changed code.
  auto g=user(UINT64_C(1)<<26,9,0x40080300);
  g.write(0x40080200,0xb9000120u,4); // str w0,[x9]
  g.write(0x40080204,0xd50b7529u,4);
  g.write(0x40080208,0x1400003eu,4); // b 0x40080300
  g.write(0x40080300,0x528000e1u,4); // old mov w1,#7
  // Load the new opcode through ordinary guest instructions before the store.
  g.write(0x40080200,0xd280a800u,4); // mov x0,#0x540
  g.write(0x40080204,0xf2aa5000u,4); // movk x0,#0x5280,lsl#16 => mov w0,#42
  g.write(0x40080208,0xb9000120u,4);
  g.write(0x4008020c,0xd50b7529u,4);
  g.write(0x40080210,0x1400003cu,4); // b 0x40080300
  auto copy=MinimalGuest::from_snapshot(g.snapshot());g.run(6);copy.run(6);
  check(!g.machine().el&&g.registers().x[0]==42&&g.snapshot()==copy.snapshot(),"Guest JIT store/cache/fetch response failed");
  hardware();std::cout<<"{\"IC_IVAU_cases\":"<<cases<<",\"checks\":"<<checks<<",\"real_AArch64_hardware_cases\":"<<hardware_cases<<",\"all_Rt_and_unaligned_offsets\":true,\"UCI_traps_exact_SYS_syndrome\":true,\"neighbor_privileged_SYS_still_rejected\":true,\"original_snapshot_roundtrip_and_guest_JIT_response\":true}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
