// Exact C42 stop and the allocated FMLA/FMLS S/D indexed-element family.
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
std::uint32_t encoding(unsigned bits,bool scalar,bool wide,bool subtract,unsigned index,unsigned rn,unsigned rm,unsigned rd){
 return (scalar?0x5f801000u:0x0f801000u|(wide?0x40000000u:0))|
  (bits==64?0x00400000u:0)|(subtract?0x4000u:0)|
  ((bits==64?index:index>>1)<<11)|(bits==32?(index&1u)<<21:0)|(rm<<16)|(rn<<5)|rd;
}
void execute(MinimalGuest g,std::uint32_t op,Vector128 expected,unsigned flags,bool restore){
 g.write(g.registers().pc,op,4);const auto cpu=g.registers();const auto fp=g.floating();
 const auto memory=g.runtime().memory_persistent_snapshot(),core=g.runtime().core_snapshot();
 if(restore){auto resumed=MinimalGuest::from_snapshot(g.snapshot());g.run(1);resumed.run(1);check(g.snapshot()==resumed.snapshot(),"Indexed FMA differs after original checkpoint restore");}
 else g.run(1);
 const auto rd=op&31;
 check(g.floating().v[rd].low==expected.low&&g.floating().v[rd].high==expected.high,"Indexed FMA result, selected lane or upper clearing differs");
 check(g.floating().fpsr==(fp.fpsr|flags)&&g.floating().fpcr==fp.fpcr,"Indexed FMA FPCR/FPSR differs");
 check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"Indexed FMA did not advance exactly one instruction");
 check(g.registers().x==cpu.x&&g.registers().sp==cpu.sp&&g.registers().nzcv==cpu.nzcv,"Indexed FMA changed integer CPU state");
 check(g.runtime().memory_persistent_snapshot()==memory&&g.runtime().core_snapshot()==core,"Indexed FMA changed XI/Ether memory or core");
 for(unsigned r=0;r<32;r++)if(r!=rd)check(g.floating().v[r].low==fp.v[r].low&&g.floating().v[r].high==fp.v[r].high,"Indexed FMA changed another vector register");
 ++cases;
}
void allocated_forms(){
 for(unsigned bits:{32u,64u}){
  Vectors values;for(unsigned r=0;r<32;r++)for(unsigned n=0;n<128/bits;n++)values[r].lane(n,bits,raw(1+r+7*n,bits));
  const auto base=prepared(values);
  for(bool scalar:{false,true})for(bool wide:{false,true}){
   if((scalar&&!wide)||(!scalar&&!wide&&bits==64))continue;
   for(bool subtract:{false,true})for(unsigned index=0;index<128/bits;index++)
    for(unsigned rn=0;rn<32;rn++)for(unsigned rd:{0u,1u,2u,15u,16u,30u,31u}){
     const unsigned rm=(rd+rn+index)&31;Vector128 expected;
     for(unsigned n=0;n<(scalar?1u:(wide?128u:64u)/bits);n++){
      const auto product=(1+rn+7*n)*(1+rm+7*index);
      expected.lane(n,bits,raw(double(1+rd+7*n)+(subtract?-double(product):double(product)),bits));
     }
     execute(base,encoding(bits,scalar,wide,subtract,index,rn,rm,rd),expected,0,rd==rn||rd==rm);
    }
  }
 }
 // The exact stopped opcode: FMLA V2.2D,V3.2D,V0.D[0].
 Vectors v;v[0]={raw(2,64),raw(99,64)};v[3]={raw(3,64),raw(4,64)};v[2]={raw(5,64),raw(6,64)};
 check(encoding(64,false,true,false,0,3,0,2)==0x4fc01062u,"Observed C42 encoding changed");
 execute(prepared(v),0x4fc01062u,{raw(11,64),raw(14,64)},0,true);
}
void numeric_examples(){
 // Exact bit expectations include a product which must not round before addition.
 struct Example{unsigned bits;std::uint64_t a,b,c,result;unsigned flags;};
 const Example examples[]{
  {32,0x3f800001,0x3f7ffffe,0xbf800000,0xa8800000,0},
  {64,UINT64_C(0x3ff0000000000001),UINT64_C(0x3feffffffffffffe),UINT64_C(0xbff0000000000000),UINT64_C(0xb970000000000000),0},
  {32,0x7f800000,0,0,0x7fc00000,1},
  {64,UINT64_C(0x7ff0000000000000),0,0,UINT64_C(0x7ff8000000000000),1},
  {32,0x3f800000,0x3f800000,0x7fc12345,0x7fc12345,0},
  {64,UINT64_C(0x3ff0000000000000),UINT64_C(0x3ff0000000000000),UINT64_C(0x7ff0000000000042),UINT64_C(0x7ff8000000000042),1}
 };
 for(const auto&e:examples)for(bool scalar:{false,true}){
  Vectors v;for(unsigned n=0;n<128/e.bits;n++){v[3].lane(n,e.bits,e.a);v[0].lane(n,e.bits,e.b);v[2].lane(n,e.bits,e.c);}
  Vector128 out;for(unsigned n=0;n<(scalar?1u:128/e.bits);n++)out.lane(n,e.bits,e.result);
  execute(prepared(v),encoding(e.bits,scalar,true,false,0,3,0,2),out,e.flags,true);
 }
}
void traps_and_rollback(){
 for(auto op:{0x4fc01062u,0x4fc05062u,0x5fc01062u,0x0f801062u}){
  MinimalGuest g(128*1024*1024);g.load_elf(elf({op},true),true);g.run(1);
  check(g.machine().exceptions==1&&(g.machine().esr>>26)==7&&!g.floating().nonzero(),"Indexed FMA bypassed FP access trap");
 }
 for(auto op:{0x0fc01062u,0x4fe01062u,0x5fe01062u,0x4f001062u}){
  MinimalGuest g(65536);g.load_elf(elf({op}));const auto before=g.snapshot();bool rejected=false;
  try{g.run(1);}catch(const std::exception&){rejected=true;}
  check(rejected&&g.snapshot()==before,"Reserved/unsupported indexed FMA modified state");
 }
}
#ifdef __aarch64__
struct NativeCode {
 void* page=nullptr;std::size_t bytes=0;
 explicit NativeCode(std::uint32_t op){
  bytes=std::size_t(::sysconf(_SC_PAGESIZE));page=::mmap(nullptr,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  check(page!=MAP_FAILED,"Cannot allocate native instruction oracle");
  auto* code=static_cast<std::uint32_t*>(page);const unsigned rn=(op>>5)&31,rm=(op>>16)&31,rd=op&31;
  // Only caller-saved V0/V2/V3/V16/V31 are used by the hardware cases.
  unsigned i=0;for(const unsigned r:{rn,rm,rd})code[i++]=0x3dc00000u|(r<<10)|r;
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
 std::uint64_t seed=0x4fc01062;auto random=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
 for(unsigned bits:{32u,64u})for(bool scalar:{false,true})for(bool wide:{false,true}){
  if((scalar&&!wide)||(!scalar&&!wide&&bits==64))continue;
  for(bool subtract:{false,true})for(unsigned index=0;index<128/bits;index++)for(unsigned rm:{0u,16u,31u}){
   const auto op=encoding(bits,scalar,wide,subtract,index,3,rm,2);NativeCode native(op);
   for(unsigned mode=0;mode<16;mode++)for(unsigned sample=0;sample<10;sample++){
    const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);Vectors v;
    const std::uint64_t special32[]{0,0x80000000,1,0x00800000,0x7f800000,0xff800000,0x7fc12345,0x7f812345};
    const std::uint64_t special64[]{0,UINT64_C(0x8000000000000000),1,UINT64_C(0x0010000000000000),UINT64_C(0x7ff0000000000000),UINT64_C(0xfff0000000000000),UINT64_C(0x7ff8000000012345),UINT64_C(0x7ff0000000012345)};
    for(unsigned r:{2u,3u,rm})for(unsigned n=0;n<128/bits;n++)v[r].lane(n,bits,sample<8?(bits==32?special32[(sample+r+n)%8]:special64[(sample+r+n)%8]):random());
    unsigned flags=0;const auto expected=native.run(v,fpcr,flags);execute(prepared(v,fpcr),op,expected,flags,false);++native_cases;
   }
  }
 }
}
#else
void hardware_oracle(){}
#endif
}
int main(){try{allocated_forms();numeric_examples();traps_and_rollback();hardware_oracle();std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"native_hardware_cases\":"<<native_cases<<",\"exact_c42_opcode\":\"0x4fc01062\",\"android_boot\":false}\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
