// Scalar floating pairwise reductions, including the observed Android instruction.
// Arm architectural state is prepared by guest instructions. On AArch64, the
// independent oracle executes the actual encoding on the host CPU.
#include "minimal_guest.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#ifdef __aarch64__
#include <sys/mman.h>
#include <unistd.h>
#endif
using namespace reverse_pc::guest;
namespace {
unsigned checks=0,cases=0,native_cases=0;
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
std::uint64_t raw(double x,unsigned bits){
 if(bits==32){const float f=float(x);std::uint32_t v;std::memcpy(&v,&f,4);return v;}
 std::uint64_t v;std::memcpy(&v,&x,8);return v;
}

using Vectors=std::array<Vector128,32>;
struct Inputs {Vectors vectors{};std::array<std::uint64_t,32> gpr{};};
static_assert(offsetof(Inputs,gpr)==512,"Native oracle input layout changed");
MinimalGuest prepared(const Inputs& values,unsigned fpcr=0,unsigned fpsr=2){
 std::vector<std::uint32_t> code{0xd2840002u};for(unsigned r=0;r<32;++r)code.push_back(0x3dc00040u|(r<<10)|r);
 constant(code,3,fpcr);code.push_back(0xd51b4403u);constant(code,3,fpsr);code.push_back(0xd51b4423u);code.push_back(0xf100045fu);
 for(unsigned r=0;r<31;++r)constant(code,r,values.gpr[r]);code.push_back(0xd503201fu);
 MinimalGuest g(65536);g.load_elf(elf(code));for(unsigned r=0;r<32;++r){g.write(0x2000+r*16,values.vectors[r].low,8);g.write(0x2008+r*16,values.vectors[r].high,8);}g.run(code.size()-1);return g;
}


struct Form {unsigned bits,operation;};
const Form forms[]{{32,2},{64,2},{32,4},{64,4},{32,5},{64,5},{32,6},{64,6},{32,7},{64,7}};
std::uint32_t encoding(Form f,unsigned rn,unsigned rd){return (f.operation==2?0x7e30d800u:f.operation<6?0x7e30f800u:0x7e30c800u)|(f.bits==64?0x00400000u:0)|((f.operation&1)?0x00800000u:0)|(rn<<5)|rd;}
std::uint64_t seed=UINT64_C(0x7e30d8e7);
std::uint64_t random_value(){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
Inputs inputs(){Inputs in;for(unsigned r=0;r<32;r++){in.vectors[r]={random_value(),random_value()};in.gpr[r]=random_value();}return in;}
struct Expected {Vector128 value;unsigned fpsr;};
// Independent existing instruction path: scalar FADD/FMAX/FMIN/FMAXNM/FMINNM
// on two separate source registers. No call to the new pairwise decoder.
Expected scalar_reference(Form f,Vector128 source,unsigned fpcr,unsigned fpsr){
 Inputs in;in.vectors[0].low=source.lane(0,f.bits);in.vectors[1].low=source.lane(1,f.bits);
 auto g=prepared(in,fpcr,fpsr);const auto op=0x1e200800u|(f.bits==64?0x00400000u:0)|(f.operation<<12)|(1u<<16)|2u;
 g.write(g.registers().pc,op,4);g.run(1);return {g.floating().v[2],g.floating().fpsr};
}
void execute(MinimalGuest g,std::uint32_t op,Vector128 expected,unsigned flags,bool restore){
 g.write(g.registers().pc,op,4);const auto cpu=g.registers();const auto fp=g.floating();const auto memory=g.runtime().memory_persistent_snapshot(),core=g.runtime().core_snapshot();
 if(restore){auto resumed=MinimalGuest::from_snapshot(g.snapshot());g.run(1);resumed.run(1);check(g.snapshot()==resumed.snapshot(),"FP pairwise changed after original checkpoint decoding");}else g.run(1);
 const unsigned rd=op&31;check(g.floating().fpsr==(fp.fpsr|flags)&&g.floating().fpcr==fp.fpcr,"FP pairwise FPCR/FPSR differs");
 check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"FP pairwise did not advance exactly one instruction");
 check(g.registers().sp==cpu.sp&&g.registers().nzcv==cpu.nzcv,"FP pairwise changed SP or NZCV");
 check(g.runtime().memory_persistent_snapshot()==memory&&g.runtime().core_snapshot()==core,"FP pairwise changed XI/Ether memory/core");
 for(unsigned r=0;r<31;++r)check(g.registers().x[r]==cpu.x[r],"FP pairwise changed a GPR");
 for(unsigned r=0;r<32;++r){const auto value=r==rd?expected:fp.v[r];check(g.floating().v[r].low==value.low&&g.floating().v[r].high==value.high,"FP pairwise destination/alias/upper SIMD bits differ");}++cases;
}

void allocated_forms(){
 for(auto f:forms){Inputs in;for(unsigned r=0;r<32;r++){in.gpr[r]=UINT64_C(0xa55a5aa5ffff0000)+r;for(unsigned lane=0;lane<128/f.bits;lane++)in.vectors[r].lane(lane,f.bits,raw(double(r+1)+double(lane)/4,f.bits));}const auto base=prepared(in,0,0x0800009f);
  for(unsigned rn=0;rn<32;rn++)for(unsigned rd=0;rd<32;rd++){const double a=rn+1,b=a+.25;const auto expected=raw(f.operation==2?a+b:(f.operation&1)?a:b,f.bits);execute(base,encoding(f,rn,rd),{expected,0},0,rn==rd);}
 }
 Inputs actual;actual.vectors[7].low=0x3cb851e8;execute(prepared(actual,0,17),0x7e30d8e7u,{0x3cb851e8,0},0,true);
}
std::vector<std::uint64_t> edges(unsigned bits){
 const auto sign=ArmFloat::sign(bits),exponent=bits==32?UINT64_C(0x7f800000):UINT64_C(0x7ff0000000000000),quiet=bits==32?UINT64_C(0x400000):UINT64_C(0x8000000000000),normal=bits==32?UINT64_C(0x800000):UINT64_C(0x10000000000000);
 return {0,sign,raw(1,bits),raw(-1,bits),raw(.5,bits),raw(2,bits),1,sign|1,normal-1,normal,normal+1,exponent-1,sign|(exponent-1),exponent,sign|exponent,exponent|quiet|0x12345,sign|exponent|quiet|0x23456,exponent|0x12345,sign|exponent|0x23456};
}
void numerical_and_random(){
 for(auto f:forms)for(unsigned mode=0;mode<16;mode++){
  const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);const auto values=edges(f.bits);
  for(unsigned ai=0;ai<values.size();ai++)for(unsigned bi=0;bi<values.size();bi++){
   auto in=inputs();in.vectors[10].lane(0,f.bits,values[ai]);in.vectors[10].lane(1,f.bits,values[bi]);const auto ref=scalar_reference(f,in.vectors[10],fpcr,2);
   execute(prepared(in,fpcr),encoding(f,10,(ai+bi)&1?10:8),ref.value,ref.fpsr,ai==0&&bi==0);
  }
  for(unsigned sample=0;sample<128;sample++){
   auto in=inputs();const unsigned rn=(sample*3+10)&31,rd=(sample*5+8)&31,initial=sample&1?0x0800009f:2;const auto ref=scalar_reference(f,in.vectors[rn],fpcr,initial);
   execute(prepared(in,fpcr,initial),encoding(f,rn,rd),ref.value,ref.fpsr,sample==0);
  }
 }
}
void traps_and_rollback(){
 for(auto f:forms){MinimalGuest g(128*1024*1024);g.load_elf(elf({encoding(f,10,8)},true),true);g.run(1);check(g.machine().exceptions==1&&(g.machine().esr>>26)==7&&!g.floating().nonzero(),"Pairwise bypassed the original FP access trap");}
 std::vector<std::uint32_t> rejected{0x7eb0d800u,0x7ef0d800u};
 // Original profile implements S/D arithmetic; FP16 arithmetic is not advertised.
 for(auto f:forms)rejected.push_back(encoding(f,10,8)&~0x20000000u);
 for(auto op:rejected){MinimalGuest g(65536);g.load_elf(elf({op}));const auto before=g.snapshot();bool failed=false;try{g.run(1);}catch(const std::exception&){failed=true;}check(failed&&g.snapshot()==before,"Reserved/unsupported pairwise form changed state");}
}
#ifdef __aarch64__
struct NativeCode {
 void* page=nullptr;std::size_t bytes=0;
 explicit NativeCode(std::uint32_t op){
  bytes=std::size_t(::sysconf(_SC_PAGESIZE));page=::mmap(nullptr,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);check(page!=MAP_FAILED,"Cannot allocate actual AArch64 FP pairwise oracle");auto* code=static_cast<std::uint32_t*>(page);unsigned at=0;
  code[at++]=0xd10203ffu;for(unsigned r=8;r<16;r+=2)code[at++]=0xad000000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;
  code[at++]=0xaa0003efu;code[at++]=0xaa0103eeu;const unsigned rn=(op>>5)&31,rm=(op>>16)&31,rd=op&31;
  for(unsigned r=0;r<32;++r)code[at++]=0x3dc00000u|(r<<10)|(15<<5)|r;
  code[at++]=op;code[at++]=0x3d800000u|(14<<5)|rd;
  for(unsigned r=8;r<16;r+=2)code[at++]=0xad400000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;code[at++]=0x910203ffu;code[at++]=0xd65f03c0u;
  __builtin___clear_cache(static_cast<char*>(page),static_cast<char*>(page)+at*4);check(::mprotect(page,bytes,PROT_READ|PROT_EXEC)==0,"Cannot publish actual AArch64 FP pairwise oracle");
 }
 ~NativeCode(){if(page&&page!=MAP_FAILED)::munmap(page,bytes);}
 Vector128 run(const Inputs& values,unsigned fpcr,unsigned& flags){
  std::uint64_t oldcr,oldsr,sr;asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr":"=r"(oldcr),"=r"(oldsr));const std::uint64_t cr=fpcr,initial=2;asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(cr),"r"(initial):"memory");Vector128 out;reinterpret_cast<void(*)(const Inputs*,Vector128*)>(page)(&values,&out);asm volatile("mrs %0, fpsr":"=r"(sr));asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(oldcr),"r"(oldsr):"memory");flags=unsigned(sr);return out;
 }
};

