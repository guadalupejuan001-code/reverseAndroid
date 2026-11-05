// Vector floating integral-rounding family, including the Android FRINTM stop.
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



struct Form {unsigned bits,width;};
const Form forms[]{{32,64},{32,128},{64,128}};
enum Operation {Nearest,Plus,Minus,Zero,Away,Current,Exact};
const Operation operations[]{Nearest,Plus,Minus,Zero,Away,Current,Exact};
unsigned lanes(Form f){return f.width/f.bits;}
std::uint32_t encoding(Form f,Operation op,unsigned rn,unsigned rd){
 const std::uint32_t base[]{0x0e218800u,0x0ea18800u,0x0e219800u,0x0ea19800u,0x2e218800u,0x2ea19800u,0x2e219800u};
 return base[op]|(f.width==128?0x40000000u:0)|(f.bits==64?0x00400000u:0)|(rn<<5)|rd;
}
void execute(MinimalGuest g,std::uint32_t op,Vector128 expected,unsigned flags,bool restore){
 g.write(g.registers().pc,op,4);const auto cpu=g.registers();const auto fp=g.floating();const auto memory=g.runtime().memory_persistent_snapshot(),core=g.runtime().core_snapshot();
 if(restore){auto resumed=MinimalGuest::from_snapshot(g.snapshot());g.run(1);resumed.run(1);check(g.snapshot()==resumed.snapshot(),"Vector FRINT changed after original checkpoint decoding");}else g.run(1);
 const unsigned rd=op&31;check(g.floating().fpsr==(fp.fpsr|flags)&&g.floating().fpcr==fp.fpcr,"Vector FRINT FPCR/FPSR differs");
 check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"Vector FRINT did not advance exactly one instruction");
 check(g.registers().sp==cpu.sp&&g.registers().nzcv==cpu.nzcv,"Vector FRINT changed SP or NZCV");
 check(g.runtime().memory_persistent_snapshot()==memory&&g.runtime().core_snapshot()==core,"Vector FRINT changed XI/Ether memory/core");
 for(unsigned r=0;r<31;++r)check(g.registers().x[r]==cpu.x[r],"Vector FRINT changed a GPR");
 for(unsigned r=0;r<32;++r){const auto value=r==rd?expected:fp.v[r];check(g.floating().v[r].low==value.low&&g.floating().v[r].high==value.high,"Vector FRINT destination/alias/upper SIMD bits differ");}++cases;
}


