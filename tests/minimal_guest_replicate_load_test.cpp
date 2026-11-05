// Replicate loads observed in original Android code, checked across all vector widths.
#include "minimal_guest.hpp"
#include <iostream>
using namespace reverse_pc::guest;
namespace {
unsigned checks=0,cases=0;
void check(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
Bytes elf(const std::vector<std::uint32_t>& words,bool system=false){
  Bytes code;for(auto x:words)put(code,x,4);const auto entry=system?0x40080000:0x1000;
  Bytes b{0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
  put(b,2,2);put(b,183,2);put(b,1,4);put(b,entry);put(b,64);put(b,0);
  put(b,0,4);put(b,64,2);put(b,56,2);put(b,1,2);put(b,0,2);put(b,0,2);put(b,0,2);
  put(b,1,4);put(b,7,4);put(b,0x100);put(b,entry);put(b,entry);put(b,code.size());put(b,code.size());put(b,0x100);
  b.resize(0x100);b.insert(b.end(),code.begin(),code.end());return b;
}
std::uint32_t op(unsigned count,bool q,unsigned size,unsigned mode,unsigned rn,unsigned rm,unsigned rd){
  return 0x0d40c000u|(q?0x40000000u:0)|(((count-1)&1)<<21)|(((count-1)>>1)<<13)|(size<<10)|(mode?0x800000u:0)|((mode==1?31u:mode==2?rm:0u)<<16)|(rn<<5)|rd;
}
MinimalGuest prepared(unsigned rn,unsigned rm,unsigned mode,unsigned base=0x3000){
  std::vector<std::uint32_t> code{0xd2840002u};
  for(unsigned r=0;r<32;r++)code.push_back(0x3dc00040u|(r<<10)|r);
  code.insert(code.end(),{0xd28000a3u,0xd51b4403u,0xd2840003u,0xd51b4423u,0xf100045fu});
  if(rn==31)code.insert(code.end(),{0xd2800000u|(base<<5)|30u,0x910003dfu});
  else code.push_back(0xd2800000u|(base<<5)|rn);
  if(mode==2)code.push_back(0xd2800800u|rm); // Capture xM before base writeback, including Rn==Rm.
  code.push_back(0xd503201fu);MinimalGuest g(65536);g.load_elf(elf(code));
  for(unsigned r=0;r<32;r++){g.write(0x2000+r*16,UINT64_C(0xfedcba9876543210)^r,8);g.write(0x2008+r*16,UINT64_C(0x0123456789abcdef)^r,8);}
  g.run(code.size()-1);return g;
}
void execute(unsigned count,bool q,unsigned size,unsigned mode,unsigned rn,unsigned rm,unsigned rd,bool restore=false,bool unaligned=false){
  auto g=prepared(rn,rm,mode,unaligned?0x3001:0x3000);const unsigned bytes=1u<<size;
  const auto base=rn==31?g.registers().sp:g.registers().x[rn];
  for(unsigned n=0;n<count*bytes;n++)g.write(base+n,(17+n*37)&255,1);
  g.write(g.registers().pc,op(count,q,size,mode,rn,rm,rd),4);
  const auto f=g.floating();const auto c=g.registers();const auto mem=g.runtime().memory_snapshot(),core=g.runtime().core_snapshot();
  const auto increment=mode==1?std::uint64_t(count*bytes):mode==2?c.x[rm]:0;
  std::array<Vector128,4> want{};
  for(unsigned r=0;r<count;r++){
    std::uint64_t value=0;for(unsigned n=0;n<bytes;n++)value|=std::uint64_t((17+(r*bytes+n)*37)&255)<<(8*n);
    for(unsigned lane=0;lane<(q?16u:8u)/bytes;lane++)want[r].lane(lane,bytes*8,value);
  }
  if(restore){auto copy=MinimalGuest::from_snapshot(g.snapshot());g.run(1);copy.run(1);check(g.snapshot()==copy.snapshot(),"Resume differs");}else g.run(1);
  for(unsigned r=0;r<32;r++){
    const unsigned relative=(r+32-rd)%32;const auto& v=relative<count?want[relative]:f.v[r];
    check(g.floating().v[r].low==v.low&&g.floating().v[r].high==v.high,"Replicate vector or unrelated vector differs");
  }
  for(unsigned r=0;r<31;r++)check(g.registers().x[r]==(mode&&rn==r?base+increment:c.x[r]),"General register writeback differs");
  check(g.registers().sp==(mode&&rn==31?base+increment:c.sp),"SP writeback differs");
  check(g.registers().pc==c.pc+4&&g.registers().instructions==c.instructions+1,"Wrong instruction boundary");
  check(g.registers().nzcv==c.nzcv&&g.floating().fpcr==f.fpcr&&g.floating().fpsr==f.fpsr,"Flags changed");
  check(g.runtime().memory_snapshot()==mem&&g.runtime().core_snapshot()==core,"Replicate load changed memory or XI/Ether");++cases;
}
void suite(){
 check(op(1,true,2,1,9,31,3)==0x4ddfc923,"Original-window LD1R encoding differs");
 for(unsigned count=1;count<=4;count++)for(bool q:{false,true})for(unsigned size=0;size<4;size++)for(unsigned mode=0;mode<3;mode++){
  for(unsigned rd=0;rd<32;rd++)for(unsigned rn:{0u,9u,31u})execute(count,q,size,mode,rn,(rn+rd)%31,rd,rd==31);
  for(unsigned rn:{0u,30u})execute(count,q,size,mode,rn,rn,31,true,true);
  MinimalGuest trap(128*1024*1024);trap.load_elf(elf({op(count,q,size,mode,9,12,3)},true),true);trap.run(1);
  check(trap.machine().exceptions==1&&(trap.machine().esr>>26)==7&&!trap.floating().nonzero(),"FP access trap bypassed");
 }
 for(unsigned count=1;count<=4;count++)for(bool q:{false,true})for(unsigned size=0;size<4;size++)for(unsigned rm=1;rm<32;rm++){
  auto bad=prepared(9,0,0);bad.write(bad.registers().pc,op(count,q,size,0,9,0,31)|(rm<<16),4);const auto before=bad.snapshot();bool rejected=false;
  try{bad.run(1);}catch(const std::exception&){rejected=true;}check(rejected&&bad.snapshot()==before,"Nonzero Rm in no-writeback encoding accepted");
 }
 auto fault=prepared(9,0,0,65533);fault.write(fault.registers().pc,op(4,true,0,1,9,31,31),4);const auto before=fault.snapshot();bool failed=false;
 try{fault.run(1);}catch(const std::exception& e){failed=std::string(e.what()).find("Unsupported AArch64")==std::string::npos;}
 check(failed&&fault.snapshot()==before,"Faulting final element changed a vector or pointer");
}
}
int main(){try{suite();std::cout<<"{\"checks\":"<<checks<<",\"cases\":"<<cases<<",\"forms\":96,\"original_window_opcode\":\"0x4ddfc923\",\"android_boot\":false}\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
