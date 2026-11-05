// Indexed FMUL S/D and vector forms, including the observed Android stop.
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
unsigned lanes(Form f){return f.scalar?1u:f.width/f.bits;}
unsigned indices(Form f){return 128/f.bits;}
std::uint32_t encoding(Form f,unsigned rn,unsigned rm,unsigned rd,unsigned index){
 return (f.scalar?0x5f009000u:0x0f009000u)|(!f.scalar&&f.width==128?0x40000000u:0)|((f.bits==32?2u:3u)<<22)|(rm<<16)|(f.bits==32?(index&1)<<21:0)|((f.bits==32?index>>1:index)<<11)|(rn<<5)|rd;
}
void execute(MinimalGuest g,std::uint32_t op,Vector128 expected,unsigned flags,bool restore){
 g.write(g.registers().pc,op,4);const auto cpu=g.registers();const auto fp=g.floating();const auto memory=g.runtime().memory_persistent_snapshot(),core=g.runtime().core_snapshot();
 if(restore){auto resumed=MinimalGuest::from_snapshot(g.snapshot());g.run(1);resumed.run(1);check(g.snapshot()==resumed.snapshot(),"Indexed FMUL changed after original checkpoint decoding");}else g.run(1);
 const unsigned rd=op&31;check(g.floating().fpsr==(fp.fpsr|flags)&&g.floating().fpcr==fp.fpcr,"Indexed FMUL FPCR/FPSR differs");
 check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"Indexed FMUL did not advance exactly one instruction");
 check(g.registers().sp==cpu.sp&&g.registers().nzcv==cpu.nzcv,"Indexed FMUL changed SP or NZCV");
 check(g.runtime().memory_persistent_snapshot()==memory&&g.runtime().core_snapshot()==core,"Indexed FMUL changed XI/Ether memory/core");
 for(unsigned r=0;r<31;++r)check(g.registers().x[r]==cpu.x[r],"Indexed FMUL changed a GPR");
 for(unsigned r=0;r<32;++r){const auto value=r==rd?expected:fp.v[r];check(g.floating().v[r].low==value.low&&g.floating().v[r].high==value.high,"Indexed FMUL destination/alias/upper SIMD bits differ");}++cases;
}

