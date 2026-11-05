// Single-lane SIMD load/store: exact C24 ST1, widths, lanes, wrapping lists and writeback.
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
std::uint32_t op(unsigned count,bool load,unsigned size,unsigned lane,unsigned mode,unsigned rn,unsigned rm,unsigned rd){
  const unsigned q=lane>>(3-size),s=size==3?0:(lane>>(2-size))&1;
  const unsigned field=size==0?lane&3:size==1?(lane&1)*2:size==2?0:1;
  const unsigned opcode=2*(size==3?2:size)+((count-1)>>1);
  return 0x0d000000u|(load?0x400000u:0)|(q<<30)|(((count-1)&1)<<21)|(opcode<<13)|(s<<12)|(field<<10)|(mode?0x800000u:0)|((mode==1?31u:mode==2?rm:0u)<<16)|(rn<<5)|rd;
}
MinimalGuest prepared(unsigned rn,unsigned rm,unsigned mode,unsigned base=0x3000){
  std::vector<std::uint32_t> code{0xd2840002u};
  for(unsigned r=0;r<32;r++)code.push_back(0x3dc00040u|(r<<10)|r);
  code.insert(code.end(),{0xd2a01803u,0xd51b4403u,0xd2800203u,0xd51b4423u,0xf100045fu});
  if(rn==31)code.insert(code.end(),{0xd2800000u|(base<<5)|30u,0x910003dfu});
  else code.push_back(0xd2800000u|(base<<5)|rn);
  if(mode==2)code.push_back(0xd2800800u|rm); // Capture xM before base writeback, including Rn==Rm.
  code.push_back(0xd503201fu);MinimalGuest g(65536);g.load_elf(elf(code));
  for(unsigned r=0;r<32;r++){g.write(0x2000+r*16,UINT64_C(0xfedcba9876543210)^r,8);g.write(0x2008+r*16,UINT64_C(0x0123456789abcdef)^r,8);}
  g.run(code.size()-1);return g;
}
void execute(unsigned count,bool load,unsigned size,unsigned lane,unsigned mode,unsigned rn,unsigned rm,unsigned rd,bool restore=false,bool unaligned=false){
  auto g=prepared(rn,rm,mode,unaligned?0x3001:0x3000);const unsigned bytes=1u<<size;
  const auto base=rn==31?g.registers().sp:g.registers().x[rn];
  for(unsigned n=0;n<count*bytes;n++)g.write(base+n,(17+n*37)&255,1);
  g.write(g.registers().pc,op(count,load,size,lane,mode,rn,rm,rd),4);
  const auto f=g.floating();const auto c=g.registers();const auto mem=g.runtime().memory_snapshot(),core=g.runtime().core_snapshot();
  const auto increment=mode==1?std::uint64_t(count*bytes):mode==2?c.x[rm]:0;
  std::array<Vector128,4> want{};
  for(unsigned r=0;r<count;r++){
    std::uint64_t value=0;for(unsigned n=0;n<bytes;n++)value|=std::uint64_t((17+(r*bytes+n)*37)&255)<<(8*n);
    want[r]=f.v[(rd+r)%32];if(load)want[r].lane(lane,bytes*8,value);
  }
  if(restore){auto copy=MinimalGuest::from_snapshot(g.snapshot());g.run(1);copy.run(1);check(g.snapshot()==copy.snapshot(),"Resume differs");}else g.run(1);
  for(unsigned r=0;r<32;r++){
    const unsigned relative=(r+32-rd)%32;const auto& v=relative<count?want[relative]:f.v[r];
    check(g.floating().v[r].low==v.low&&g.floating().v[r].high==v.high,"Lane vector or unrelated vector differs");
  }
  for(unsigned r=0;r<31;r++)check(g.registers().x[r]==(mode&&rn==r?base+increment:c.x[r]),"General register writeback differs");
  check(g.registers().sp==(mode&&rn==31?base+increment:c.sp),"SP writeback differs");
  check(g.registers().pc==c.pc+4&&g.registers().instructions==c.instructions+1,"Wrong instruction boundary");
  check(g.registers().nzcv==c.nzcv&&g.floating().fpcr==f.fpcr&&g.floating().fpsr==f.fpsr,"Flags changed");
  check(g.runtime().core_snapshot()==core,"Single-lane transfer changed XI/Ether core");
  if(load)check(g.runtime().memory_snapshot()==mem,"Lane load changed RAM");
  else for(int n=-8;n<int(count*bytes)+8;n++){
    const auto address=base+n;auto expected=g.read(address,1);
    if(n>=0&&n<int(count*bytes))expected=(f.v[(rd+unsigned(n)/bytes)%32].lane(lane,bytes*8)>>(8*(unsigned(n)%bytes)))&255;
    else expected=0; // The fixture's untouched guard bytes are zero.
    check(g.read(address,1)==expected,"ST lane endian order or guard bytes differ");
  }
  ++cases;
}
void suite(){
 check(op(1,false,2,0,1,12,31,6)==0x0d9f8186,"Exact C24 ST1 encoding differs");
 for(unsigned count=1;count<=4;count++)for(bool load:{false,true})for(unsigned size=0;size<4;size++)for(unsigned lane=0;lane<(16u>>size);lane++)for(unsigned mode=0;mode<3;mode++){
  for(unsigned rd=0;rd<32;rd++)for(unsigned rn:{0u,12u,31u})execute(count,load,size,lane,mode,rn,(rn+rd)%31,rd,rd==31);
  for(unsigned rn:{0u,30u})execute(count,load,size,lane,mode,rn,rn,31,true,true);
  MinimalGuest trap(128*1024*1024);trap.load_elf(elf({op(count,load,size,lane,mode,9,12,3)},true),true);trap.run(1);
  check(trap.machine().exceptions==1&&(trap.machine().esr>>26)==7&&!trap.floating().nonzero(),"FP access trap bypassed");
 }
 auto reject=[&](std::uint32_t instruction){auto bad=prepared(9,0,0);bad.write(bad.registers().pc,instruction,4);const auto before=bad.snapshot();bool rejected=false;try{bad.run(1);}catch(const std::exception&){rejected=true;}check(rejected&&bad.snapshot()==before,"Reserved lane transfer executed or mutated state");};
 for(unsigned count=1;count<=4;count++)for(bool load:{false,true})for(unsigned size=0;size<4;size++)for(unsigned lane=0;lane<(16u>>size);lane++)for(unsigned rm=1;rm<32;rm++)reject(op(count,load,size,lane,0,9,0,31)|(rm<<16));
 for(unsigned count=1;count<=4;count++)for(bool load:{false,true})for(unsigned q=0;q<2;q++)for(unsigned s=0;s<2;s++)for(unsigned field=0;field<4;field++)for(unsigned scale=1;scale<=2;scale++){
  if(scale==1?!(field&1):!((field&2)||((field&1)&&s)))continue;
  for(unsigned post=0;post<2;post++)reject(0x0d000000u|(load?0x400000u:0)|(q<<30)|(((count-1)&1)<<21)|((2*scale+((count-1)>>1))<<13)|(s<<12)|(field<<10)|(post?0x9f0000u:0)|(9<<5)|31);
 }
 for(bool load:{false,true})for(unsigned size=0;size<4;size++){
  auto fault=prepared(9,0,0,65536-4*(1u<<size)+1);fault.write(fault.registers().pc,op(4,load,size,0,1,9,31,31),4);const auto before=fault.snapshot();bool failed=false;
  try{fault.run(1);}catch(const std::exception& e){failed=std::string(e.what()).find("Unsupported AArch64")==std::string::npos;}
  check(failed&&fault.snapshot()==before,"Faulting final lane changed RAM, vector or pointer");
 }
}
}
int main(){try{suite();std::cout<<"{\"checks\":"<<checks<<",\"cases\":"<<cases<<",\"lane_and_address_forms\":720,\"exact_c24_opcode\":\"0x0d9f8186\",\"android_boot\":false}\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
