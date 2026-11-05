// Hardware composer fixed-point GPR/FP conversions, including the real stop.
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
MinimalGuest prepared(const Inputs& values,unsigned fpcr=0,unsigned fpsr=10){
 std::vector<std::uint32_t> code{0xd2840002u};for(unsigned r=0;r<32;++r)code.push_back(0x3dc00040u|(r<<10)|r);
 constant(code,3,fpcr);code.push_back(0xd51b4403u);constant(code,3,fpsr);code.push_back(0xd51b4423u);code.push_back(0xf100045fu);
 for(unsigned r=0;r<31;++r)constant(code,r,values.gpr[r]);code.push_back(0xd503201fu);
 MinimalGuest g(65536);g.load_elf(elf(code));for(unsigned r=0;r<32;++r){g.write(0x2000+r*16,values.vectors[r].low,8);g.write(0x2008+r*16,values.vectors[r].high,8);}g.run(code.size()-1);return g;
}
std::uint32_t encoding(bool to_integer,unsigned bits,unsigned integer_bits,bool uns,unsigned frac,unsigned rn,unsigned rd){
 return (to_integer?0x1e180000u:0x1e020000u)|(bits==64?0x00400000u:0)|(integer_bits==64?0x80000000u:0)|(uns?0x10000u:0)|((64-frac)<<10)|(rn<<5)|rd;
}
void execute(MinimalGuest g,std::uint32_t op,bool to_integer,Vector128 expected,unsigned flags,bool restore){
 g.write(g.registers().pc,op,4);const auto cpu=g.registers();const auto fp=g.floating();const auto memory=g.runtime().memory_persistent_snapshot(),core=g.runtime().core_snapshot();
 if(restore){auto resumed=MinimalGuest::from_snapshot(g.snapshot());g.run(1);resumed.run(1);check(g.snapshot()==resumed.snapshot(),"Fixed GPR conversion changed after original checkpoint decoding");}else g.run(1);
 const unsigned rd=op&31;check(g.floating().fpsr==(fp.fpsr|flags)&&g.floating().fpcr==fp.fpcr,"Fixed GPR conversion FPCR/FPSR differs");
 check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"Fixed GPR conversion did not advance exactly one instruction");
 check(g.registers().sp==cpu.sp&&g.registers().nzcv==cpu.nzcv,"Fixed GPR conversion changed SP or NZCV");
 check(g.runtime().memory_persistent_snapshot()==memory&&g.runtime().core_snapshot()==core,"Fixed GPR conversion changed XI/Ether memory/core");
 for(unsigned r=0;r<31;++r)check(g.registers().x[r]==(to_integer&&r==rd?expected.low:cpu.x[r]),"Fixed GPR conversion changed another register or failed W zero extension");
 for(unsigned r=0;r<32;++r){const auto value=!to_integer&&r==rd?expected:fp.v[r];check(g.floating().v[r].low==value.low&&g.floating().v[r].high==value.high,"Fixed GPR conversion source/upper SIMD bits differ");}++cases;
}
std::pair<std::uint64_t,unsigned> to_reference(unsigned bits,std::uint64_t value,unsigned integer_bits,bool uns,unsigned frac,unsigned fpcr){
 const auto exponent=bits==32?UINT64_C(0x7f800000):UINT64_C(0x7ff0000000000000),fraction=bits==32?UINT64_C(0x7fffff):UINT64_C(0xfffffffffffff),mask=integer_bits==32?UINT64_C(0xffffffff):UINT64_MAX;
 if((fpcr&(1u<<24))&&!(value&exponent)&&(value&fraction))return {0,128};
 long double x;if(bits==32){float f;auto v=std::uint32_t(value);std::memcpy(&f,&v,4);x=f;}else{double d;std::memcpy(&d,&value,8);x=d;}
 if(std::isnan(x))return {0,1};const auto scaled=std::ldexp(x,int(frac)),rounded=std::trunc(scaled),upper=std::ldexp(1.0L,int(uns?integer_bits:integer_bits-1)),lower=uns?0.0L:-upper;
 if(rounded>=upper)return {uns?mask:(UINT64_C(1)<<(integer_bits-1))-1,1};if(rounded<lower)return {uns?0:UINT64_C(1)<<(integer_bits-1),1};
 return {(uns?std::uint64_t(rounded):std::uint64_t(std::int64_t(rounded)))&mask,scaled==rounded?0u:16u};
}
// Independent integer IEEE rounding oracle; no SoftFloat/runtime helper calls.
std::pair<std::uint64_t,unsigned> from_reference(unsigned bits,std::uint64_t value,unsigned integer_bits,bool uns,unsigned frac,unsigned fpcr){
 const auto mask=integer_bits==32?UINT64_C(0xffffffff):UINT64_MAX;value&=mask;const bool negative=!uns&&bool(value&(UINT64_C(1)<<(integer_bits-1)));const auto magnitude=negative?(UINT64_C(0)-value)&mask:value;
 if(!magnitude)return {0,0};unsigned top=0;for(auto x=magnitude;x>>1;x>>=1)++top;const unsigned precision=bits==32?24:53;int exponent=int(top)-int(frac);std::uint64_t significand;unsigned flags=0;
 if(top<precision)significand=magnitude<<(precision-1-top);
 else{const unsigned shift=top-(precision-1);significand=magnitude>>shift;const auto remainder=magnitude&((UINT64_C(1)<<shift)-1),half=UINT64_C(1)<<(shift-1);const auto mode=(fpcr>>22)&3;
  if(remainder){flags=16;const bool up=mode==0?(remainder>half||(remainder==half&&(significand&1))):mode==1?!negative:mode==2?negative:false;if(up)++significand;}
  if(significand==(UINT64_C(1)<<precision)){significand>>=1;++exponent;}
 }
 const unsigned fraction_bits=precision-1,bias=bits==32?127:1023;check(exponent+int(bias)>0,"Independent fixed rational should be normal");
 return {(negative?UINT64_C(1)<<(bits-1):0)|(std::uint64_t(exponent+int(bias))<<fraction_bits)|(significand&((UINT64_C(1)<<fraction_bits)-1)),flags};
}
void allocated_forms(){
 for(bool to_integer:{false,true})for(unsigned bits:{32u,64u})for(unsigned integer_bits:{32u,64u})for(bool uns:{false,true}){
  Inputs in;for(unsigned r=0;r<32;++r){in.vectors[r]={raw(double(r+1)/256,bits),UINT64_MAX-r};in.gpr[r]=to_integer?UINT64_C(0xfedcba9876540000)+r:std::uint64_t(r+1)*256;}
  const auto base=prepared(in);for(unsigned rn=0;rn<32;++rn)for(unsigned rd=0;rd<32;++rd){Vector128 out;
   if(to_integer)out.low=rn+1;else out.low=raw(rn==31?0.0:double(rn+1),bits);
   execute(base,encoding(to_integer,bits,integer_bits,uns,8,rn,rd),to_integer,out,0,rn==rd);
  }
 }
 Inputs actual;actual.gpr[8]=10;execute(prepared(actual,0,16),0x1e18c148u,true,{0,0},0,true);
}
const std::uint64_t floating32[]{0,0x80000000,1,0x80000001,0x3e000000,0xbe000000,0x3eaaaaab,0xbeaaaaab,0x3f800000,0xbf800000,0x4f000000,0xcf000000,0x4f7fffff,0x4f800000,0x7f800000,0xff800000,0x7fc12345,0x7f812345,0x7f7fffff,0xff7fffff,0x00800000,0x007fffff};
const std::uint64_t floating64[]{0,UINT64_C(0x8000000000000000),1,UINT64_C(0x8000000000000001),UINT64_C(0x3fc0000000000000),UINT64_C(0xbfc0000000000000),UINT64_C(0x3fd5555555555555),UINT64_C(0xbfd5555555555555),UINT64_C(0x3ff0000000000000),UINT64_C(0xbff0000000000000),UINT64_C(0x43e0000000000000),UINT64_C(0xc3e0000000000000),UINT64_C(0x43efffffffffffff),UINT64_C(0x43f0000000000000),UINT64_C(0x7ff0000000000000),UINT64_C(0xfff0000000000000),UINT64_C(0x7ff8000000012345),UINT64_C(0x7ff0000000012345),UINT64_C(0x7fefffffffffffff),UINT64_C(0xffefffffffffffff),UINT64_C(0x0010000000000000),UINT64_C(0x000fffffffffffff)};
const std::uint64_t integers[]{0,1,2,3,UINT64_MAX,UINT64_MAX-1,UINT64_C(0x8000000000000000),UINT64_C(0x7fffffffffffffff),UINT64_C(0xffffffff),UINT64_C(0x80000000),UINT64_C(0x7fffffff),UINT64_C(0x100000001),UINT64_C(0x01000001),UINT64_C(0x0020000000000001),UINT64_C(0xaaaaaaaaaaaaaaaa),UINT64_C(0x5555555555555555)};
void numeric_examples(){
 for(bool to_integer:{false,true})for(unsigned bits:{32u,64u})for(unsigned integer_bits:{32u,64u})for(bool uns:{false,true})for(unsigned frac=1;frac<=integer_bits;++frac)for(unsigned mode=0;mode<16;++mode)for(unsigned sample=0;sample<4;++sample){
  const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0),rn=mode&1?31:10,rd=mode&2?31:8,index=frac+mode+sample*7;Inputs in;for(unsigned r=0;r<32;++r){in.gpr[r]=UINT64_C(0xa55a5aa5ffff0000)+r;in.vectors[r]={UINT64_MAX-r,UINT64_C(0xa5a5a5a5a5a5a5a5)};}
  in.vectors[rn].low=bits==32?floating32[index%22]:floating64[index%22];in.gpr[rn]=integers[index%16];
  auto expected=to_integer?to_reference(bits,in.vectors[rn].low,integer_bits,uns,frac,fpcr):from_reference(bits,rn==31?0:in.gpr[rn],integer_bits,uns,frac,fpcr);
  execute(prepared(in,fpcr),encoding(to_integer,bits,integer_bits,uns,frac,rn,rd),to_integer,{expected.first,0},expected.second,sample==0);
 }
}
void traps_and_rollback(){
 for(bool to_integer:{false,true})for(unsigned bits:{32u,64u})for(unsigned integer_bits:{32u,64u})for(bool uns:{false,true}){
  const auto op=encoding(to_integer,bits,integer_bits,uns,16,10,8);MinimalGuest g(128*1024*1024);g.load_elf(elf({op},true),true);g.run(1);check(g.machine().exceptions==1&&(g.machine().esr>>26)==7&&!g.floating().nonzero(),"Fixed GPR conversion bypassed original FP access trap");
  for(unsigned bad_type:{2u,3u}){const auto invalid=(op&~0x00c00000u)|(bad_type<<22);MinimalGuest bad(65536);bad.load_elf(elf({invalid}));const auto before=bad.snapshot();bool rejected=false;try{bad.run(1);}catch(const std::exception&){rejected=true;}check(rejected&&bad.snapshot()==before,"Reserved/FP16 fixed GPR conversion changed guest state");}
  if(integer_bits==32){const auto invalid=(op&~0xfc00u)|(31<<10);MinimalGuest bad(65536);bad.load_elf(elf({invalid}));const auto before=bad.snapshot();bool rejected=false;try{bad.run(1);}catch(const std::exception&){rejected=true;}check(rejected&&bad.snapshot()==before,"Invalid 32-bit fixed scale changed guest state");}
 }
}
#ifdef __aarch64__
struct NativeCode {
 void* page=nullptr;std::size_t bytes=0;
 NativeCode(std::uint32_t op,bool to_integer){
  bytes=std::size_t(::sysconf(_SC_PAGESIZE));page=::mmap(nullptr,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);check(page!=MAP_FAILED,"Cannot allocate actual AArch64 fixed conversion oracle");auto* code=static_cast<std::uint32_t*>(page);unsigned at=0;
  code[at++]=0xd10203ffu;for(unsigned r=8;r<16;r+=2)code[at++]=0xad000000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;
  code[at++]=0xaa0003efu;code[at++]=0xaa0103eeu;const unsigned rn=(op>>5)&31,rd=op&31,fpreg=to_integer?rn:rd,gpreg=to_integer?rd:rn;check(gpreg!=14&&gpreg!=15,"Oracle pointer register overlaps operand");
  code[at++]=0x3dc00000u|(fpreg<<10)|(15<<5)|fpreg;if(gpreg!=31)code[at++]=0xf9400000u|((64+gpreg)<<10)|(15<<5)|gpreg;code[at++]=op;code[at++]=(to_integer?0xf9000000u:0x3d800000u)|(14<<5)|rd;
  for(unsigned r=8;r<16;r+=2)code[at++]=0xad400000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;code[at++]=0x910203ffu;code[at++]=0xd65f03c0u;
  __builtin___clear_cache(static_cast<char*>(page),static_cast<char*>(page)+at*4);check(::mprotect(page,bytes,PROT_READ|PROT_EXEC)==0,"Cannot publish actual AArch64 fixed conversion oracle");
 }
 ~NativeCode(){if(page&&page!=MAP_FAILED)::munmap(page,bytes);}
 Vector128 run(const Inputs& values,unsigned fpcr,unsigned& flags){
  std::uint64_t oldcr,oldsr,sr;asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr":"=r"(oldcr),"=r"(oldsr));const std::uint64_t cr=fpcr,initial=10;asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(cr),"r"(initial):"memory");Vector128 out;reinterpret_cast<void(*)(const Inputs*,Vector128*)>(page)(&values,&out);asm volatile("mrs %0, fpsr":"=r"(sr));asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(oldcr),"r"(oldsr):"memory");flags=unsigned(sr);return out;
 }
};
void hardware_oracle(){
 std::uint64_t seed=0x1e18c148;auto random=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
 auto run=[&](bool to_integer,unsigned bits,unsigned integer_bits,bool uns,unsigned frac,unsigned rn,unsigned rd,unsigned samples){
  const auto op=encoding(to_integer,bits,integer_bits,uns,frac,rn,rd);NativeCode native(op,to_integer);
  for(unsigned mode=0;mode<16;++mode)for(unsigned sample=0;sample<samples;++sample){const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);Inputs in;
   for(unsigned r=0;r<32;++r){in.gpr[r]=sample<6?integers[(frac+sample*3+r)%16]:random();in.vectors[r]={sample<6?(bits==32?floating32[(frac+sample*3+r)%22]:floating64[(frac+sample*3+r)%22]):random(),random()};}
   unsigned flags=0;const auto expected=native.run(in,fpcr,flags);execute(prepared(in,fpcr),op,to_integer,expected,flags,false);++native_cases;
  }
 };
 for(bool to_integer:{false,true})for(unsigned bits:{32u,64u})for(unsigned integer_bits:{32u,64u})for(bool uns:{false,true}){
  for(unsigned frac=1;frac<=integer_bits;++frac){run(to_integer,bits,integer_bits,uns,frac,10,8,8);run(to_integer,bits,integer_bits,uns,frac,31,31,2);}
  for(unsigned frac:{1u,integer_bits})run(to_integer,bits,integer_bits,uns,frac,0,0,8);
 }
}
#else
void hardware_oracle(){}
#endif
}
int main(){try{allocated_forms();numeric_examples();traps_and_rollback();hardware_oracle();std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"native_hardware_cases\":"<<native_cases<<",\"exact_c54_opcode\":\"0x1e18c148\",\"android_boot\":false}\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