std::pair<std::uint64_t,unsigned> reference(unsigned bits,std::uint64_t x,Operation op,unsigned fpcr){
 const auto sign=bits==32?UINT64_C(0x80000000):UINT64_C(0x8000000000000000),exponent=bits==32?UINT64_C(0x7f800000):UINT64_C(0x7ff0000000000000),fraction=bits==32?UINT64_C(0x7fffff):UINT64_C(0xfffffffffffff),quiet=bits==32?UINT64_C(0x400000):UINT64_C(0x8000000000000),mask=bits==32?UINT64_C(0xffffffff):UINT64_MAX;
 x&=mask;if((x&exponent)==exponent){if(!(x&fraction))return {x,0};return {(fpcr&(1u<<25))?exponent|quiet:x|quiet,(x&quiet)?0u:1u};}
 if((fpcr&(1u<<24))&&!(x&exponent)&&(x&fraction))return {x&sign,128};
 if(!(x&~sign))return {x,0};long double value;
 if(bits==32){float f;auto w=std::uint32_t(x);std::memcpy(&f,&w,4);value=f;}else{double d;std::memcpy(&d,&x,8);value=d;}
 const unsigned mode=op==Current||op==Exact?(fpcr>>22)&3:unsigned(op);long double result;
 if(mode==1)result=std::ceil(value);else if(mode==2)result=std::floor(value);else if(mode==3)result=std::trunc(value);else if(mode==4)result=std::round(value);
 else{result=std::floor(value);const auto part=value-result;if(part>.5L||(part==.5L&&std::fmod(result,2.0L)!=0))result+=1;}
 auto output=raw(double(result),bits);if(result==0)output=x&sign;
 return {output,op==Exact&&result!=value?16u:0u};
}
Inputs finite_inputs(unsigned bits){Inputs in;for(unsigned r=0;r<32;++r){in.gpr[r]=UINT64_C(0xa55a5aa5ffff0000)+r;for(unsigned e=0;e<128/bits;++e)in.vectors[r].lane(e,bits,raw(double(r)-16+double(e)/4,bits));}return in;}
void allocated_forms(){
 for(auto f:forms)for(auto op:operations){auto in=finite_inputs(f.bits);const auto base=prepared(in);
  for(unsigned rn=0;rn<32;++rn)for(unsigned rd=0;rd<32;++rd){Vector128 expected;unsigned flags=0;for(unsigned e=0;e<lanes(f);++e){auto v=reference(f.bits,in.vectors[rn].lane(e,f.bits),op,0);expected.lane(e,f.bits,v.first);flags|=v.second;}execute(base,encoding(f,op,rn,rd),expected,flags,rn==rd);}
 }
 Inputs actual;execute(prepared(actual,0,16),0x0e219821u,{0,0},0,true);
}
const std::uint64_t floating32[]{0,0x80000000,1,0x80000001,0x3e000000,0xbe000000,0x3eaaaaab,0xbeaaaaab,0x3f000000,0xbf000000,0x3fc00000,0xbfc00000,0x40200000,0xc0200000,0x40600000,0xc0600000,0x3f800000,0xbf800000,0x4f000000,0xcf000000,0x7f800000,0xff800000,0x7fc12345,0x7f812345,0xffc12345,0xff812345,0x7f7fffff,0xff7fffff,0x00800000,0x007fffff,0x80000001,0x4affffff};
const std::uint64_t floating64[]{0,UINT64_C(0x8000000000000000),1,UINT64_C(0x8000000000000001),UINT64_C(0x3fc0000000000000),UINT64_C(0xbfc0000000000000),UINT64_C(0x3fd5555555555555),UINT64_C(0xbfd5555555555555),UINT64_C(0x3fe0000000000000),UINT64_C(0xbfe0000000000000),UINT64_C(0x3ff8000000000000),UINT64_C(0xbff8000000000000),UINT64_C(0x4004000000000000),UINT64_C(0xc004000000000000),UINT64_C(0x400c000000000000),UINT64_C(0xc00c000000000000),UINT64_C(0x3ff0000000000000),UINT64_C(0xbff0000000000000),UINT64_C(0x43e0000000000000),UINT64_C(0xc3e0000000000000),UINT64_C(0x7ff0000000000000),UINT64_C(0xfff0000000000000),UINT64_C(0x7ff8000000012345),UINT64_C(0x7ff0000000012345),UINT64_C(0xfff8000000012345),UINT64_C(0xfff0000000012345),UINT64_C(0x7fefffffffffffff),UINT64_C(0xffefffffffffffff),UINT64_C(0x0010000000000000),UINT64_C(0x000fffffffffffff),UINT64_C(0x8000000000000001),UINT64_C(0x432fffffffffffff)};
void numerical_examples(){
 for(auto f:forms)for(auto op:operations)for(unsigned mode=0;mode<16;++mode){const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);
  for(unsigned sample=0;sample<32;++sample){Inputs in;for(unsigned r=0;r<32;++r){in.gpr[r]=UINT64_C(0xa55a5aa5ffff0000)+r;for(unsigned e=0;e<128/f.bits;++e)in.vectors[r].lane(e,f.bits,f.bits==32?UINT64_C(0x7f800001):UINT64_C(0x7ff0000000000001));}
   Vector128 expected;unsigned flags=0;for(unsigned e=0;e<lanes(f);++e){auto x=f.bits==32?floating32[(sample+e)%32]:floating64[(sample+e)%32];in.vectors[10].lane(e,f.bits,x);auto v=reference(f.bits,x,op,fpcr);expected.lane(e,f.bits,v.first);flags|=v.second;}
   execute(prepared(in,fpcr),encoding(f,op,10,8),expected,flags,sample%7==0);
  }
 }
}
void traps_and_rollback(){
 for(auto f:forms)for(auto op:operations){MinimalGuest g(128*1024*1024);g.load_elf(elf({encoding(f,op,10,8)},true),true);g.run(1);check(g.machine().exceptions==1&&(g.machine().esr>>26)==7&&!g.floating().nonzero(),"Vector FRINT bypassed original FP access trap");}
 for(auto op:operations){MinimalGuest bad(65536);bad.load_elf(elf({encoding({64,64},op,10,8)}));const auto before=bad.snapshot();bool rejected=false;try{bad.run(1);}catch(const std::exception&){rejected=true;}check(rejected&&bad.snapshot()==before,"Unallocated vector 1D FRINT changed state");}
}
#ifdef __aarch64__
struct NativeCode {
 void* page=nullptr;std::size_t bytes=0;
 explicit NativeCode(std::uint32_t op){
  bytes=std::size_t(::sysconf(_SC_PAGESIZE));page=::mmap(nullptr,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);check(page!=MAP_FAILED,"Cannot allocate actual AArch64 vector FRINT oracle");auto* code=static_cast<std::uint32_t*>(page);unsigned at=0;
  code[at++]=0xd10203ffu;for(unsigned r=8;r<16;r+=2)code[at++]=0xad000000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;
  code[at++]=0xaa0003efu;code[at++]=0xaa0103eeu;const unsigned rn=(op>>5)&31,rm=(op>>16)&31,rd=op&31;
  for(unsigned r=0;r<32;++r)code[at++]=0x3dc00000u|(r<<10)|(15<<5)|r;
  code[at++]=op;code[at++]=0x3d800000u|(14<<5)|rd;
  for(unsigned r=8;r<16;r+=2)code[at++]=0xad400000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;code[at++]=0x910203ffu;code[at++]=0xd65f03c0u;
  __builtin___clear_cache(static_cast<char*>(page),static_cast<char*>(page)+at*4);check(::mprotect(page,bytes,PROT_READ|PROT_EXEC)==0,"Cannot publish actual AArch64 vector FRINT oracle");
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
 std::uint64_t seed=0x0e219821;auto random=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
 for(auto f:forms)for(auto op:operations)for(unsigned mode=0;mode<16;++mode)for(unsigned sample=0;sample<256;++sample){const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0),rn=(sample*3+10)&31,rd=(sample*5+8)&31;Inputs in;
  for(unsigned r=0;r<32;++r){in.gpr[r]=random();in.vectors[r]={random(),random()};}
  if(sample<64)for(unsigned e=0;e<lanes(f);++e)in.vectors[rn].lane(e,f.bits,f.bits==32?floating32[(sample+e)%32]:floating64[(sample+e)%32]);
  const auto word=encoding(f,op,rn,rd);NativeCode native(word);unsigned flags;const auto result=native.run(in,fpcr,flags);execute(prepared(in,fpcr),word,result,flags,sample==0);++native_cases;
 }
}
#else
void hardware_oracle(){}
#endif
}
int main(){try{allocated_forms();numerical_examples();traps_and_rollback();hardware_oracle();std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"native_hardware_cases\":"<<native_cases<<",\"exact_c58_opcode\":\"0x0e219821\",\"android_boot\":false}\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
