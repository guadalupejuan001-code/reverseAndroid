// Exact SurfaceFlinger UCVTF stop and allocated vector integer-to-FP family.
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
MinimalGuest prepared(const Vectors& values,unsigned fpcr=0){
 std::vector<std::uint32_t> code{0xd2840002u};
 for(unsigned r=0;r<32;r++)code.push_back(0x3dc00040u|(r<<10)|r);
 constant(code,3,fpcr);code.push_back(0xd51b4403u);
 constant(code,3,10);code.push_back(0xd51b4423u);
 code.push_back(0xf100045fu);code.push_back(0xd503201fu);
 MinimalGuest g(65536);g.load_elf(elf(code));
 for(unsigned r=0;r<32;r++){g.write(0x2000+r*16,values[r].low,8);g.write(0x2008+r*16,values[r].high,8);}
 g.run(code.size()-1);return g;
}
std::uint32_t encoding(unsigned bits,bool wide,bool uns,unsigned rn,unsigned rd){
 return 0x0e21d800u|(bits==64?0x00400000u:0)|(wide?0x40000000u:0)|(uns?0x20000000u:0)|(rn<<5)|rd;
}
void execute(MinimalGuest g,std::uint32_t op,Vector128 expected,unsigned flags,bool restore){
 g.write(g.registers().pc,op,4);const auto cpu=g.registers();const auto fp=g.floating();
 const auto memory=g.runtime().memory_persistent_snapshot(),core=g.runtime().core_snapshot();
 if(restore){auto resumed=MinimalGuest::from_snapshot(g.snapshot());g.run(1);resumed.run(1);check(g.snapshot()==resumed.snapshot(),"Vector integer conversion differs after original checkpoint restore");}
 else g.run(1);
 const auto rd=op&31;
 check(g.floating().v[rd].low==expected.low&&g.floating().v[rd].high==expected.high,"Vector integer conversion result, selected lane or upper clearing differs");
 check(g.floating().fpsr==(fp.fpsr|flags)&&g.floating().fpcr==fp.fpcr,"Vector integer conversion FPCR/FPSR differs");
 check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"Vector integer conversion did not advance exactly one instruction");
 check(g.registers().x==cpu.x&&g.registers().sp==cpu.sp&&g.registers().nzcv==cpu.nzcv,"Vector integer conversion changed integer CPU state");
 check(g.runtime().memory_persistent_snapshot()==memory&&g.runtime().core_snapshot()==core,"Vector integer conversion changed XI/Ether memory or core");
 for(unsigned r=0;r<32;r++)if(r!=rd)check(g.floating().v[r].low==fp.v[r].low&&g.floating().v[r].high==fp.v[r].high,"Vector integer conversion changed another vector register");
 ++cases;
}
void allocated_forms(){
 for(unsigned bits:{32u,64u})for(bool wide:{false,true}){
  if(bits==64&&!wide)continue;
  Vectors values;for(unsigned r=0;r<32;++r)for(unsigned n=0;n<128/bits;++n)values[r].lane(n,bits,1+r+7*n);
  const auto base=prepared(values);
  for(bool uns:{false,true})for(unsigned rn=0;rn<32;++rn)for(unsigned rd=0;rd<32;++rd){
   Vector128 expected;for(unsigned n=0;n<(wide?128u:64u)/bits;++n)expected.lane(n,bits,raw(double(1+rn+7*n),bits));
   execute(base,encoding(bits,wide,uns,rn,rd),expected,0,rn==rd);
  }
 }
 // Exact observed SurfaceFlinger source: UCVTF V0.4S,V1.4S, integers 0,1,2,3.
 Vectors v;for(unsigned n=0;n<4;++n)v[1].lane(n,32,n);
 Vector128 out;for(unsigned n=0;n<4;++n)out.lane(n,32,raw(double(n),32));
 execute(prepared(v),0x6e21d820u,out,0,true);
}
void numeric_examples(){
 struct Example{unsigned bits;bool uns;std::uint64_t value;std::array<std::uint64_t,4> expected;unsigned flags;};
 const Example examples[]{
  {32,false,0xffffffff,{0xbf800000,0xbf800000,0xbf800000,0xbf800000},0},
  {64,false,UINT64_MAX,{UINT64_C(0xbff0000000000000),UINT64_C(0xbff0000000000000),UINT64_C(0xbff0000000000000),UINT64_C(0xbff0000000000000)},0},
  {32,true,0xffffffff,{0x4f800000,0x4f800000,0x4f7fffff,0x4f7fffff},16},
  {64,true,UINT64_MAX,{UINT64_C(0x43f0000000000000),UINT64_C(0x43f0000000000000),UINT64_C(0x43efffffffffffff),UINT64_C(0x43efffffffffffff)},16},
  {32,false,0x7fffffff,{0x4f000000,0x4f000000,0x4effffff,0x4effffff},16},
  {64,false,UINT64_C(0x7fffffffffffffff),{UINT64_C(0x43e0000000000000),UINT64_C(0x43e0000000000000),UINT64_C(0x43dfffffffffffff),UINT64_C(0x43dfffffffffffff)},16},
  {32,false,0x80000000,{0xcf000000,0xcf000000,0xcf000000,0xcf000000},0},
  {64,false,UINT64_C(0x8000000000000000),{UINT64_C(0xc3e0000000000000),UINT64_C(0xc3e0000000000000),UINT64_C(0xc3e0000000000000),UINT64_C(0xc3e0000000000000)},0},
  {32,false,0x01000001,{0x4b800000,0x4b800001,0x4b800000,0x4b800000},16},
  {32,false,0xfeffffff,{0xcb800000,0xcb800000,0xcb800001,0xcb800000},16},
  {64,false,UINT64_C(0x0020000000000001),{UINT64_C(0x4340000000000000),UINT64_C(0x4340000000000001),UINT64_C(0x4340000000000000),UINT64_C(0x4340000000000000)},16}
 };
 for(const auto& e:examples)for(unsigned mode=0;mode<16;++mode)for(bool wide:{false,true}){
  if(e.bits==64&&!wide)continue;
  const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);
  Vectors v;for(unsigned n=0;n<128/e.bits;++n)v[31].lane(n,e.bits,e.value);
  Vector128 out;for(unsigned n=0;n<(wide?128u:64u)/e.bits;++n)out.lane(n,e.bits,e.expected[mode&3]);
  for(unsigned rd:{0u,31u})execute(prepared(v,fpcr),encoding(e.bits,wide,e.uns,31,rd),out,e.flags,true);
 }
 Vectors v;v[1].lane(1,32,1);v[1].high=UINT64_MAX;Vector128 out;out.lane(1,32,0x3f800000);
 execute(prepared(v),encoding(32,false,true,1,1),out,0,true); // Unused upper lanes must not raise IXC.
}
void traps_and_rollback(){
 for(auto op:{0x0e21d820u,0x2e21d820u,0x4e21d820u,0x6e21d820u,0x4e61d820u,0x6e61d820u}){
  MinimalGuest g(128*1024*1024);g.load_elf(elf({op},true),true);g.run(1);
  check(g.machine().exceptions==1&&(g.machine().esr>>26)==7&&!g.floating().nonzero(),"Vector conversion bypassed FP access trap");
 }
 for(auto op:{0x0e61d820u,0x2e61d820u,0x0e79d820u,0x4e79d820u}){
  MinimalGuest g(65536);g.load_elf(elf({op}));const auto before=g.snapshot();bool rejected=false;
  try{g.run(1);}catch(const std::exception&){rejected=true;}
  check(rejected&&g.snapshot()==before,"Reserved or unsupported conversion modified guest state");
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
 std::uint64_t seed=0x6e21d820;auto random=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
 for(unsigned bits:{32u,64u})for(bool wide:{false,true}){
  if(bits==64&&!wide)continue;
  for(bool uns:{false,true})for(unsigned rn:{0u,16u,31u})for(unsigned rd:{0u,2u}){
   const auto op=encoding(bits,wide,uns,rn,rd);NativeCode native(op);
   for(unsigned mode=0;mode<16;++mode)for(unsigned sample=0;sample<20;++sample){
    const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);Vectors v;
    for(unsigned r:{rn,rd})for(unsigned n=0;n<128/bits;++n)v[r].lane(n,bits,sample<4?(sample==0?0:sample==1?UINT64_MAX:sample==2?UINT64_C(0x8000000000000000):UINT64_C(0x0020000000000001)):random());
    unsigned flags=0;const auto expected=native.run(v,fpcr,flags);execute(prepared(v,fpcr),op,expected,flags,false);++native_cases;
   }
  }
 }
}
#else
void hardware_oracle(){}
#endif
}
int main(){try{allocated_forms();numeric_examples();traps_and_rollback();hardware_oracle();std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"native_hardware_cases\":"<<native_cases<<",\"exact_c47_opcode\":\"0x6e21d820\",\"android_boot\":false}\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
