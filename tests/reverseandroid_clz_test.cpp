#include "minimal_guest.hpp"
#include <iostream>
using namespace reverse_pc::guest;
namespace {
unsigned checks = 0, cases = 0, hardware_cases = 0;
void check(bool b, const char* why) { ++checks; if (!b) throw std::runtime_error(why); }
Bytes elf(const std::vector<std::uint32_t>& words, bool system = false) {
  Bytes code; for (auto word : words) put(code, word, 4);
  const auto entry = system ? 0x40080000 : 0x1000;
  Bytes b{0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
  put(b,2,2); put(b,183,2); put(b,1,4); put(b,entry); put(b,64); put(b,0);
  put(b,0,4); put(b,64,2); put(b,56,2); put(b,1,2); put(b,0,2); put(b,0,2); put(b,0,2);
  put(b,1,4); put(b,7,4); put(b,0x100); put(b,entry); put(b,entry);
  put(b,code.size()); put(b,code.size()); put(b,0x100);
  b.resize(0x100); b.insert(b.end(),code.begin(),code.end()); return b;
}
using Vectors = std::array<Vector128, 32>;
MinimalGuest prepared(const Vectors& values, unsigned cr = 0, unsigned sr = 0) {
  std::vector<std::uint32_t> code{0xd2840002u}; // x2 = 0x2000
  for (unsigned r=0;r<32;++r) code.push_back(0x3dc00040u|(r<<10)|r);
  // FPCR and FPSR are initialized with ordinary guest instructions.
  for (auto item : {std::pair<unsigned,unsigned>{cr,0xd51b4403u},{sr,0xd51b4423u}}) {
    code.push_back(0xd2800003u|((item.first&65535)<<5));
    code.push_back(0xf2a00003u|((item.first>>16)<<5)); code.push_back(item.second);
  }
  code.push_back(0xf100045fu); code.push_back(0xd503201fu);
  MinimalGuest g(65536); g.load_elf(elf(code));
  for (unsigned r=0;r<32;++r) {g.write(0x2000+r*16,values[r].low,8);g.write(0x2008+r*16,values[r].high,8);}
  g.run(code.size()-1); return g;
}
std::uint64_t seed=0x6ea04800;
std::uint64_t random_value(){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
Vectors fixture(){Vectors v;for(auto& x:v)x={random_value(),random_value()};return v;}
unsigned oracle(std::uint64_t x,unsigned width){unsigned digits=0;while(x){++digits;x>>=1;}return width-digits;}
void execute(MinimalGuest g,unsigned bits,bool q,unsigned rn,unsigned rd,bool restore=false){
  const unsigned size=bits==8?0:bits==16?1:2;
  const auto op=0x2e204800u|(q?0x40000000u:0)|(size<<22)|(rn<<5)|rd;
  g.write(g.registers().pc,op,4);
  const auto cpu=g.registers();const auto fp=g.floating();
  const auto memory=g.runtime().memory_persistent_snapshot(),core=g.runtime().core_snapshot();
  Vector128 expected;
  for(unsigned lane=0;lane<(q?128:64)/bits;++lane)expected.lane(lane,bits,oracle(fp.v[rn].lane(lane,bits),bits));
#ifdef __aarch64__
  // Independent real AArch64 NEON instructions, in addition to the integer oracle.
  const auto source=fp.v[rn];Vector128 actual;
  // Fixed architectural assembly avoids a collision between arm_neon.h and
  // the pinned SoftFloat library's public float16_t/bfloat16_t typedefs.
#define HARDWARE_CLZ(form) asm volatile("ldr q0, %1\n\tclz v0." form ", v0." form "\n\tstr q0, %0" : "=m"(actual) : "m"(source) : "v0")
  if(q){if(bits==8){HARDWARE_CLZ("16b");}else if(bits==16){HARDWARE_CLZ("8h");}else{HARDWARE_CLZ("4s");}}
  else{if(bits==8){HARDWARE_CLZ("8b");}else if(bits==16){HARDWARE_CLZ("4h");}else{HARDWARE_CLZ("2s");}}
#undef HARDWARE_CLZ
  check(actual.low==expected.low&&actual.high==expected.high,"Real AArch64 CLZ differs from mathematical oracle");++hardware_cases;
#endif
  if(restore){auto copy=MinimalGuest::from_snapshot(g.snapshot());g.run(1);copy.run(1);check(g.snapshot()==copy.snapshot(),"CLZ restore differs");}else g.run(1);
  check(g.floating().fpsr==fp.fpsr&&g.floating().fpcr==fp.fpcr,"CLZ changed floating flags/control");
  check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"CLZ instruction retirement differs");
  check(g.registers().sp==cpu.sp&&g.registers().nzcv==cpu.nzcv&&g.registers().x==cpu.x,"CLZ changed integer state");
  check(g.runtime().memory_persistent_snapshot()==memory&&g.runtime().core_snapshot()==core,"CLZ changed XI/Ether state");
  for(unsigned r=0;r<32;++r){const auto want=r==rd?expected:fp.v[r];check(g.floating().v[r].low==want.low&&g.floating().v[r].high==want.high,"CLZ lane, alias or upper bits differ");}
  ++cases;
}
void suite(){
  for(unsigned bits:{8u,16u,32u})for(bool q:{false,true}){
    const auto base=prepared(fixture(),0x03c00000,0x0800009f);
    for(unsigned rn=0;rn<32;++rn)for(unsigned rd=0;rd<32;++rd)execute(base,bits,q,rn,rd,rn==rd);
    // Every possible B/H element, including zero and every leading-bit boundary.
    if(bits<=16)for(unsigned start=0;start<(1u<<bits);start+=(q?128:64)/bits){
      auto v=fixture();for(unsigned n=0;n<(q?128:64)/bits;++n)v[5].lane(n,bits,start+n);
      execute(prepared(v),bits,q,5,5,start==0);
    }
    for(unsigned k=0;k<=bits;++k){auto v=fixture();for(unsigned n=0;n<128/bits;++n)v[7].lane(n,bits,k==bits?0:UINT64_C(1)<<k);execute(prepared(v),bits,q,7,31,k==bits);}
    const auto op=0x2e204800u|(q?0x40000000u:0)|((bits==8?0:bits==16?1:2)<<22)|(5<<5)|7;
    MinimalGuest trap(128*1024*1024);trap.load_elf(elf({op},true),true);trap.run(1);
    check(trap.machine().exceptions==1&&(trap.machine().esr>>26)==7&&!trap.floating().nonzero(),"CLZ bypassed FP/SIMD access trap");
  }
  auto exact=fixture();exact[0]={UINT64_C(0x0000000100000000),UINT64_C(0xffffffff80000000)};
  execute(prepared(exact),32,true,0,0,true); // Exact Android opcode: 0x6ea04800.
  for(unsigned op:{0x6ee04800u,0x2ee04800u,0x7ea04800u,0x3ea04800u})for(bool system:{false,true}){
    MinimalGuest g(system?128*1024*1024:65536);g.load_elf(elf({op},system),system);const auto before=g.snapshot();bool rejected=false;
    try{g.run(1);}catch(const std::exception&){rejected=true;}
    check(rejected&&g.snapshot()==before,"Reserved/scalar CLZ form changed state or bypassed rejection");
  }
}
}
int main(int argc,char** argv){try{
  if(argc==2&&std::string(argv[1])=="--expect-unsupported"){
    auto g=prepared(fixture());g.write(g.registers().pc,0x6ea04800u,4);const auto before=g.snapshot();std::string error;
    try{g.run(1);}catch(const std::exception& e){error=e.what();}
    check(error.find("Unsupported AArch64 instruction 0x6ea04800")!=std::string::npos&&g.snapshot()==before,"Original baseline did not reproduce exact opcode refusal");
    std::cout<<"{\"original_exact_opcode_rejected_without_state_change\":true}\n";return 0;
  }
  suite();std::cout<<"{\"CLZ_cases\":"<<cases<<",\"checks\":"<<checks<<",\"real_AArch64_hardware_cases\":"<<hardware_cases<<",\"mathematical_oracle\":true,\"all_six_forms\":true,\"all_register_pairs\":true,\"exhaustive_B_H_values\":true,\"FP_traps_and_reserved_rejection\":true}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
