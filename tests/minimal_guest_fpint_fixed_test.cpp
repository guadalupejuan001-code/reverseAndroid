// Exact SurfaceFlinger FCVTZS #8 stop and allocated fixed-point FP conversions.
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
MinimalGuest prepared(const Vectors& values,unsigned fpcr=0,unsigned fpsr=10){
 std::vector<std::uint32_t> code{0xd2840002u};
 for(unsigned r=0;r<32;r++)code.push_back(0x3dc00040u|(r<<10)|r);
 constant(code,3,fpcr);code.push_back(0xd51b4403u);
 constant(code,3,fpsr);code.push_back(0xd51b4423u);
 code.push_back(0xf100045fu);code.push_back(0xd503201fu);
 MinimalGuest g(65536);g.load_elf(elf(code));
 for(unsigned r=0;r<32;r++){g.write(0x2000+r*16,values[r].low,8);g.write(0x2008+r*16,values[r].high,8);}
 g.run(code.size()-1);return g;
}

std::uint32_t encoding(unsigned bits,bool wide,bool uns,unsigned rn,unsigned rd,unsigned frac,bool scalar=false){
 return (scalar?0x5f00fc00u:0x0f00fc00u)|(!scalar&&wide?0x40000000u:0)|(uns?0x20000000u:0)|((2*bits-frac)<<16)|(rn<<5)|rd;
}
struct Form {unsigned bits;bool wide,scalar;};
const Form forms[]{{32,false,false},{32,true,false},{64,true,false},{32,false,true},{64,false,true}};
unsigned lanes(Form f){return f.scalar?1u:(f.wide?128u:64u)/f.bits;}
std::pair<std::uint64_t,unsigned> reference(unsigned bits,std::uint64_t value,bool uns,unsigned frac,unsigned fpcr){
 const auto mask=bits==32?UINT64_C(0xffffffff):UINT64_MAX;
 const auto exponent=bits==32?UINT64_C(0x7f800000):UINT64_C(0x7ff0000000000000);
 const auto fraction=bits==32?UINT64_C(0x7fffff):UINT64_C(0xfffffffffffff);
 if((fpcr&(1u<<24))&&!(value&exponent)&&(value&fraction))return {0,128};
 long double x;
 if(bits==32){float f;const auto v=std::uint32_t(value);std::memcpy(&f,&v,4);x=f;}
 else{double d;std::memcpy(&d,&value,8);x=d;}
 if(std::isnan(x))return {0,1};
 const auto scaled=std::ldexp(x,int(frac)),rounded=std::trunc(scaled);
 const auto upper=std::ldexp(1.0L,int(uns?bits:bits-1)),lower=uns?0.0L:-upper;
 if(rounded>=upper)return {uns?mask:(UINT64_C(1)<<(bits-1))-1,1};
 if(rounded<lower)return {uns?0:UINT64_C(1)<<(bits-1),1};
 const auto result=uns?static_cast<std::uint64_t>(rounded):std::uint64_t(static_cast<std::int64_t>(rounded));
 return {result&mask,scaled==rounded?0u:16u};
}
void execute(MinimalGuest g,std::uint32_t op,Vector128 expected,unsigned flags,bool restore){
 g.write(g.registers().pc,op,4);const auto cpu=g.registers();const auto fp=g.floating();
 const auto memory=g.runtime().memory_persistent_snapshot(),core=g.runtime().core_snapshot();
 if(restore){auto resumed=MinimalGuest::from_snapshot(g.snapshot());g.run(1);resumed.run(1);check(g.snapshot()==resumed.snapshot(),"Vector FP-to-integer conversion differs after original checkpoint restore");}
 else g.run(1);
 const auto rd=op&31;
 check(g.floating().v[rd].low==expected.low&&g.floating().v[rd].high==expected.high,"Vector FP-to-integer conversion result, selected lane or upper clearing differs");
 check(g.floating().fpsr==(fp.fpsr|flags)&&g.floating().fpcr==fp.fpcr,"Vector FP-to-integer conversion FPCR/FPSR differs");
 check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"Vector FP-to-integer conversion did not advance exactly one instruction");
 check(g.registers().x==cpu.x&&g.registers().sp==cpu.sp&&g.registers().nzcv==cpu.nzcv,"Vector FP-to-integer conversion changed integer CPU state");
 check(g.runtime().memory_persistent_snapshot()==memory&&g.runtime().core_snapshot()==core,"Vector FP-to-integer conversion changed XI/Ether memory or core");
 for(unsigned r=0;r<32;r++)if(r!=rd)check(g.floating().v[r].low==fp.v[r].low&&g.floating().v[r].high==fp.v[r].high,"Vector FP-to-integer conversion changed another vector register");
 ++cases;
}
void allocated_forms(){
 for(const auto f:forms){
  Vectors v;for(unsigned r=0;r<32;++r)for(unsigned n=0;n<128/f.bits;++n)v[r].lane(n,f.bits,raw(double(1+r+7*n)/256,f.bits));
  const auto base=prepared(v);
  for(bool uns:{false,true})for(unsigned rn=0;rn<32;++rn)for(unsigned rd=0;rd<32;++rd){
   Vector128 out;for(unsigned n=0;n<lanes(f);++n)out.lane(n,f.bits,1+rn+7*n);
   execute(base,encoding(f.bits,f.wide,uns,rn,rd,8,f.scalar),out,0,rn==rd);
  }
 }
 Vectors v;v[0]={UINT64_C(0x3ec00000be000000),UINT64_C(0x3e000000bec00000)};
 Vector128 out{UINT64_C(0x00000060ffffffe0),UINT64_C(0x00000020ffffffa0)};
 execute(prepared(v,0,16),0x4f38fc00u,out,0,true);
 execute(prepared(v,0,0x0800009f),0x4f38fc00u,out,0,true);
}
const std::uint64_t s32[]{0,0x80000000,1,0x80000001,0x3f800000,0xbf800000,0x3e000000,0xbe000000,0x3eaaaaab,0xbeaaaaab,0x3f7fffff,0x3f800001,0x7f800000,0xff800000,0x7fc12345,0x7f812345,0x7f7fffff,0xff7fffff,0x00800000,0x007fffff,0x4f000000,0xcf000000,0x4f7fffff,0x4f800000};
const std::uint64_t s64[]{0,UINT64_C(0x8000000000000000),1,UINT64_C(0x8000000000000001),UINT64_C(0x3ff0000000000000),UINT64_C(0xbff0000000000000),UINT64_C(0x3fc0000000000000),UINT64_C(0xbfc0000000000000),UINT64_C(0x3fd5555555555555),UINT64_C(0xbfd5555555555555),UINT64_C(0x3fefffffffffffff),UINT64_C(0x3ff0000000000001),UINT64_C(0x7ff0000000000000),UINT64_C(0xfff0000000000000),UINT64_C(0x7ff8000000012345),UINT64_C(0x7ff0000000012345),UINT64_C(0x7fefffffffffffff),UINT64_C(0xffefffffffffffff),UINT64_C(0x0010000000000000),UINT64_C(0x000fffffffffffff),UINT64_C(0x43e0000000000000),UINT64_C(0xc3e0000000000000),UINT64_C(0x43efffffffffffff),UINT64_C(0x43f0000000000000)};
void numeric_examples(){
 for(const auto f:forms)for(unsigned frac=1;frac<=f.bits;++frac)for(bool uns:{false,true})for(unsigned mode=0;mode<16;++mode){
  const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);Vectors v;Vector128 out;unsigned flags=0;
  for(unsigned n=0;n<128/f.bits;++n){
   const unsigned index=(frac+mode+3*n)%24;const auto x=f.bits==32?s32[index]:s64[index];v[31].lane(n,f.bits,x);
   if(n<lanes(f)){const auto expected=reference(f.bits,x,uns,frac,fpcr);out.lane(n,f.bits,expected.first);flags|=expected.second;}
  }
  execute(prepared(v,fpcr),encoding(f.bits,f.wide,uns,31,mode&1?31:0,frac,f.scalar),out,flags,true);
 }
 // Precisely straddle every signed/unsigned integer boundary at every scale.
 for(const auto f:forms)for(unsigned frac=1;frac<=f.bits;++frac)for(bool uns:{false,true})for(int delta:{-1,0,1}){
  const auto centre=raw(std::ldexp(1.0,int(uns?f.bits:f.bits-1)-int(frac)),f.bits);
  Vectors v;Vector128 out;const auto x=std::uint64_t(centre+delta);const auto expected=reference(f.bits,x,uns,frac,0);
  for(unsigned n=0;n<128/f.bits;++n)v[31].lane(n,f.bits,x);for(unsigned n=0;n<lanes(f);++n)out.lane(n,f.bits,expected.first);
  execute(prepared(v),encoding(f.bits,f.wide,uns,31,31,frac,f.scalar),out,expected.second,true);
 }
}
void traps_and_rollback(){
 for(auto f:forms)for(bool uns:{false,true}){
  MinimalGuest g(128*1024*1024);g.load_elf(elf({encoding(f.bits,f.wide,uns,1,0,8,f.scalar)},true),true);g.run(1);
  check(g.machine().exceptions==1&&(g.machine().esr>>26)==7&&!g.floating().nonzero(),"Fixed-point FP conversion bypassed FP access trap");
 }
 for(auto op:{0x0f40fc20u,0x2f40fc20u,0x0f18fc20u,0x4f18fc20u,0x5f18fc20u,0x7f18fc20u,0x0f08fc20u,0x5f08fc20u,0x0f00fc20u,0x5f00fc20u}){
  MinimalGuest g(65536);g.load_elf(elf({op}));const auto before=g.snapshot();bool rejected=false;
  try{g.run(1);}catch(const std::exception&){rejected=true;}
  check(rejected&&g.snapshot()==before,"Reserved or unsupported fixed FP conversion changed guest state");
 }
}
#ifdef __aarch64__
struct NativeCode {
 void* page=nullptr;std::size_t bytes=0;
 explicit NativeCode(std::uint32_t op){
  bytes=std::size_t(::sysconf(_SC_PAGESIZE));page=::mmap(nullptr,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  check(page!=MAP_FAILED,"Cannot allocate native instruction oracle");
  auto* code=static_cast<std::uint32_t*>(page);const unsigned rn=(op>>5)&31,rd=op&31;
  // Only caller-saved V0/V2/V3/V16/V31 are used by the hardware cases.
  unsigned i=0;for(const unsigned r:{rn,rd})code[i++]=0x3dc00000u|(r<<10)|r;
  code[i++]=op;code[i++]=0x3d800020u|rd;code[i++]=0xd65f03c0u;
  __builtin___clear_cache(static_cast<char*>(page),static_cast<char*>(page)+i*4);
  check(::mprotect(page,bytes,PROT_READ|PROT_EXEC)==0,"Cannot publish native instruction oracle");
 }
 ~NativeCode(){if(page&&page!=MAP_FAILED)::munmap(page,bytes);}
 NativeCode(const NativeCode&)=delete;
 Vector128 run(const Vectors& values,unsigned fpcr,unsigned& flags){
  std::uint64_t oldcr,oldsr,sr;
  asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr":"=r"(oldcr),"=r"(oldsr));
  const std::uint64_t cr=fpcr,initial=10;
  asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(cr),"r"(initial):"memory");
  Vector128 out;reinterpret_cast<void(*)(const Vector128*,Vector128*)>(page)(values.data(),&out);
  asm volatile("mrs %0, fpsr":"=r"(sr));
  asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(oldcr),"r"(oldsr):"memory");
  flags=unsigned(sr);return out;
 }
};

void hardware_oracle(){
 std::uint64_t seed=0x4f38fc00;auto random=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
 for(const auto f:forms)for(unsigned frac=1;frac<=f.bits;++frac)for(bool uns:{false,true})for(unsigned rn:{0u,31u}){
  const unsigned rd=0;const auto op=encoding(f.bits,f.wide,uns,rn,rd,frac,f.scalar);NativeCode native(op);
  for(unsigned mode=0;mode<16;++mode)for(unsigned sample=0;sample<8;++sample){
   const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);Vectors v;
   for(unsigned r:{rn,rd})for(unsigned n=0;n<128/f.bits;++n)v[r].lane(n,f.bits,sample<6?(f.bits==32?s32[(sample*3+frac+n)%24]:s64[(sample*3+frac+n)%24]):random());
   unsigned flags=0;const auto expected=native.run(v,fpcr,flags);execute(prepared(v,fpcr),op,expected,flags,false);++native_cases;
  }
 }
}
#else
void hardware_oracle(){}
#endif
}
int main(){try{allocated_forms();numeric_examples();traps_and_rollback();hardware_oracle();std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"native_hardware_cases\":"<<native_cases<<",\"exact_c50_opcode\":\"0x4f38fc00\",\"android_boot\":false}\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
