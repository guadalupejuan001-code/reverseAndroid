// C09 regression: exact XTN stop, every register pair/size, aliasing and restore.
// These are generated instruction tests, not Android boot evidence.
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
// Independent byte gather: keep the low half of each original element.
Vector128 oracle(const Vector128& src,const Vector128& old,unsigned bytes,bool upper){
  Vector128 expected=upper?old:Vector128{};
  for(unsigned n=0;n<8/bytes;n++)for(unsigned b=0;b<bytes;b++)
    expected.lane((upper?8:0)+n*bytes+b,8,src.lane(n*bytes*2+b,8));
  return expected;
}
MinimalGuest prepared(unsigned pattern){
  std::vector<std::uint32_t> code{0xd2840002u};
  for(unsigned n=0;n<32;n++)code.push_back(0x3dc00040u|(n<<10)|n); // LDR Qn,[X2,#n*16].
  code.push_back(0xd503201fu);MinimalGuest g(65536);g.load_elf(elf(code));
  for(unsigned r=0;r<32;r++)for(unsigned b=0;b<16;b++){
    const unsigned v=pattern==0?(r*61+b*37+0x83)&255:pattern==1?(b&1?0xff:0x80):pattern==2?(b&1?0:0xff):(r+b)&1?0x55:0xaa;
    g.write(0x2000+r*16+b,v,1);
  }
  g.run(33);return g;
}
void execute(MinimalGuest g,unsigned rn,unsigned rd,unsigned size,bool upper,bool restore){
  const auto opcode=0x0e212800u|(size<<22)|(upper?0x40000000u:0u)|(rn<<5)|rd;
  g.write(g.registers().pc,opcode,4);
  const auto fp=g.floating();const auto cpu=g.registers();
  const auto expected=oracle(fp.v[rn],fp.v[rd],1u<<size,upper);
  const auto memory=g.runtime().memory_snapshot(),core=g.runtime().core_snapshot();
  if(restore){auto restored=MinimalGuest::from_snapshot(g.snapshot());g.run(1);restored.run(1);check(g.snapshot()==restored.snapshot(),"XTN changed after native restore");}
  else g.run(1);
  check(g.floating().v[rd].low==expected.low&&g.floating().v[rd].high==expected.high,"XTN differs from independent byte gather");
  check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"XTN must execute exactly one instruction");
  check(g.registers().x==cpu.x&&g.registers().nzcv==cpu.nzcv,"XTN changed GPRs/NZCV");
  check(g.floating().fpcr==fp.fpcr&&g.floating().fpsr==fp.fpsr,"XTN must not change FPCR/FPSR or saturate");
  check(g.runtime().memory_snapshot()==memory&&g.runtime().core_snapshot()==core,"XTN modified XI/Ether memory or quantum state");
  for(unsigned n=0;n<32;n++)if(n!=rd)check(g.floating().v[n].low==fp.v[n].low&&g.floating().v[n].high==fp.v[n].high,"XTN changed an unrelated vector");
  ++cases;
}
void sweep(){
  const auto g=prepared(0);
  for(unsigned size=0;size<3;size++)for(bool upper:{false,true})
  for(unsigned rn=0;rn<32;rn++)for(unsigned rd=0;rd<32;rd++)execute(g,rn,rd,size,upper,rn==rd);
  for(unsigned pattern=1;pattern<4;pattern++){
    const auto edge=prepared(pattern);
    for(unsigned size=0;size<3;size++)for(bool upper:{false,true})
    for(unsigned rn:{0u,15u,31u})for(unsigned rd:{0u,15u,31u})execute(edge,rn,rd,size,upper,true);
  }
  // Single-bit source basis proves truncation, ordering and all 128 input bits.
  for(unsigned size=0;size<3;size++)for(bool upper:{false,true})for(unsigned bit=0;bit<128;bit++){
    MinimalGuest basis(65536);basis.load_elf(elf({0xd2840002u,0x3dc00040u,0x3dc00441u,0xd503201fu}));
    basis.write(0x2000+bit/8,1u<<(bit%8),1);
    for(unsigned b=0;b<16;b++)basis.write(0x2010+b,0x5a,1);
    basis.run(3);execute(basis,0,1,size,upper,false);
  }
}
void regression(){
  auto g=prepared(3);g.apply_xi_delta("h 4095\n");execute(g,0,4,1,false,true); // Exact 0x0e612804 C09 stop.
  // All reserved size=11 encodings must be rejected transactionally.
  for(bool upper:{false,true})for(unsigned rn=0;rn<32;rn++)for(unsigned rd=0;rd<32;rd++){
    const auto op=0x0ee12800u|(upper?0x40000000u:0u)|(rn<<5)|rd;
    MinimalGuest bad(65536);bad.load_elf(elf({op}));const auto before=bad.snapshot();bool rejected=false;
    try{bad.run(1);}catch(const std::exception&){rejected=true;}
    check(rejected&&bad.snapshot()==before,"Reserved XTN encoding accepted or modified state");
  }
  for(bool upper:{false,true}){
    MinimalGuest trapped(128*1024*1024);trapped.load_elf(elf({0x0e612804u|(upper?0x40000000u:0u)},true),true);trapped.run(1);
    check(trapped.machine().exceptions==1&&(trapped.machine().esr>>26)==7&&!trapped.floating().nonzero(),"XTN bypassed FP access trap");
  }
}
}
int main(){try{sweep();regression();std::cout<<"{\"checks\":"<<checks<<",\"xtn_cases\":"<<cases<<",\"register_pairs_per_form\":1024,\"exact_c09_opcode\":\"0x0e612804\",\"android_boot\":false}\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
