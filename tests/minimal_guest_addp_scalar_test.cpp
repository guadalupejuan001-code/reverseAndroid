// Exact C11 ADDP stop. Generated instruction tests, not Android boot evidence.
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
// Independent byte adder with an explicit carry; discard overflow at byte 8.
std::uint64_t oracle(const Vector128& v){
  std::uint64_t result=0;unsigned carry=0;
  for(unsigned b=0;b<8;b++){
    const unsigned sum=unsigned((v.low>>(8*b))&255)+unsigned((v.high>>(8*b))&255)+carry;
    result|=std::uint64_t(sum&255)<<(8*b);carry=sum>>8;
  }
  return result;
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
void execute(MinimalGuest g,unsigned rn,unsigned rd,bool restore){
  g.write(g.registers().pc,0x5ef1b800u|(rn<<5)|rd,4);
  const auto fp=g.floating();const auto cpu=g.registers();
  const auto memory=g.runtime().memory_snapshot(),core=g.runtime().core_snapshot();
  const auto expected=oracle(fp.v[rn]);
  if(restore){auto resumed=MinimalGuest::from_snapshot(g.snapshot());g.run(1);resumed.run(1);check(g.snapshot()==resumed.snapshot(),"ADDP differs after checkpoint restore");}
  else g.run(1);
  check(g.floating().v[rd].low==expected&&g.floating().v[rd].high==0,"ADDP scalar sum or upper clearing differs");
  check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"ADDP did not execute exactly once");
  check(g.registers().x==cpu.x&&g.registers().nzcv==cpu.nzcv,"ADDP changed integer registers or NZCV");
  check(g.floating().fpcr==fp.fpcr&&g.floating().fpsr==fp.fpsr,"ADDP changed FPCR/FPSR");
  check(g.runtime().memory_snapshot()==memory&&g.runtime().core_snapshot()==core,"ADDP changed XI/Ether state");
  for(unsigned n=0;n<32;n++)if(n!=rd)check(g.floating().v[n].low==fp.v[n].low&&g.floating().v[n].high==fp.v[n].high,"ADDP changed unrelated vector");
  ++cases;
}
void suite(){
  const auto g=prepared(UINT64_C(0xfedcba9876543210),UINT64_C(0x0123456789abcdef),true);
  execute(g,0,0,true); // Exact 0x5ef1b800 C11 stop, source/destination alias.
  for(unsigned rn=0;rn<32;rn++)for(unsigned rd=0;rd<32;rd++)execute(g,rn,rd,rn==rd);
  const std::uint64_t edges[]{0,1,UINT64_MAX,UINT64_C(0x8000000000000000),UINT64_C(0x7fffffffffffffff),UINT64_C(0x5555555555555555),UINT64_C(0xaaaaaaaaaaaaaaaa)};
  for(auto low:edges)for(auto high:edges){const auto e=prepared(low,high,false);for(unsigned rn:{0u,15u,31u})for(unsigned rd:{0u,15u,31u})execute(e,rn,rd,true);}
  for(unsigned bit=0;bit<64;bit++){
    const auto bit_value=UINT64_C(1)<<bit;
    for(const auto pair:std::array<std::array<std::uint64_t,2>,4>{{{bit_value,0},{0,bit_value},{bit_value,bit_value},{bit_value-1,1}}}){
      const auto e=prepared(pair[0],pair[1],false);execute(e,0,0,true);execute(e,31,15,false);
    }
  }
  auto quantum=g;quantum.apply_xi_delta("h 4095\n");execute(quantum,0,0,true);
  // size=00/01/10 are unallocated for scalar ADDP; never accept them as .2D.
  for(unsigned size=0;size<3;size++)for(unsigned rn=0;rn<32;rn++)for(unsigned rd=0;rd<32;rd++){
    const auto opcode=0x5e31b800u|(size<<22)|(rn<<5)|rd;
    MinimalGuest bad(65536);bad.load_elf(elf({opcode}));const auto before=bad.snapshot();bool rejected=false;
    try{bad.run(1);}catch(const std::exception&){rejected=true;}
    check(rejected&&bad.snapshot()==before,"Unallocated scalar ADDP accepted or changed state");
  }
  for(unsigned rn:{0u,31u})for(unsigned rd:{0u,31u}){
    MinimalGuest trapped(128*1024*1024);trapped.load_elf(elf({0x5ef1b800u|(rn<<5)|rd},true),true);trapped.run(1);
    check(trapped.machine().exceptions==1&&(trapped.machine().esr>>26)==7&&!trapped.floating().nonzero(),"ADDP bypassed FP access trap");
  }
}
}
int main(){try{suite();std::cout<<"{\"checks\":"<<checks<<",\"addp_scalar_cases\":"<<cases<<",\"register_pairs\":1024,\"reserved_encodings\":3072,\"exact_c11_opcode\":\"0x5ef1b800\",\"android_boot\":false}\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
