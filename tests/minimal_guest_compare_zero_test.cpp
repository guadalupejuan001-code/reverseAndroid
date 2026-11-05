// The genuine C15 CMLT stop and the complete integer compare-with-zero vector family.
#include "minimal_guest.hpp"
#include <cstring>
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
MinimalGuest prepared(std::uint64_t low,std::uint64_t high,bool varied){
  std::vector<std::uint32_t> code{0xd2840002u};
  for(unsigned r=0;r<32;r++)code.push_back(0x3dc00040u|(r<<10)|r);
  code.insert(code.end(),{0xd28000a3u,0xd51b4403u,0xd2840003u,0xd51b4423u,0xf100045fu,0xd503201fu});
  MinimalGuest g(65536);g.load_elf(elf(code));
  for(unsigned r=0;r<32;r++){
    g.write(0x2000+r*16,low^(varied?std::uint64_t(r)*UINT64_C(0x0102040810204081):0),8);
    g.write(0x2008+r*16,high^(varied?std::uint64_t(r)*UINT64_C(0x8040201008040201):0),8);
  }
  g.run(38);return g;
}
std::uint32_t op(unsigned size,bool q,unsigned operation,unsigned rn,unsigned rd){
  const std::uint32_t base[]{0x0e209800u,0x0e208800u,0x2e208800u,0x0e20a800u,0x2e209800u};
  return base[operation]|(q?0x40000000u:0)|(size<<22)|(rn<<5)|rd;
}
Vector128 oracle(const Vector128& v,unsigned size,bool q,unsigned operation){
  const unsigned bits=8u<<size;Vector128 want;
  for(unsigned n=0;n<(q?128u:64u)/bits;n++){
    const auto raw=v.lane(n,bits);std::int64_t x;
    if(bits==64)std::memcpy(&x,&raw,8);else x=raw<(UINT64_C(1)<<(bits-1))?std::int64_t(raw):std::int64_t(raw)-std::int64_t(UINT64_C(1)<<bits);
    const bool yes=operation==0?x==0:operation==1?x>0:operation==2?x>=0:operation==3?x<0:x<=0;
    want.lane(n,bits,yes?UINT64_MAX:0);
  }
  return want;
}
void execute(MinimalGuest g,unsigned size,bool q,unsigned operation,unsigned rn,unsigned rd,bool restore=false){
  g.write(g.registers().pc,op(size,q,operation,rn,rd),4);
  const auto f=g.floating();const auto c=g.registers();const auto mem=g.runtime().memory_snapshot(),core=g.runtime().core_snapshot();
  const auto want=oracle(f.v[rn],size,q,operation);
  if(restore){auto copy=MinimalGuest::from_snapshot(g.snapshot());g.run(1);copy.run(1);check(g.snapshot()==copy.snapshot(),"Resume differs");}else g.run(1);
  check(g.floating().v[rd].low==want.low&&g.floating().v[rd].high==want.high,"Compare-zero lane mask or high clearing differs");
  check(g.registers().pc==c.pc+4&&g.registers().instructions==c.instructions+1,"Wrong instruction boundary");
  check(g.registers().x==c.x&&g.registers().nzcv==c.nzcv,"Integer state changed");
  check(g.floating().fpcr==f.fpcr&&g.floating().fpsr==f.fpsr,"FP flags changed");
  check(g.runtime().memory_snapshot()==mem&&g.runtime().core_snapshot()==core,"XI/Ether state changed");
  for(unsigned r=0;r<32;r++)if(r!=rd)check(g.floating().v[r].low==f.v[r].low&&g.floating().v[r].high==f.v[r].high,"Unrelated vector changed");++cases;
}
void suite(){
  const auto g=prepared(UINT64_C(0xfedcba9876543210),UINT64_C(0x0123456789abcdef),true);
  check(op(0,false,3,3,3)==0x0e20a863,"Exact CMLT opcode differs");execute(g,0,false,3,3,3,true);
  for(unsigned operation=0;operation<5;operation++)for(unsigned size=0;size<4;size++)for(bool q:{false,true}){
    if(size==3&&!q)continue;
    for(unsigned rn=0;rn<32;rn++)for(unsigned rd=0;rd<32;rd++)execute(g,size,q,operation,rn,rd,rn==rd);
    const unsigned bits=8u<<size;const auto mask=bits==64?UINT64_MAX:(UINT64_C(1)<<bits)-1;
    for(auto edge:{UINT64_C(0),UINT64_C(1),mask,mask>>1,(mask>>1)+1}){
      Vector128 v;for(unsigned n=0;n<128/bits;n++)v.lane(n,bits,edge);
      execute(prepared(v.low,v.high,false),size,q,operation,31,31,true);
    }
    auto quantum=g;quantum.apply_xi_delta("h 4095\n");execute(quantum,size,q,operation,31,0,true);
    MinimalGuest trap(128*1024*1024);trap.load_elf(elf({op(size,q,operation,3,3)},true),true);trap.run(1);
    check(trap.machine().exceptions==1&&(trap.machine().esr>>26)==7&&!trap.floating().nonzero(),"FP access trap bypassed");
  }
  for(unsigned operation=0;operation<5;operation++)for(unsigned rn=0;rn<32;rn++)for(unsigned rd=0;rd<32;rd++){
    auto bad=g;bad.write(bad.registers().pc,op(3,false,operation,rn,rd),4);const auto before=bad.snapshot();bool rejected=false;
    try{bad.run(1);}catch(const std::exception&){rejected=true;}check(rejected&&bad.snapshot()==before,"Reserved 1D vector executed or changed state");++reserved;
  }
}
}
int main(){try{suite();std::cout<<"{\"checks\":"<<checks<<",\"cases\":"<<cases<<",\"valid_forms\":35,\"reserved_encodings\":"<<reserved<<",\"exact_c15_opcode\":\"0x0e20a863\",\"android_boot\":false}\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