Inputs finite_inputs(unsigned bits){Inputs in;for(unsigned r=0;r<32;++r){in.gpr[r]=UINT64_C(0xa55a5aa5ffff0000)+r;for(unsigned e=0;e<128/bits;++e)in.vectors[r].lane(e,bits,raw(double(r+1)+double(e)/4,bits));}return in;}
void allocated_forms(){
 for(auto f:forms){auto in=finite_inputs(f.bits);const auto base=prepared(in);
  for(unsigned rn=0;rn<32;++rn)for(unsigned rm=0;rm<32;++rm)for(unsigned rd:{rn,rm,31u,(rn*7+rm*3+11)&31u,(rn*13+rm*17+7)&31u})for(unsigned index=0;index<indices(f);++index){
   Vector128 expected;for(unsigned e=0;e<lanes(f);++e)expected.lane(e,f.bits,raw((double(rn+1)+double(e)/4)*(double(rm+1)+double(index)/4),f.bits));
   execute(base,encoding(f,rn,rm,rd,index),expected,0,rn==rm&&rd==rn);
  }
 }
 Inputs actual;actual.vectors[5].low=raw(1,32);execute(prepared(actual,0,16),0x0f8190a1u,{0,0},0,true);
}
struct Example {std::uint64_t a,b,result;unsigned flags;};
std::vector<Example> examples(unsigned bits,unsigned fpcr){
 const auto sign=bits==32?UINT64_C(0x80000000):UINT64_C(0x8000000000000000),exponent=bits==32?UINT64_C(0x7f800000):UINT64_C(0x7ff0000000000000),quiet=bits==32?UINT64_C(0x400000):UINT64_C(0x8000000000000),normal=bits==32?UINT64_C(0x800000):UINT64_C(0x10000000000000),maximum=exponent-1,qnan=exponent|quiet,snan=exponent|0x12345,one=raw(1,bits);
 const bool fz=fpcr&(1u<<24),dn=fpcr&(1u<<25);const unsigned mode=(fpcr>>22)&3;
 return {{one,one,one,0},{sign|one,one,sign|one,0},{0,one,0,0},{sign,one,sign,0},{sign,sign|one,0,0},
  {raw(.5,bits),raw(.5,bits),raw(.25,bits),0},{raw(2,bits),raw(3,bits),raw(6,bits),0},
  {0,exponent,qnan,1},{sign,exponent,qnan,1},{exponent,one,exponent,0},{sign|exponent,one,sign|exponent,0},
  {sign|qnan|0x12345,one,qnan|(dn?0:sign|0x12345),0},
  {one,sign|qnan|0x23456,qnan|(dn?0:sign|0x23456),0},
  {sign|snan,one,qnan|(dn?0:sign|0x12345),1},
  {qnan|0x23456,sign|snan,qnan|(dn?0:sign|0x12345),1},
  {1,one,fz?0u:1u,fz?128u:0u},{sign|1,one,sign|(fz?0u:1u),fz?128u:0u},
  {normal,raw(.5,bits),fz?0:normal/2,fz?8u:0u},
  {normal,normal,fz?0u:(mode==1?1u:0u),fz?8u:24u},
  {normal-1,one,fz?0:normal-1,fz?128u:0u},
  {maximum,raw(2,bits),(mode==2||mode==3)?maximum:exponent,20},
  {sign|maximum,raw(2,bits),sign|((mode==1||mode==3)?maximum:exponent),20},
  {one+1,one+1,one+(mode==1?3:2),16},
  {sign|(one+1),one+1,sign|(one+(mode==2?3:2)),16}};
}
void numerical_examples(){
 for(auto f:forms)for(unsigned mode=0;mode<16;++mode){const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);const auto values=examples(f.bits,fpcr);
  for(unsigned sample=0;sample<values.size();++sample)for(unsigned index=0;index<indices(f);++index){Inputs in;for(unsigned r=0;r<32;++r){in.gpr[r]=UINT64_C(0xa55a5aa5ffff0000)+r;for(unsigned e=0;e<128/f.bits;++e)in.vectors[r].lane(e,f.bits,f.bits==32?UINT64_C(0x7f800001):UINT64_C(0x7ff0000000000001));}
   const auto x=values[sample];Vector128 expected;for(unsigned e=0;e<lanes(f);++e){in.vectors[10].lane(e,f.bits,x.a);expected.lane(e,f.bits,x.result);}in.vectors[11].lane(index,f.bits,x.b);
   execute(prepared(in,fpcr),encoding(f,10,11,8,index),expected,x.flags,sample%7==0);
  }
 }
}
void ordinary_fma_neighbors(){
 for(unsigned bits:{32u,64u})for(unsigned negate=0;negate<2;++negate)for(unsigned subtract=0;subtract<2;++subtract){
  auto in=finite_inputs(bits);in.vectors[10].low=raw(2,bits);in.vectors[11].low=raw(3,bits);in.vectors[4].low=raw(5,bits);
  auto g=prepared(in);const auto op=0x1f000000u|(bits==64?0x00400000u:0)|(negate<<21)|(subtract<<15)|(11u<<16)|(4u<<10)|(10u<<5)|8u;
  g.write(g.registers().pc,op,4);const auto cpu=g.registers();g.run(1);
  check((g.floating().v[8].low&~(UINT64_C(1)<<(bits-1)))==raw(subtract?1:11,bits)&&g.floating().v[8].high==0,"Indexed decoder intercepted an existing scalar FMA opcode");
  check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1&&g.registers().nzcv==cpu.nzcv&&g.floating().fpsr==2,"Existing scalar FMA advancement/flags changed");++cases;
 }
}
void traps_and_rollback(){
 for(auto f:forms){MinimalGuest g(128*1024*1024);g.load_elf(elf({encoding(f,10,11,8,0)},true),true);g.run(1);check(g.machine().exceptions==1&&(g.machine().esr>>26)==7&&!g.floating().nonzero(),"Indexed FMUL bypassed original FP access trap");}
 std::vector<std::uint32_t> invalid;
 for(auto f:forms){const auto op=encoding(f,10,11,8,0);for(unsigned size:{0u,1u})invalid.push_back((op&~0x00c00000u)|(size<<22));if(f.scalar)invalid.push_back(op^0x40000000u);if(f.bits==64)invalid.push_back(op|0x00200000u);}
 invalid.push_back(encoding({false,64,64},10,11,8,0));
 for(auto op:invalid){MinimalGuest bad(65536);bad.load_elf(elf({op}));const auto before=bad.snapshot();bool rejected=false;try{bad.run(1);}catch(const std::exception&){rejected=true;}check(rejected&&bad.snapshot()==before,"Unallocated/unsupported indexed FMUL changed guest state");}
}
#ifdef __aarch64__
struct NativeCode {
 void* page=nullptr;std::size_t bytes=0;
 explicit NativeCode(std::uint32_t op){
  bytes=std::size_t(::sysconf(_SC_PAGESIZE));page=::mmap(nullptr,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);check(page!=MAP_FAILED,"Cannot allocate actual AArch64 indexed FMUL oracle");auto* code=static_cast<std::uint32_t*>(page);unsigned at=0;
  code[at++]=0xd10203ffu;for(unsigned r=8;r<16;r+=2)code[at++]=0xad000000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;
  code[at++]=0xaa0003efu;code[at++]=0xaa0103eeu;const unsigned rn=(op>>5)&31,rm=(op>>16)&31,rd=op&31;
  for(unsigned r=0;r<32;++r)code[at++]=0x3dc00000u|(r<<10)|(15<<5)|r;
  code[at++]=op;code[at++]=0x3d800000u|(14<<5)|rd;
  for(unsigned r=8;r<16;r+=2)code[at++]=0xad400000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;code[at++]=0x910203ffu;code[at++]=0xd65f03c0u;
  __builtin___clear_cache(static_cast<char*>(page),static_cast<char*>(page)+at*4);check(::mprotect(page,bytes,PROT_READ|PROT_EXEC)==0,"Cannot publish actual AArch64 indexed FMUL oracle");
 }
 ~NativeCode(){if(page&&page!=MAP_FAILED)::munmap(page,bytes);}
 Vector128 run(const Inputs& values,unsigned fpcr,unsigned& flags){
  std::uint64_t oldcr,oldsr,sr;asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr":"=r"(oldcr),"=r"(oldsr));const std::uint64_t cr=fpcr,initial=2;asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(cr),"r"(initial):"memory");Vector128 out;reinterpret_cast<void(*)(const Inputs*,Vector128*)>(page)(&values,&out);asm volatile("mrs %0, fpsr":"=r"(sr));asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(oldcr),"r"(oldsr):"memory");flags=unsigned(sr);return out;
 }
};