void hardware_oracle(){
 for(auto f:forms){auto in=inputs();const auto base=prepared(in);for(unsigned rn=0;rn<32;rn++)for(unsigned rd=0;rd<32;rd++){const auto op=encoding(f,rn,rd);NativeCode native(op);unsigned flags=0;const auto expected=native.run(in,0,flags);execute(base,op,expected,flags,false);++native_cases;}
  for(unsigned sample=0;sample<2048;sample++){
   auto in=inputs();const unsigned mode=sample%16,fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0),rn=(sample*3+10)&31,rd=(sample*5+8)&31;const auto values=edges(f.bits);
   if(sample<values.size()*values.size()){in.vectors[rn].lane(0,f.bits,values[sample%values.size()]);in.vectors[rn].lane(1,f.bits,values[(sample/values.size())%values.size()]);}
   const auto op=encoding(f,rn,rd);NativeCode native(op);unsigned flags=0;const auto expected=native.run(in,fpcr,flags);execute(prepared(in,fpcr),op,expected,flags,sample==0);++native_cases;
  }
 }
}
#else
void hardware_oracle(){}
#endif
}
int main(){try{allocated_forms();numerical_and_random();traps_and_rollback();hardware_oracle();std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"native_hardware_cases\":"<<native_cases<<",\"scalar_pairwise_forms\":10,\"exact_android_opcode\":\"0x7e30d8e7\",\"android_boot\":false}\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
