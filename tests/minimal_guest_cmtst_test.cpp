// C25 actual CMTST stop, independently checked per bit with CMEQ regression.
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
  code.insert(code.end(),{0xd2a01803u,0xd51b4403u,0xd2800203u,0xd51b4423u,0xf100045fu,0xd503201fu});
  MinimalGuest g(65536);g.load_elf(elf(code));
  for(unsigned r=0;r<32;r++){
    g.write(0x2000+r*16,low^(varied?std::uint64_t(r)*UINT64_C(0x0102040810204081):0),8);
    g.write(0x2008+r*16,high^(varied?std::uint64_t(r)*UINT64_C(0x8040201008040201):0),8);
  }
  if(last){g.write(0x21f0,last->low,8);g.write(0x21f8,last->high,8);}
  g.run(38);return g;
}
std::uint32_t op(unsigned size,bool q,bool equality,unsigned rn,unsigned rm,unsigned rd){
  return 0x0e208c00u|(q?0x40000000u:0)|(equality?0x20000000u:0)|(size<<22)|(rm<<16)|(rn<<5)|rd;
}
Vector128 oracle(const FloatingState& f,unsigned size,bool q,bool equality,unsigned rn,unsigned rm,unsigned){
  const unsigned bits=8u<<size,count=(q?128u:64u)/bits;Vector128 expected;
  for(unsigned lane=0;lane<count;lane++){
    const auto x=f.v[rn].lane(lane,bits),y=f.v[rm].lane(lane,bits);bool overlap=false;
    for(unsigned bit=0;bit<bits;bit++)if((x>>bit)%2&&(y>>bit)%2)overlap=true;
    expected.lane(lane,bits,(equality?x==y:overlap)?UINT64_MAX:0);
  }
  return expected;
}
void execute(MinimalGuest g,unsigned size,bool q,bool equality,unsigned rn,unsigned rm,unsigned rd,bool restore=false){
  g.write(g.registers().pc,op(size,q,equality,rn,rm,rd),4);
  const auto f=g.floating();const auto c=g.registers();const auto mem=g.runtime().memory_snapshot(),core=g.runtime().core_snapshot();
  const auto want=oracle(f,size,q,equality,rn,rm,rd);
  if(restore){auto copy=MinimalGuest::from_snapshot(g.snapshot());g.run(1);copy.run(1);check(g.snapshot()==copy.snapshot(),"Resume differs");}else g.run(1);
  check(g.floating().v[rd].low==want.low&&g.floating().v[rd].high==want.high,"Integer vector predicate differs");
  check(g.registers().pc==c.pc+4&&g.registers().instructions==c.instructions+1,"Wrong instruction boundary");
  check(g.registers().x==c.x&&g.registers().nzcv==c.nzcv,"Integer state changed");
  check(g.floating().fpcr==f.fpcr&&g.floating().fpsr==f.fpsr,"FP flags changed");
  check(g.runtime().memory_snapshot()==mem&&g.runtime().core_snapshot()==core,"XI/Ether state changed");
  for(unsigned r=0;r<32;r++)if(r!=rd)check(g.floating().v[r].low==f.v[r].low&&g.floating().v[r].high==f.v[r].high,"Unrelated vector changed");
  ++cases;
}
void suite(){
  const auto g=prepared(UINT64_C(0xfedcba9876543210),UINT64_C(0x0123456789abcdef),true);
  check(op(0,true,false,3,3,7)==0x4e238c67,"Exact C25 CMTST encoding differs");
  check(g.floating().fpcr==0x00c00000&&g.floating().fpsr==16,"Nontrivial FP flags were not seeded");
  execute(g,0,true,false,3,3,7,true);
  for(bool q:{false,true})for(bool equality:{false,true})for(unsigned size=0;size<4;size++){
    if(size==3&&!q)continue;
    for(unsigned rn=0;rn<32;rn++)for(unsigned rm=0;rm<32;rm++)execute(g,size,q,equality,rn,rm,(rn+rm)%3==0?rn:(rn+rm)%3==1?rm:(rn+rm+1)%32);
    for(unsigned rd=0;rd<32;rd++)execute(g,size,q,equality,rd,rd,rd,true);
    const unsigned bits=8u<<size;const auto mask=bits==64?UINT64_MAX:(UINT64_C(1)<<bits)-1,sign=UINT64_C(1)<<(bits-1);
    for(auto a:{UINT64_C(0),UINT64_C(1),UINT64_C(2),mask,sign,sign|1})for(auto b:{UINT64_C(0),UINT64_C(1),UINT64_C(2),mask,sign,sign|1}){
      Vector128 low,high;for(unsigned n=0;n<128/bits;n++){low.lane(n,bits,a);high.lane(n,bits,b);}
      execute(prepared(low.low,low.high,false,&high),size,q,equality,0,31,31,true);
      execute(prepared(high.low,high.high,false),size,q,equality,31,31,31,true);
    }
    auto quantum=g;quantum.apply_xi_delta("h 4095\n");execute(quantum,size,q,equality,0,31,0,true);
    MinimalGuest trap(128*1024*1024);trap.load_elf(elf({op(size,q,equality,1,5,2)},true),true);trap.run(1);
    check(trap.machine().exceptions==1&&(trap.machine().esr>>26)==7&&!trap.floating().nonzero(),"FP access trap bypassed");
  }
  for(bool equality:{false,true})for(unsigned rn=0;rn<32;rn++)for(unsigned rm=0;rm<32;rm++){
    auto bad=g;bad.write(bad.registers().pc,op(3,false,equality,rn,rm,(rn+rm)%32),4);const auto before=bad.snapshot();bool rejected=false;
    try{bad.run(1);}catch(const std::exception&){rejected=true;}
    check(rejected&&bad.snapshot()==before,"Reserved form executed or changed state");++reserved;
  }
}
}
int main(){try{suite();std::cout<<"{\"checks\":"<<checks<<",\"cases\":"<<cases<<",\"valid_forms\":14,\"reserved_encodings\":"<<reserved<<",\"exact_c25_opcode\":\"0x4e238c67\",\"android_boot\":false}\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
