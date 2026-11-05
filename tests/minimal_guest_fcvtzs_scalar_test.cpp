// C41 exact C40/C40R3 stop. Native instruction tests; not Android boot evidence.
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
void constant(std::vector<std::uint32_t>& code,unsigned rd,std::uint64_t value){
 code.push_back(0xd2800000u|(std::uint32_t(value&65535)<<5)|rd);
 for(unsigned k=1;k<4;k++)code.push_back(0xf2800000u|(k<<21)|(std::uint32_t((value>>(16*k))&65535)<<5)|rd);
}
MinimalGuest prepared(unsigned bits,std::uint64_t value,unsigned fpcr=0){
 std::vector<std::uint32_t> code{0xd2840002u};
 for(unsigned r=0;r<32;r++)code.push_back(0x3dc00040u|(r<<10)|r);
 constant(code,3,fpcr);code.push_back(0xd51b4403u);
 constant(code,3,10);code.push_back(0xd51b4423u); // Existing DZC/UFC are cumulative.
 code.push_back(0xf100045fu);code.push_back(0xd503201fu);
 MinimalGuest g(65536);g.load_elf(elf(code));
 for(unsigned r=0;r<32;r++){
  g.write(0x2000+r*16,bits==32?UINT64_C(0xfeedface00000000)|value:value,8);
  g.write(0x2008+r*16,UINT64_C(0x0123456789abcdef)^r,8);
 }
 g.run(code.size()-1);return g;
}
void execute(MinimalGuest g,unsigned bits,bool uns,unsigned rn,unsigned rd,std::uint64_t expected,unsigned flags,bool restore,bool from_integer=false){
 const auto op=(from_integer?0x5e21d800u:0x5ea1b800u)|(bits==64?0x00400000u:0)|(uns?0x20000000u:0)|(rn<<5)|rd;
 g.write(g.registers().pc,op,4);const auto cpu=g.registers();const auto fp=g.floating();
 const auto memory=g.runtime().memory_snapshot(),core=g.runtime().core_snapshot();
 if(restore){auto resumed=MinimalGuest::from_snapshot(g.snapshot());g.run(1);resumed.run(1);check(g.snapshot()==resumed.snapshot(),"FCVTZS differs after checkpoint restore");}
 else g.run(1);
 check(g.floating().v[rd].low==expected&&g.floating().v[rd].high==0,"FCVTZS value or upper clearing differs");
 check(g.floating().fpsr==(fp.fpsr|flags)&&g.floating().fpcr==fp.fpcr,"FCVTZS flags or fixed rounding differs");
 check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"FCVTZS did not advance exactly one instruction");
 check(g.registers().x==cpu.x&&g.registers().sp==cpu.sp&&g.registers().nzcv==cpu.nzcv,"FCVTZS changed integer CPU registers");
 check(g.runtime().memory_snapshot()==memory&&g.runtime().core_snapshot()==core,"FCVTZS changed XI/Ether memory or core");
 for(unsigned r=0;r<32;r++)if(r!=rd)check(g.floating().v[r].low==fp.v[r].low&&g.floating().v[r].high==fp.v[r].high,"FCVTZS changed an unrelated vector register");
 ++cases;
}
void suite(){
 // Exact observed encoding: FCVTZS D1,D0. Expected result is independently specified.
 execute(prepared(64,UINT64_C(0x4004000000000000)),64,false,0,1,2,16,true);
 for(unsigned bits:{32u,64u})for(bool uns:{false,true}){
  const auto g=prepared(bits,bits==32?UINT64_C(0x40200000):UINT64_C(0x4004000000000000));
  for(unsigned rn=0;rn<32;rn++)for(unsigned rd=0;rd<32;rd++)execute(g,bits,uns,rn,rd,2,16,rn==rd);
 }
 struct Example{unsigned bits;std::uint64_t raw,signed_result,unsigned_result;unsigned signed_flags,unsigned_flags;};
 const Example examples[]{
  {32,0,0,0,0,0},{64,UINT64_C(0x8000000000000000),0,0,0,0},
  {32,0xc0200000,0xfffffffe,0,16,1},{64,UINT64_C(0xc004000000000000),UINT64_MAX-1,0,16,1},
  {32,0xbf000000,0,0,16,16},{64,UINT64_C(0xbfe0000000000000),0,0,16,16},
  {32,0x7fc12345,0,0,1,1},{64,UINT64_C(0x7ff0000000000001),0,0,1,1},
  {32,0x7f800000,0x7fffffff,0xffffffff,1,1},
  {64,UINT64_C(0x7ff0000000000000),UINT64_C(0x7fffffffffffffff),UINT64_MAX,1,1},
  {32,0xff800000,0x80000000,0,1,1},
  {64,UINT64_C(0xfff0000000000000),UINT64_C(0x8000000000000000),0,1,1},
  {32,0xcf000000,0x80000000,0,0,1},
  {64,UINT64_C(0xc3e0000000000000),UINT64_C(0x8000000000000000),0,0,1},
  {32,0x4f000000,0x7fffffff,0x80000000,1,0},
  {64,UINT64_C(0x43e0000000000000),UINT64_C(0x7fffffffffffffff),UINT64_C(0x8000000000000000),1,0},
  {32,1,0,0,16,16},{64,1,0,0,16,16}
 };
 for(const auto& e:examples)for(unsigned rounding=0;rounding<4;rounding++)for(bool uns:{false,true})
  for(unsigned rd:{0u,1u,31u})execute(prepared(e.bits,e.raw,rounding<<22),e.bits,uns,0,rd,uns?e.unsigned_result:e.signed_result,uns?e.unsigned_flags:e.signed_flags,true);
 for(unsigned bits:{32u,64u})for(bool uns:{false,true})execute(prepared(bits,1,1u<<24),bits,uns,0,0,0,128,true);
 // Exact second Android stop, twelve instructions later: SCVTF D1,D1.
 execute(prepared(64,2),64,false,1,1,UINT64_C(0x4000000000000000),0,true,true);
 for(unsigned bits:{32u,64u})for(bool uns:{false,true}){
  const auto g=prepared(bits,2);
  for(unsigned rn=0;rn<32;rn++)for(unsigned rd=0;rd<32;rd++)execute(g,bits,uns,rn,rd,bits==32?UINT64_C(0x40000000):UINT64_C(0x4000000000000000),0,rn==rd,true);
 }
 struct IntegerExample{unsigned bits;bool uns;std::uint64_t raw;std::array<std::uint64_t,4> result;unsigned flags;};
 const IntegerExample integers[]{
  {32,false,0xffffffff,{0xbf800000,0xbf800000,0xbf800000,0xbf800000},0},
  {64,false,UINT64_MAX,{UINT64_C(0xbff0000000000000),UINT64_C(0xbff0000000000000),UINT64_C(0xbff0000000000000),UINT64_C(0xbff0000000000000)},0},
  {32,true,0xffffffff,{0x4f800000,0x4f800000,0x4f7fffff,0x4f7fffff},16},
  {64,true,UINT64_MAX,{UINT64_C(0x43f0000000000000),UINT64_C(0x43f0000000000000),UINT64_C(0x43efffffffffffff),UINT64_C(0x43efffffffffffff)},16},
  {32,false,0x7fffffff,{0x4f000000,0x4f000000,0x4effffff,0x4effffff},16},
  {64,false,UINT64_C(0x7fffffffffffffff),{UINT64_C(0x43e0000000000000),UINT64_C(0x43e0000000000000),UINT64_C(0x43dfffffffffffff),UINT64_C(0x43dfffffffffffff)},16},
  {32,false,0x80000000,{0xcf000000,0xcf000000,0xcf000000,0xcf000000},0},
  {64,false,UINT64_C(0x8000000000000000),{UINT64_C(0xc3e0000000000000),UINT64_C(0xc3e0000000000000),UINT64_C(0xc3e0000000000000),UINT64_C(0xc3e0000000000000)},0},
  {32,false,0x01000001,{0x4b800000,0x4b800001,0x4b800000,0x4b800000},16},
  {32,false,0xfeffffff,{0xcb800000,0xcb800000,0xcb800001,0xcb800000},16},
  {64,false,UINT64_C(0x0020000000000001),{UINT64_C(0x4340000000000000),UINT64_C(0x4340000000000001),UINT64_C(0x4340000000000000),UINT64_C(0x4340000000000000)},16}
 };
 for(const auto& e:integers)for(unsigned mode=0;mode<4;mode++)for(unsigned rd:{0u,1u,31u})
  execute(prepared(e.bits,e.raw,mode<<22),e.bits,e.uns,1,rd,e.result[mode],e.flags,true,true);
 for(const auto op:{0x5ee1b801u,0x5ea1b801u,0x7ee1b801u,0x7ea1b801u,0x5e61d821u,0x5e21d821u,0x7e61d821u,0x7e21d821u}){
  MinimalGuest g(128*1024*1024);g.load_elf(elf({op},true),true);g.run(1);
  check(g.machine().exceptions==1&&(g.machine().esr>>26)==7&&!g.floating().nonzero(),"FCVTZS bypassed the architectural FP access trap");
 }
 // The unallocated 1D vector form must not match the scalar decoder.
 MinimalGuest bad(65536);bad.load_elf(elf({0x0ee1b801u}));const auto before=bad.snapshot();bool rejected=false;
 try{bad.run(1);}catch(const std::exception&){rejected=true;}
 check(rejected&&bad.snapshot()==before,"Reserved 1D conversion was accepted or modified state");
}
}
int main(){try{suite();std::cout<<"{\"checks\":"<<checks<<",\"cases\":"<<cases<<",\"exact_c40_opcode\":\"0x5ee1b801\",\"exact_c41_opcode\":\"0x5e61d821\",\"android_boot\":false}\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
