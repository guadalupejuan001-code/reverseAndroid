#include "minimal_guest.hpp"
#include <iostream>
using namespace reverse_pc::guest;
namespace {
unsigned checks=0,cases=0,reserved=0;
void check(bool yes,const char* why){++checks;require(yes,why);}
Bytes elf(const std::vector<std::uint32_t>& words,bool system){
 Bytes code;for(auto x:words)put(code,x,4);const auto entry=system?0x40080000:0x1000;
 Bytes b{0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
 put(b,2,2);put(b,183,2);put(b,1,4);put(b,entry);put(b,64);put(b,0);put(b,0,4);put(b,64,2);put(b,56,2);put(b,1,2);put(b,0,2);put(b,0,2);put(b,0,2);
 put(b,1,4);put(b,7,4);put(b,0x100);put(b,entry);put(b,entry);put(b,code.size());put(b,code.size());put(b,0x100);b.resize(0x100);b.insert(b.end(),code.begin(),code.end());return b;
}
MinimalGuest prepared(bool system=false){
 std::vector<std::uint32_t> code;for(unsigned r=0;r<31;r++)code.push_back(0xd2e00000u|((0xf000+r)<<5)|r);
 code.insert(code.end(),{0x9100043fu,0xf100041fu,0xd503201fu}); // Unmapped, unaligned SP, nonzero flags.
 MinimalGuest g(system?128*1024*1024:65536);g.load_elf(elf(code,system),system);g.run(33);return g;
}
void execute(MinimalGuest g,std::uint32_t opcode,bool restore=false){
 g.write(g.registers().pc,opcode,4);const auto cpu=g.registers();const auto fp=g.floating();auto machine=g.machine();
 const auto memory=g.runtime().memory_snapshot(),core=g.runtime().core_snapshot();
 if(restore){auto copy=MinimalGuest::from_snapshot(g.snapshot());g.run(1);copy.run(1);check(g.snapshot()==copy.snapshot(),"Prefetch resume differs");}else g.run(1);
 check(g.registers().instructions==cpu.instructions+1&&g.registers().pc==cpu.pc+4,"Hint did not retire exactly once");
 check(g.registers().x==cpu.x&&g.registers().sp==cpu.sp&&g.registers().nzcv==cpu.nzcv,"Hint altered an architectural operand");
 check(g.floating().save()==fp.save(),"Hint altered FP/SIMD state");
 if(machine.enabled)machine.ticks+=machine.counter_latency.instruction_tick();
 check(g.machine().save()==machine.save(),"Hint accessed a device, faulted, or changed exclusive/clock state");
 check(g.runtime().memory_snapshot()==memory&&g.runtime().core_snapshot()==core,"Hint altered XI/Ether memory or core");++cases;
}
void reject(MinimalGuest g,std::uint32_t opcode){g.write(g.registers().pc,opcode,4);const auto before=g.snapshot();bool failed=false;try{g.run(1);}catch(const std::exception&){failed=true;}check(failed&&g.snapshot()==before,"Reserved prefetch form executed or modified state");++reserved;}
void suite(){
 const auto g=prepared(),system=prepared(true);execute(g,0xf8a86a80,true);execute(system,0xf8a86a80,true);
 for(unsigned hint=0;hint<32;hint++){
  for(unsigned rn=0;rn<32;rn++){
   for(unsigned imm:{0u,1u,2048u,4095u})execute(g,0xf9800000u|(imm<<10)|(rn<<5)|hint);
   for(int imm:{-256,-1,0,1,255})execute(g,0xf8800000u|((unsigned(imm)&511)<<12)|(rn<<5)|hint);
  }
  for(int imm:{-262144,-1,0,1,262143})execute(g,0xd8000000u|((unsigned(imm)&0x7ffff)<<5)|hint);
  for(unsigned option:{2u,3u,6u,7u})for(unsigned scale=0;scale<2;scale++){
   const auto op=0xf8a00800u|(8u<<16)|(option<<13)|(scale<<12)|(20u<<5)|hint;execute(g,op,true);execute(system,op);
  }
 }
 for(unsigned rn=0;rn<32;rn++)for(unsigned rm=0;rm<32;rm++)for(unsigned option:{2u,3u,6u,7u})for(unsigned scale=0;scale<2;scale++){
  execute(g,0xf8a00800u|(rm<<16)|(option<<13)|(scale<<12)|(rn<<5));
 }
 for(unsigned rn=0;rn<32;rn++)for(unsigned option:{0u,1u,4u,5u})for(unsigned hint=0;hint<32;hint++)reject(g,0xf8a00800u|(8u<<16)|(option<<13)|(rn<<5)|hint);
 for(unsigned mode:{1u,2u,3u})for(unsigned rn=0;rn<32;rn++)reject(g,0xf8800000u|(mode<<10)|(rn<<5));
 reject(g,0xf9c00000u); // Reserved integer unsigned load, not PRFM.
 auto quantum=g;quantum.apply_xi_delta("h 4095\n");execute(quantum,0xf8a86a80,true);
 // Ordinary data instructions must still fault on these invalid base addresses.
 reject(g,0xf9400020u);reject(g,0xf9000020u);
}
}
int main(){try{suite();std::cout<<"{\"checks\":"<<checks<<",\"cases\":"<<cases<<",\"reserved_or_faulting_forms\":"<<reserved<<",\"hint_forms\":4,\"exact_c18_opcode\":\"0xf8a86a80\",\"android_boot\":false}\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
