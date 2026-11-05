// Floating compare-with-zero family, including the observed Android stop.
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


struct Form {bool scalar;unsigned bits,width;};
const Form forms[]{{true,32,128},{true,64,128},{false,32,64},{false,32,128},{false,64,128}};
enum Operation {EQ,GE,GT,LE,LT};
const Operation operations[]{EQ,GE,GT,LE,LT};
unsigned lanes(Form f){return f.scalar?1u:f.width/f.bits;}
std::uint32_t encoding(Form f,Operation op,unsigned rn,unsigned rd){
 const std::uint32_t base[]{0x0ea0d800u,0x2ea0c800u,0x0ea0c800u,0x2ea0d800u,0x0ea0e800u};
 return base[op]|(f.scalar?0x50000000u:(f.width==128?0x40000000u:0))|(f.bits==64?0x00400000u:0)|(rn<<5)|rd;
}
void execute(MinimalGuest g,std::uint32_t op,Vector128 expected,unsigned flags,bool restore){
 g.write(g.registers().pc,op,4);const auto cpu=g.registers();const auto fp=g.floating();const auto memory=g.runtime().memory_persistent_snapshot(),core=g.runtime().core_snapshot();
 if(restore){auto resumed=MinimalGuest::from_snapshot(g.snapshot());g.run(1);resumed.run(1);check(g.snapshot()==resumed.snapshot(),"Floating zero compare changed after original checkpoint decoding");}else g.run(1);
 const unsigned rd=op&31;check(g.floating().fpsr==(fp.fpsr|flags)&&g.floating().fpcr==fp.fpcr,"Floating zero compare FPCR/FPSR differs");
 check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"Floating zero compare did not advance exactly one instruction");
 check(g.registers().sp==cpu.sp&&g.registers().nzcv==cpu.nzcv,"Floating zero compare changed SP or NZCV");
 check(g.runtime().memory_persistent_snapshot()==memory&&g.runtime().core_snapshot()==core,"Floating zero compare changed XI/Ether memory/core");
 for(unsigned r=0;r<31;++r)check(g.registers().x[r]==cpu.x[r],"Floating zero compare changed a GPR");
 for(unsigned r=0;r<32;++r){const auto value=r==rd?expected:fp.v[r];check(g.floating().v[r].low==value.low&&g.floating().v[r].high==value.high,"Floating zero compare destination/alias/upper SIMD bits differ");}++cases;
}

