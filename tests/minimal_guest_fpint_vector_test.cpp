// Exact SurfaceFlinger FCVTZU stop and allocated vector FP-to-integer family.
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
std::uint32_t encoding(unsigned bits,bool wide,bool uns,unsigned rn,unsigned rd){
 return 0x0ea1b800u|(bits==64?0x00400000u:0)|(wide?0x40000000u:0)|(uns?0x20000000u:0)|(rn<<5)|rd;
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
 for(unsigned bits:{32u,64u})for(bool wide:{false,true}){
  if(bits==64&&!wide)continue;
  Vectors v;for(unsigned r=0;r<32;++r)for(unsigned n=0;n<128/bits;++n)v[r].lane(n,bits,raw(double(1+r+7*n),bits));
  const auto base=prepared(v);
  for(bool uns:{false,true})for(unsigned rn=0;rn<32;++rn)for(unsigned rd=0;rd<32;++rd){
   Vector128 out;for(unsigned n=0;n<(wide?128u:64u)/bits;++n)out.lane(n,bits,1+rn+7*n);
   execute(base,encoding(bits,wide,uns,rn,rd),out,0,rn==rd);
  }
 }
 Vectors v;v[0]={UINT64_C(0x41a283913f000000),UINT64_C(0x426fc55a42208391)};
 const Vector128 out{UINT64_C(0x0000001400000000),UINT64_C(0x0000003b00000028)};
 execute(prepared(v,0,16),0x6ea1b800u,out,16,true); // Actual SurfaceFlinger values: 0.5,20.314...,40.128...,59.942...
 execute(prepared(v,0,0x0800009f),0x6ea1b800u,out,16,true); // Preserve all preexisting cumulative flags, including QC.
}
void numeric_examples(){
 struct Example{unsigned bits;std::uint64_t raw,signed_result,unsigned_result;unsigned signed_flags,unsigned_flags;};
 const Example examples[]{
  {32,0x40200000,2,2,16,16},{64,UINT64_C(0x4004000000000000),2,2,16,16},
  {32,0x4f800000,0x7fffffff,0xffffffff,1,1},
  {32,0x4f7fffff,0x7fffffff,0xffffff00,1,0},
  {64,UINT64_C(0x43f0000000000000),UINT64_C(0x7fffffffffffffff),UINT64_MAX,1,1},
  {64,UINT64_C(0x43efffffffffffff),UINT64_C(0x7fffffffffffffff),UINT64_C(0xfffffffffffff800),1,0},
  {32,0,0,0,0,0},{64,UINT64_C(0x8000000000000000),0,0,0,0},
  {32,0xc0200000,0xfffffffe,0,16,1},{64,UINT64_C(0xc004000000000000),UINT64_MAX-1,0,16,1},
  {32,0xbf000000,0,0,16,16},{64,UINT64_C(0xbfe0000000000000),0,0,16,16},
  {32,0x7fc12345,0,0,1,1},{64,UINT64_C(0x7ff0000000000001),0,0,1,1},
  {32,0x7f800000,0x7fffffff,0xffffffff,1,1},
  {64,UINT64_C(0x7ff0000000000000),UINT64_C(0x7fffffffffffffff),UINT64_MAX,1,1},
  {32,0xff800000,0x80000000,0,1,1},
  {64,UINT64_C(0xfff0000000000000),UINT64_C(0x8000000000000000),0,1,1},
  {32,0xcf000000,0x80000000,0,0,1},
  {64,UINT64_C(0xc3e0000000000000),UINT64_C(0x8000000000000000),0,0,1},
  {32,0x4f000000,0x7fffffff,0x80000000,1,0},
  {64,UINT64_C(0x43e0000000000000),UINT64_C(0x7fffffffffffffff),UINT64_C(0x8000000000000000),1,0},
  {32,1,0,0,16,16},{64,1,0,0,16,16}
 };
 for(const auto& e:examples)for(unsigned mode=0;mode<16;++mode)for(bool wide:{false,true}){
  if(e.bits==64&&!wide)continue;
  const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);
  const auto exponent=e.bits==32?UINT64_C(0x7f800000):UINT64_C(0x7ff0000000000000),fraction=e.bits==32?UINT64_C(0x7fffff):UINT64_C(0xfffffffffffff);
  for(bool uns:{false,true}){
   const auto result=uns?e.unsigned_result:e.signed_result;
   unsigned flags=uns?e.unsigned_flags:e.signed_flags;
   if((fpcr&(1u<<24))&&!(e.raw&exponent)&&(e.raw&fraction))flags=128;
   Vectors v;for(unsigned n=0;n<128/e.bits;++n)v[31].lane(n,e.bits,e.raw);
   Vector128 out;for(unsigned n=0;n<(wide?128u:64u)/e.bits;++n)out.lane(n,e.bits,result);
   for(unsigned rd:{0u,31u})execute(prepared(v,fpcr),encoding(e.bits,wide,uns,31,rd),out,flags,true);
  }
 }
 Vectors v;v[1].lane(1,32,0x3f800000);v[1].high=UINT64_C(0x7f8123457f800000);Vector128 out;out.lane(1,32,1);
 execute(prepared(v),encoding(32,false,true,1,1),out,0,true); // Unused upper lanes do not raise NaN/Infinity exceptions.
 v[1]={UINT64_C(0x7f80000040200000),UINT64_C(0x7f81234500000001)};out={UINT64_C(0xffffffff00000002),0};
 execute(prepared(v),encoding(32,true,true,1,1),out,17,true);
 execute(prepared(v,1u<<24),encoding(32,true,true,1,1),out,145,true);
}
void traps_and_rollback(){
 for(auto op:{0x0ea1b820u,0x2ea1b820u,0x4ea1b820u,0x6ea1b820u,0x4ee1b820u,0x6ee1b820u}){
  MinimalGuest g(128*1024*1024);g.load_elf(elf({op},true),true);g.run(1);
  check(g.machine().exceptions==1&&(g.machine().esr>>26)==7&&!g.floating().nonzero(),"Vector FP conversion bypassed FP access trap");
 }
 for(auto op:{0x0ee1b820u,0x2ee1b820u,0x0ef9b820u,0x4ef9b820u}){
  MinimalGuest g(65536);g.load_elf(elf({op}));const auto before=g.snapshot();bool rejected=false;
  try{g.run(1);}catch(const std::exception&){rejected=true;}
  check(rejected&&g.snapshot()==before,"Reserved or unsupported FP conversion changed guest state");
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
 std::uint64_t seed=0x6ea1b800;auto random=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
 const std::uint64_t s32[]{0,0x80000000,1,0x80000001,0x40200000,0xc0200000,0x7f800000,0xff800000,0x7fc12345,0x7f812345,0x4f800000,0xcf000000};
 const std::uint64_t s64[]{0,UINT64_C(0x8000000000000000),1,UINT64_C(0x8000000000000001),UINT64_C(0x4004000000000000),UINT64_C(0xc004000000000000),UINT64_C(0x7ff0000000000000),UINT64_C(0xfff0000000000000),UINT64_C(0x7ff8000000012345),UINT64_C(0x7ff0000000012345),UINT64_C(0x43f0000000000000),UINT64_C(0xc3e0000000000000)};
 for(unsigned bits:{32u,64u})for(bool wide:{false,true}){
  if(bits==64&&!wide)continue;
  for(bool uns:{false,true})for(unsigned rn:{0u,16u,31u})for(unsigned rd:{0u,2u}){
   const auto op=encoding(bits,wide,uns,rn,rd);NativeCode native(op);
   for(unsigned mode=0;mode<16;++mode)for(unsigned sample=0;sample<20;++sample){
    const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);Vectors v;
    for(unsigned r:{rn,rd})for(unsigned n=0;n<128/bits;++n)v[r].lane(n,bits,sample<12?(bits==32?s32[(sample+n)%12]:s64[(sample+n)%12]):random());
    unsigned flags=0;const auto expected=native.run(v,fpcr,flags);execute(prepared(v,fpcr),op,expected,flags,false);++native_cases;
   }
  }
 }
}
#else
void hardware_oracle(){}
#endif
}
int main(){try{allocated_forms();numeric_examples();traps_and_rollback();hardware_oracle();std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"native_hardware_cases\":"<<native_cases<<",\"exact_c49_opcode\":\"0x6ea1b800\",\"android_boot\":false}\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
