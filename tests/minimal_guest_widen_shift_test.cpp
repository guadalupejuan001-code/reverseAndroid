// Regression for C08's genuine 0x2f10a442 stop. Exercises the original XI/Ether CPU.
#include "minimal_guest.hpp"
#include <iostream>
using namespace reverse_pc::guest;
namespace {
unsigned checks=0,cases=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
Bytes elf(std::initializer_list<std::uint32_t> words,bool system=false){
  Bytes code;for(auto x:words)put(code,x,4);const auto entry=system?0x40080000:0x1000;
  Bytes b{0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
  put(b,2,2);put(b,183,2);put(b,1,4);put(b,entry);put(b,64);put(b,0);
  put(b,0,4);put(b,64,2);put(b,56,2);put(b,1,2);put(b,0,2);put(b,0,2);put(b,0,2);
  put(b,1,4);put(b,7,4);put(b,0x100);put(b,entry);put(b,entry);put(b,code.size());put(b,code.size());put(b,0x100);
  b.resize(0x100);b.insert(b.end(),code.begin(),code.end());return b;
}
std::uint64_t oracle(std::uint64_t x,unsigned bits,unsigned shift,bool uns){
  std::uint64_t result=0;
  for(unsigned bit=0;bit<2*bits;bit++){
    const int from=int(bit)-int(shift);
    const bool set=from<0?false:from<int(bits)?bool((x>>from)&1):!uns&&bool((x>>(bits-1))&1);
    if(set)result|=UINT64_C(1)<<bit;
  }return result;
}
void sweep(){
  for(unsigned bits:{8u,16u,32u})for(unsigned shift=0;shift<bits;shift++)
  for(bool upper:{false,true})for(bool uns:{false,true})for(bool alias:{false,true})for(unsigned pattern=0;pattern<4;pattern++){
    const unsigned rd=alias?1:2,lanes=64/bits;
    const auto opcode=0x0f00a400u|(upper?0x40000000u:0u)|(uns?0x20000000u:0u)|((bits+shift)<<16)|(1<<5)|rd;
    MinimalGuest g(65536);g.load_elf(elf({0xd2840002u,0x4c40a041u,opcode}));
    const auto sign=UINT64_C(1)<<(bits-1),mask=(UINT64_C(1)<<bits)-1;
    const std::uint64_t values[]{0,1,mask,sign,sign-1,sign+1,0x55aa55aau&mask,0xaa55aa55u&mask};
    for(unsigned n=0;n<128/bits;n++)g.write(0x2000+n*(bits/8),values[(n+pattern*2)%8],bits/8);
    for(unsigned n=0;n<16;n++)g.write(0x2010+n,0xa5,1);
    g.run(2);const auto before=g.floating();const auto cpu=g.registers();
    const auto core=g.runtime().core_snapshot(),memory=g.runtime().memory_snapshot();
    auto restored=MinimalGuest::from_snapshot(g.snapshot());g.run(1);restored.run(1);
    check(g.snapshot()==restored.snapshot(),"Restore changed complete widened state");
    check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"Widening did not execute exactly one instruction");
    check(g.registers().x==cpu.x&&g.registers().nzcv==cpu.nzcv,"Widening changed integer registers or flags");
    check(g.floating().fpcr==before.fpcr&&g.floating().fpsr==before.fpsr,"Widening changed FP control/status");
    check(g.runtime().core_snapshot()==core&&g.runtime().memory_snapshot()==memory,"Widening changed XI/Ether memory authority");
    for(unsigned n=0;n<lanes;n++)check(g.floating().v[rd].lane(n,2*bits)==oracle(before.v[1].lane(n+(upper?lanes:0),bits),bits,shift,uns),"Widened lane differs from bit-position oracle");
    for(unsigned n=0;n<32;n++)if(n!=rd)check(g.floating().v[n].low==before.v[n].low&&g.floating().v[n].high==before.v[n].high,"Widening modified another vector");
    ++cases;
  }
}
void regression_and_rejections(){
  // Exact failing opcode, including source/destination alias v2.
  MinimalGuest g(65536);g.load_elf(elf({0xd2840002u,0x4c407042u,0x2f10a442u}));
  const std::uint64_t data[]{0,0xffff,0x8000,0x1234,0xabcd,1,2,3};
  for(unsigned n=0;n<8;n++)g.write(0x2000+n*2,data[n],2);
  g.apply_xi_delta("h 4095\n");const auto core=g.runtime().core_snapshot();g.run(3);
  for(unsigned n=0;n<4;n++)check(g.floating().v[2].lane(n,32)==data[n],"Exact C08 opcode regression");
  check(g.runtime().core_snapshot()==core,"Exact C08 opcode changed quantum witness");
  for(bool upper:{false,true})for(bool uns:{false,true})for(unsigned encoded:{64u,95u,127u}){
    const auto opcode=0x0f00a400u|(upper?0x40000000u:0u)|(uns?0x20000000u:0u)|(encoded<<16)|0x22u;
    MinimalGuest bad(65536);bad.load_elf(elf({opcode}));const auto before=bad.snapshot();bool rejected=false;
    try{bad.run(1);}catch(const std::exception&){rejected=true;}
    check(rejected&&bad.snapshot()==before,"Reserved widening encoding accepted or mutated state");
  }
  MinimalGuest trapped(128*1024*1024);trapped.load_elf(elf({0x2f10a442u},true),true);trapped.run(1);
  check(trapped.machine().exceptions==1&&(trapped.machine().esr>>26)==7&&!trapped.floating().nonzero(),"FP access trap was bypassed");
  // immh=0 remains a modified immediate, not a widening instruction.
  MinimalGuest modified(65536);modified.load_elf(elf({0x0f00a422u}));modified.run(1);
  check(modified.floating().v[2].low==UINT64_C(0x0100010001000100)&&!modified.floating().v[2].high,"immh=0 modified immediate regressed");
}
}
int main(){try{sweep();regression_and_rejections();std::cout<<"{\"checks\":"<<checks<<",\"widen_cases\":"<<cases<<",\"exact_c08_opcode\":\"0x2f10a442\",\"android_boot\":false}\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