std::pair<std::uint64_t,unsigned> reference(unsigned bits,std::uint64_t x,Operation op,unsigned fpcr){
 const auto sign=bits==32?UINT64_C(0x80000000):UINT64_C(0x8000000000000000),exponent=bits==32?UINT64_C(0x7f800000):UINT64_C(0x7ff0000000000000),fraction=bits==32?UINT64_C(0x7fffff):UINT64_C(0xfffffffffffff),quiet=bits==32?UINT64_C(0x400000):UINT64_C(0x8000000000000),mask=bits==32?UINT64_C(0xffffffff):UINT64_MAX;
 x&=mask;const bool nan=(x&exponent)==exponent&&(x&fraction);if(nan)return {0,(op!=EQ||!(x&quiet))?1u:0u};
 unsigned flags=0;if((fpcr&(1u<<24))&&!(x&exponent)&&(x&fraction)){flags=128;x&=sign;}
 const bool zero=!(x&~sign),negative=(x&sign)&&!zero,positive=!(x&sign)&&!zero;
 const bool match=op==EQ?zero:op==GE?positive||zero:op==GT?positive:op==LE?negative||zero:negative;
 return {match?mask:0,flags};
}
Inputs finite_inputs(unsigned bits){Inputs in;for(unsigned r=0;r<32;++r){in.gpr[r]=UINT64_C(0xa55a5aa5ffff0000)+r;for(unsigned lane=0;lane<128/bits;++lane)in.vectors[r].lane(lane,bits,raw(double(r)-16+double(lane)/4,bits));}return in;}
void allocated_forms(){
 for(auto f:forms)for(auto op:operations){auto in=finite_inputs(f.bits);const auto base=prepared(in);
  for(unsigned rn=0;rn<32;++rn)for(unsigned rd=0;rd<32;++rd){Vector128 expected;unsigned flags=0;for(unsigned e=0;e<lanes(f);++e){auto v=reference(f.bits,in.vectors[rn].lane(e,f.bits),op,0);expected.lane(e,f.bits,v.first);flags|=v.second;}execute(base,encoding(f,op,rn,rd),expected,flags,rn==rd);}
 }
 Inputs actual;actual.vectors[4].low=0x34324241;execute(prepared(actual,0,16),0x2ea0d824u,{UINT64_MAX,0},0,true);
}
const std::uint64_t floating32[]{0,0x80000000,1,0x80000001,0x3e000000,0xbe000000,0x3eaaaaab,0xbeaaaaab,0x3f800000,0xbf800000,0x4f000000,0xcf000000,0x4f7fffff,0x4f800000,0x7f800000,0xff800000,0x7fc12345,0x7f812345,0x7f7fffff,0xff7fffff,0x00800000,0x007fffff};
const std::uint64_t floating64[]{0,UINT64_C(0x8000000000000000),1,UINT64_C(0x8000000000000001),UINT64_C(0x3fc0000000000000),UINT64_C(0xbfc0000000000000),UINT64_C(0x3fd5555555555555),UINT64_C(0xbfd5555555555555),UINT64_C(0x3ff0000000000000),UINT64_C(0xbff0000000000000),UINT64_C(0x43e0000000000000),UINT64_C(0xc3e0000000000000),UINT64_C(0x43efffffffffffff),UINT64_C(0x43f0000000000000),UINT64_C(0x7ff0000000000000),UINT64_C(0xfff0000000000000),UINT64_C(0x7ff8000000012345),UINT64_C(0x7ff0000000012345),UINT64_C(0x7fefffffffffffff),UINT64_C(0xffefffffffffffff),UINT64_C(0x0010000000000000),UINT64_C(0x000fffffffffffff)};
void numerical_examples(){
 for(auto f:forms)for(auto op:operations)for(unsigned mode=0;mode<16;++mode){const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);
  for(unsigned sample=0;sample<22;++sample){Inputs in;for(unsigned r=0;r<32;++r){in.gpr[r]=UINT64_C(0xa55a5aa5ffff0000)+r;for(unsigned e=0;e<128/f.bits;++e)in.vectors[r].lane(e,f.bits,f.bits==32?UINT64_C(0x7f800001):UINT64_C(0x7ff0000000000001));}
   Vector128 expected;unsigned flags=0;for(unsigned e=0;e<lanes(f);++e){auto x=f.bits==32?floating32[(sample+e)%22]:floating64[(sample+e)%22];in.vectors[10].lane(e,f.bits,x);auto v=reference(f.bits,x,op,fpcr);expected.lane(e,f.bits,v.first);flags|=v.second;}
   execute(prepared(in,fpcr),encoding(f,op,10,8),expected,flags,sample%7==0);
  }
 }
}
void traps_and_rollback(){
 for(auto f:forms)for(auto op:operations){MinimalGuest g(128*1024*1024);g.load_elf(elf({encoding(f,op,10,8)},true),true);g.run(1);check(g.machine().exceptions==1&&(g.machine().esr>>26)==7&&!g.floating().nonzero(),"Floating zero compare bypassed original FP access trap");}
 std::vector<std::uint32_t> invalid{0x2ea0e800u,0x6ea0e800u,0x7ea0e800u};
 for(auto op:operations){invalid.push_back(encoding({false,64,64},op,10,8));invalid.push_back(encoding({true,32,128},op,10,8)^0x40000000u);}
 for(auto op:invalid){MinimalGuest bad(65536);bad.load_elf(elf({op}));const auto before=bad.snapshot();bool rejected=false;try{bad.run(1);}catch(const std::exception&){rejected=true;}check(rejected&&bad.snapshot()==before,"Unallocated floating zero compare changed guest state");}
}
#ifdef __aarch64__
struct NativeCode {
 void* page=nullptr;std::size_t bytes=0;
 explicit NativeCode(std::uint32_t op){
  bytes=std::size_t(::sysconf(_SC_PAGESIZE));page=::mmap(nullptr,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);check(page!=MAP_FAILED,"Cannot allocate actual AArch64 floating zero compare oracle");auto* code=static_cast<std::uint32_t*>(page);unsigned at=0;
  code[at++]=0xd10203ffu;for(unsigned r=8;r<16;r+=2)code[at++]=0xad000000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;
  code[at++]=0xaa0003efu;code[at++]=0xaa0103eeu;const unsigned rn=(op>>5)&31,rm=(op>>16)&31,rd=op&31;
  for(unsigned r:{rn,rm,rd})code[at++]=0x3dc00000u|(r<<10)|(15<<5)|r;
  code[at++]=op;code[at++]=0x3d800000u|(14<<5)|rd;
  for(unsigned r=8;r<16;r+=2)code[at++]=0xad400000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;code[at++]=0x910203ffu;code[at++]=0xd65f03c0u;
  __builtin___clear_cache(static_cast<char*>(page),static_cast<char*>(page)+at*4);check(::mprotect(page,bytes,PROT_READ|PROT_EXEC)==0,"Cannot publish actual AArch64 floating zero compare oracle");
 }
 ~NativeCode(){if(page&&page!=MAP_FAILED)::munmap(page,bytes);}
 Vector128 run(const Inputs& values,unsigned fpcr,unsigned& flags){
  std::uint64_t oldcr,oldsr,sr;asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr":"=r"(oldcr),"=r"(oldsr));const std::uint64_t cr=fpcr,initial=2;asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(cr),"r"(initial):"memory");Vector128 out;reinterpret_cast<void(*)(const Inputs*,Vector128*)>(page)(&values,&out);asm volatile("mrs %0, fpsr":"=r"(sr));asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(oldcr),"r"(oldsr):"memory");flags=unsigned(sr);return out;
 }
};

