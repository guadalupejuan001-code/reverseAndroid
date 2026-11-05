// Exact C13 UMAXV stop. Generated instruction tests, not Android boot evidence.
#include "minimal_guest.hpp"
#include <iostream>
using namespace reverse_pc::guest;
namespace {
unsigned checks=0,cases=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
Bytes elf(const std::vector<std::uint32_t>& words,bool system=false){
  Bytes code;for(auto x:words)put(code,x,4);const auto entry=system?0x40080000:0x1000;
  Bytes b{0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
  put(b,2,2);put(b,183,2);put(b,1,4);put(b,entry);put(b,64);put(b,0);
  put(b,0,4);put(b,64,2);put(b,56,2);put(b,1,2);put(b,0,2);put(b,0,2);put(b,0,2);
  put(b,1,4);put(b,7,4);put(b,0x100);put(b,entry);put(b,entry);put(b,code.size());put(b,code.size());put(b,0x100);
  b.resize(0x100);b.insert(b.end(),code.begin(),code.end());return b;
}
MinimalGuest prepared(std::uint64_t low,std::uint64_t high,bool varied){
  std::vector<std::uint32_t> code{0xd2840002u};
  for(unsigned n=0;n<32;n++)code.push_back(0x3dc00040u|(n<<10)|n);
  // Preserve nonzero NZCV, FPCR and FPSR across the integer instruction.
  code.insert(code.end(),{0xd28000a3u,0xd51b4403u,0xd2840003u,0xd51b4423u,0xf100045fu,0xd503201fu});
  MinimalGuest g(65536);g.load_elf(elf(code));
  for(unsigned r=0;r<32;r++){
    g.write(0x2000+r*16,low^(varied?std::uint64_t(r)*UINT64_C(0x0102040810204081):0),8);
    g.write(0x2008+r*16,high^(varied?std::uint64_t(r)*UINT64_C(0x8040201008040201):0),8);
  }
  g.run(38);return g;
}

std::uint64_t oracle(const Vector128& v,unsigned size,bool q,bool u,bool minimum){
  const unsigned bits=8u<<size,lanes=(q?128:64)/bits;
  auto interpret=[&](std::uint64_t x){return u||x<(UINT64_C(1)<<(bits-1))?std::int64_t(x):std::int64_t(x)-std::int64_t(UINT64_C(1)<<bits);};
  auto best=interpret(v.lane(0,bits));
  for(unsigned n=1;n<lanes;n++){const auto x=interpret(v.lane(n,bits));best=minimum?std::min(best,x):std::max(best,x);}
  return std::uint64_t(best)&((UINT64_C(1)<<bits)-1);
}
std::uint32_t op(unsigned size,bool q,bool u,bool minimum,unsigned rn,unsigned rd){return 0x0e30a800u|(q?0x40000000u:0)|(u?0x20000000u:0)|(minimum?0x10000u:0)|(size<<22)|(rn<<5)|rd;}
void execute(MinimalGuest g,unsigned size,bool q,bool u,bool minimum,unsigned rn,unsigned rd,bool restore=false){
  g.write(g.registers().pc,op(size,q,u,minimum,rn,rd),4);
  const auto f=g.floating();const auto c=g.registers();const auto mem=g.runtime().memory_snapshot(),core=g.runtime().core_snapshot();
  const auto expected=oracle(f.v[rn],size,q,u,minimum);
  if(restore){auto copy=MinimalGuest::from_snapshot(g.snapshot());g.run(1);copy.run(1);check(g.snapshot()==copy.snapshot(),"Resume differs");}else g.run(1);
  check(g.floating().v[rd].low==expected&&g.floating().v[rd].high==0,"Min/max across result or scalar clearing differs");
  check(g.registers().pc==c.pc+4&&g.registers().instructions==c.instructions+1,"Wrong instruction boundary");
  check(g.registers().x==c.x&&g.registers().nzcv==c.nzcv,"Integer state changed");
  check(g.floating().fpcr==f.fpcr&&g.floating().fpsr==f.fpsr,"FP flags changed");
  check(g.runtime().memory_snapshot()==mem&&g.runtime().core_snapshot()==core,"XI/Ether state changed");
  for(unsigned n=0;n<32;n++)if(n!=rd)check(g.floating().v[n].low==f.v[n].low&&g.floating().v[n].high==f.v[n].high,"Unrelated vector changed");++cases;
}
void suite(){
 const auto g=prepared(UINT64_C(0xfedcba9876543210),UINT64_C(0x0123456789abcdef),true);
 execute(g,0,false,true,false,3,2,true); // Genuine C13 fault: UMAXV B2,V3.8B.
 for(bool u:{false,true})for(bool minimum:{false,true})for(unsigned size=0;size<3;size++)for(bool q:{false,true}){
  if(size==2&&!q)continue;
  for(unsigned rn=0;rn<32;rn++)for(unsigned rd=0;rd<32;rd++)execute(g,size,q,u,minimum,rn,rd,rn==rd);
  const unsigned bits=8u<<size,lanes=(q?128:64)/bits;const auto mask=(UINT64_C(1)<<bits)-1;
  for(unsigned pos=0;pos<lanes;pos++)for(auto edge:{UINT64_C(0),UINT64_C(1),mask,mask>>1,(mask>>1)+1}){
   Vector128 input;for(unsigned n=0;n<128/bits;n++)input.lane(n,bits,n==pos?edge:mask>>2);
   execute(prepared(input.low,input.high,false),size,q,u,minimum,31,31,true);
  }
  auto quantum=g;quantum.apply_xi_delta("h 4095\n");execute(quantum,size,q,u,minimum,31,0,true);
  MinimalGuest trap(128*1024*1024);trap.load_elf(elf({op(size,q,u,minimum,3,2)},true),true);trap.run(1);
  check(trap.machine().exceptions==1&&(trap.machine().esr>>26)==7&&!trap.floating().nonzero(),"FP access trap bypassed");
 }
 for(bool u:{false,true})for(bool minimum:{false,true})for(const auto arrangement:std::array<std::pair<unsigned,bool>,3>{{{2,false},{3,false},{3,true}}})for(unsigned rn=0;rn<32;rn++)for(unsigned rd=0;rd<32;rd++){
  MinimalGuest bad(65536);bad.load_elf(elf({op(arrangement.first,arrangement.second,u,minimum,rn,rd)}));const auto before=bad.snapshot();bool rejected=false;
  try{bad.run(1);}catch(const std::exception&){rejected=true;}check(rejected&&bad.snapshot()==before,"Reserved form executed or changed state");
 }
}
}
int main(){try{suite();std::cout<<"{\"checks\":"<<checks<<",\"cases\":"<<cases<<",\"valid_forms\":20,\"reserved_encodings\":12288,\"exact_c13_opcode\":\"0x2e30a862\",\"android_boot\":false}\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