void hardware_oracle(){
 for(unsigned bits:{32u,64u})for(unsigned negate=0;negate<2;++negate)for(unsigned subtract=0;subtract<2;++subtract){
  auto in=finite_inputs(bits);in.vectors[10].low=raw(2,bits);in.vectors[11].low=raw(3,bits);in.vectors[4].low=raw(5,bits);
  const auto op=0x1f000000u|(bits==64?0x00400000u:0)|(negate<<21)|(subtract<<15)|(11u<<16)|(4u<<10)|(10u<<5)|8u;NativeCode native(op);unsigned flags;const auto result=native.run(in,0,flags);execute(prepared(in),op,result,flags,true);++native_cases;
 }

 for(auto f:forms){auto in=finite_inputs(f.bits);const auto base=prepared(in);
  for(unsigned rn=0;rn<32;++rn)for(unsigned rm=0;rm<32;++rm)for(unsigned rd:{rn,rm,31u,(rn*7+rm*3+11)&31u,(rn*13+rm*17+7)&31u})for(unsigned index=0;index<indices(f);++index){const auto op=encoding(f,rn,rm,rd,index);NativeCode native(op);unsigned flags;const auto result=native.run(in,0,flags);execute(base,op,result,flags,false);++native_cases;}
 }
 std::uint64_t seed=0x0f8190a1;auto random=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
 for(auto f:forms)for(unsigned mode=0;mode<16;++mode)for(unsigned sample=0;sample<256;++sample){const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0),rn=(sample*3+10)&31,rm=(sample*7+11)&31,rd=(sample*5+8)&31,index=sample%indices(f);Inputs in;
  for(unsigned r=0;r<32;++r){in.gpr[r]=random();in.vectors[r]={random(),random()};}
  const auto values=examples(f.bits,fpcr);
  if(sample<values.size()){const auto v=values[sample];for(unsigned e=0;e<lanes(f);++e)in.vectors[rn].lane(e,f.bits,v.a);in.vectors[rm].lane(index,f.bits,v.b);}
  const auto op=encoding(f,rn,rm,rd,index);NativeCode native(op);unsigned flags;const auto result=native.run(in,fpcr,flags);execute(prepared(in,fpcr),op,result,flags,sample==0);++native_cases;
 }
}
#else
void hardware_oracle(){}
#endif
}
int main(){try{allocated_forms();numerical_examples();ordinary_fma_neighbors();traps_and_rollback();hardware_oracle();std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"native_hardware_cases\":"<<native_cases<<",\"exact_c57_opcode\":\"0x0f8190a1\",\"android_boot\":false}\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