void hardware_oracle(){
 for(auto f:forms)for(auto op:operations){auto in=finite_inputs(f.bits);const auto base=prepared(in);
  for(unsigned rn=0;rn<32;++rn)for(unsigned rd=0;rd<32;++rd){const auto word=encoding(f,op,rn,rd);NativeCode native(word);unsigned flags;const auto result=native.run(in,0,flags);execute(base,word,result,flags,false);++native_cases;}
 }
 std::uint64_t seed=0x2ea0d824;auto random=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
 for(auto f:forms)for(auto op:operations)for(unsigned mode=0;mode<16;++mode)for(unsigned sample=0;sample<256;++sample){const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0),rn=(sample*3+10)&31,rd=(sample*5+8)&31;Inputs in;
  for(unsigned r=0;r<32;++r){in.gpr[r]=random();in.vectors[r]={random(),random()};}
  if(sample<44)for(unsigned e=0;e<lanes(f);++e)in.vectors[rn].lane(e,f.bits,f.bits==32?floating32[(sample+e)%22]:floating64[(sample+e)%22]);
  const auto word=encoding(f,op,rn,rd);NativeCode native(word);unsigned flags;const auto result=native.run(in,fpcr,flags);execute(prepared(in,fpcr),word,result,flags,sample==0);++native_cases;
 }
}
#else
void hardware_oracle(){}
#endif
}
int main(){try{allocated_forms();numerical_examples();traps_and_rollback();hardware_oracle();std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"native_hardware_cases\":"<<native_cases<<",\"exact_c56_opcode\":\"0x2ea0d824\",\"android_boot\":false}\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
