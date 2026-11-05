// Signed/unsigned vector min/max at the genuine UMIN stop.
#include "minimal_guest.hpp"
#include <iostream>
using namespace reverse_pc::guest;
namespace {
unsigned checks=0,cases=0,reserved=0;
void check(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
Bytes elf(const std::vector<std::uint32_t>& words,bool system=false){
  Bytes code;for(auto x:words)put(code,x,4);const auto entry=system?0x40080000:0x1000;
  Bytes b{0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
  put(b,2,2);put(b,183,2);put(b,1,4);put(b,entry);put(b,64);put(b,0);
  put(b,0,4);put(b,64,2);put(b,56,2);put(b,1,2);put(b,0,2);put(b,0,2);put(b,0,2);
  put(b,1,4);put(b,7,4);put(b,0x100);put(b,entry);put(b,entry);put(b,code.size());put(b,code.size());put(b,0x100);
  b.resize(0x100);b.insert(b.end(),code.begin(),code.end());return b;
}
MinimalGuest prepared(std::uint64_t low,std::uint64_t high,bool varied,const Vector128* last=nullptr){
  std::vector<std::uint32_t> code{0xd2840002u};
  for(unsigned r=0;r<32;r++)code.push_back(0x3dc00040u|(r<<10)|r);
  code.insert(code.end(),{0xd28000a3u,0xd51b4403u,0xd2840003u,0xd51b4423u,0xf100045fu,0xd503201fu});
  MinimalGuest g(65536);g.load_elf(elf(code));
  for(unsigned r=0;r<32;r++){
    g.write(0x2000+r*16,low^(varied?std::uint64_t(r)*UINT64_C(0x0102040810204081):0),8);
    g.write(0x2008+r*16,high^(varied?std::uint64_t(r)*UINT64_C(0x8040201008040201):0),8);
  }
  if(last){g.write(0x21f0,last->low,8);g.write(0x21f8,last->high,8);}
  g.run(38);return g;
}
std::uint32_t op(unsigned size,bool q,bool uns,unsigned minimum,unsigned rn,unsigned rm,unsigned rd){return (minimum?0x0e206c00u:0x0e206400u)|(q?0x40000000u:0)|(uns?0x20000000u:0)|(size<<22)|(rm<<16)|(rn<<5)|rd;}
Vector128 oracle(const FloatingState& f,unsigned size,bool q,bool uns,unsigned minimum,unsigned rn,unsigned rm,unsigned){
 const unsigned bits=8u<<size;Vector128 want;
 const auto integer=[&](std::uint64_t x){return uns||x<(UINT64_C(1)<<(bits-1))?std::int64_t(x):std::int64_t(x)-std::int64_t(UINT64_C(1)<<bits);};
 for(unsigned n=0;n<(q?128u:64u)/bits;n++){const auto x=integer(f.v[rn].lane(n,bits)),y=integer(f.v[rm].lane(n,bits));want.lane(n,bits,std::uint64_t(minimum?std::min(x,y):std::max(x,y)));}
 return want;
}
void execute(MinimalGuest g,unsigned size,bool upper,bool uns,unsigned operation,unsigned rn,unsigned rm,unsigned rd,bool restore=false){
  g.write(g.registers().pc,op(size,upper,uns,operation,rn,rm,rd),4);
  const auto f=g.floating();const auto c=g.registers();const auto mem=g.runtime().memory_snapshot(),core=g.runtime().core_snapshot();
  const auto want=oracle(f,size,upper,uns,operation,rn,rm,rd);
  if(restore){auto copy=MinimalGuest::from_snapshot(g.snapshot());g.run(1);copy.run(1);check(g.snapshot()==copy.snapshot(),"Resume differs");}else g.run(1);
  check(g.floating().v[rd].low==want.low&&g.floating().v[rd].high==want.high,"Vector min/max differs");
  check(g.registers().pc==c.pc+4&&g.registers().instructions==c.instructions+1,"Wrong instruction boundary");
  check(g.registers().x==c.x&&g.registers().nzcv==c.nzcv,"Integer state changed");
  check(g.floating().fpcr==f.fpcr&&g.floating().fpsr==f.fpsr,"FP flags changed");
  check(g.runtime().memory_snapshot()==mem&&g.runtime().core_snapshot()==core,"XI/Ether state changed");
  for(unsigned r=0;r<32;r++)if(r!=rd)check(g.floating().v[r].low==f.v[r].low&&g.floating().v[r].high==f.v[r].high,"Unrelated vector changed");
  ++cases;
}
void suite(){
  const auto g=prepared(UINT64_C(0xfedcba9876543210),UINT64_C(0x0123456789abcdef),true);
  check(op(2,false,true,1,0,1,0)==0x2ea16c00,"Exact UMIN encoding differs");
  execute(g,2,false,true,1,0,1,0,true);
  for(bool upper:{false,true})for(bool uns:{false,true})for(unsigned operation=0;operation<2;operation++)for(unsigned size=0;size<3;size++){
    for(unsigned rn=0;rn<32;rn++)for(unsigned rm=0;rm<32;rm++)execute(g,size,upper,uns,operation,rn,rm,(rn+rm)%3==0?rn:(rn+rm)%3==1?rm:(rn+rm+1)%32);
    for(unsigned rd=0;rd<32;rd++)execute(g,size,upper,uns,operation,rd,rd,rd,true);
    const unsigned bits=8u<<size;const auto mask=(UINT64_C(1)<<bits)-1;
    for(auto a:{UINT64_C(0),UINT64_C(1),mask,mask>>1,(mask>>1)+1})for(auto b:{UINT64_C(0),UINT64_C(1),mask,mask>>1,(mask>>1)+1}){
      Vector128 low,high;for(unsigned n=0;n<128/bits;n++){low.lane(n,bits,a);high.lane(n,bits,b);}
      auto edge=prepared(low.low,low.high,false,&high);
      execute(edge,size,upper,uns,operation,0,31,31,true);
      execute(prepared(high.low,high.high,false),size,upper,uns,operation,31,31,31,true);
    }
    auto quantum=g;quantum.apply_xi_delta("h 4095\n");execute(quantum,size,upper,uns,operation,0,31,0,true);
    MinimalGuest trap(128*1024*1024);trap.load_elf(elf({op(size,upper,uns,operation,1,5,2)},true),true);trap.run(1);
    check(trap.machine().exceptions==1&&(trap.machine().esr>>26)==7&&!trap.floating().nonzero(),"FP access trap bypassed");
  }
  for(bool upper:{false,true})for(bool uns:{false,true})for(unsigned operation=0;operation<2;operation++)for(unsigned rn=0;rn<32;rn++)for(unsigned rm=0;rm<32;rm++){
    auto bad=g;bad.write(bad.registers().pc,op(3,upper,uns,operation,rn,rm,(rn+rm)%32),4);const auto before=bad.snapshot();bool rejected=false;
    try{bad.run(1);}catch(const std::exception&){rejected=true;}
    check(rejected&&bad.snapshot()==before,"Reserved form executed or changed state");++reserved;
  }
}
}
int main(){try{suite();std::cout<<"{\"checks\":"<<checks<<",\"cases\":"<<cases<<",\"valid_forms\":24,\"reserved_encodings\":"<<reserved<<",\"exact_c16_opcode\":\"0x2ea16c00\",\"android_boot\":false}\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
