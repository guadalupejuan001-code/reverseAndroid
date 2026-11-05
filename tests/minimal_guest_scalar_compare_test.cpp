// Actual CMHI D2,D0,D1 stop and the allocated scalar integer compare family.
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

const std::uint32_t bases[]{0x5ee03400u,0x7ee03400u,0x5ee03c00u,0x7ee03c00u,0x5ee08c00u,0x7ee08c00u,0x5ee08800u,0x7ee08800u,0x5ee09800u,0x7ee09800u,0x5ee0a800u};
std::uint32_t encoding(unsigned operation,unsigned rn,unsigned rm,unsigned rd){return bases[operation]|(operation<6?rm<<16:0)|(rn<<5)|rd;}
std::int64_t signed_bits(std::uint64_t x){std::int64_t result;std::memcpy(&result,&x,8);return result;}
Vector128 reference(unsigned operation,std::uint64_t x,std::uint64_t y){
 bool result=false;
 switch(operation){case 0:result=signed_bits(x)>signed_bits(y);break;case 1:result=x>y;break;case 2:result=signed_bits(x)>=signed_bits(y);break;case 3:result=x>=y;break;case 4:result=(x&y)!=0;break;case 5:result=x==y;break;case 6:result=signed_bits(x)>0;break;case 7:result=signed_bits(x)>=0;break;case 8:result=x==0;break;case 9:result=signed_bits(x)<=0;break;case 10:result=signed_bits(x)<0;break;default:throw std::runtime_error("Bad compare oracle operation");}
 return {result?UINT64_MAX:0,0};
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
 Vectors v;for(unsigned r=0;r<32;++r)v[r]={UINT64_C(0x80000000ffffffff)*r,UINT64_MAX-r};const auto base=prepared(v);
 for(unsigned op=0;op<6;++op)for(unsigned rn=0;rn<32;++rn)for(unsigned rm=0;rm<32;++rm)for(unsigned rd:{rn,rm,(rn+rm)&31})
  execute(base,encoding(op,rn,rm,rd),reference(op,v[rn].low,v[rm].low),0,true);
 for(unsigned op=6;op<11;++op)for(unsigned rn=0;rn<32;++rn)for(unsigned rd=0;rd<32;++rd)
  execute(base,encoding(op,rn,0,rd),reference(op,v[rn].low,0),0,true);
 v={};v[0]=v[1]={0x100,0x100};execute(prepared(v,0,16),0x7ee13402u,{0,0},0,true);
 v[2]={UINT64_MAX,UINT64_MAX};execute(prepared(v,0,0x0800009f),0x7ee13402u,{0,0},0,true);
}
const std::uint64_t values[]{0,1,2,255,256,257,UINT64_C(0x7fffffffffffffff),UINT64_C(0x8000000000000000),UINT64_C(0x8000000000000001),UINT64_MAX-1,UINT64_MAX,UINT64_C(0xaaaaaaaaaaaaaaaa),UINT64_C(0x5555555555555555),UINT64_C(0x7ff0000000000001),UINT64_C(0x7ff8000000000000)};
void numeric_examples(){
 for(unsigned op=0;op<6;++op)for(unsigned mode=0;mode<16;++mode)for(auto x:values)for(auto y:values){
  const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);Vectors v;v[0]={x,UINT64_MAX-y};v[31]={y,UINT64_MAX-x};
  execute(prepared(v,fpcr),encoding(op,0,31,16),reference(op,x,y),0,false);
 }
 for(unsigned op=6;op<11;++op)for(unsigned mode=0;mode<16;++mode)for(auto x:values){
  const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);Vectors v;v[31]={x,UINT64_MAX};
  execute(prepared(v,fpcr),encoding(op,31,0,31),reference(op,x,0),0,true);
 }
}
void traps_and_rollback(){
 for(unsigned op=0;op<11;++op){
  MinimalGuest g(128*1024*1024);g.load_elf(elf({encoding(op,1,2,0)},true),true);g.run(1);
  check(g.machine().exceptions==1&&(g.machine().esr>>26)==7&&!g.floating().nonzero(),"Scalar integer comparison bypassed original FP access trap");
  for(unsigned size=0;size<3;++size){
   MinimalGuest bad(65536);bad.load_elf(elf({(encoding(op,1,2,0)&~0x00c00000u)|(size<<22)}));const auto before=bad.snapshot();bool rejected=false;
   try{bad.run(1);}catch(const std::exception&){rejected=true;}
   check(rejected&&bad.snapshot()==before,"Unallocated scalar comparison changed guest state");
  }
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
 std::uint64_t seed=0x7ee13402;auto random=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
 for(unsigned op=0;op<11;++op)for(unsigned rn:{0u,16u,31u})for(unsigned rm:{0u,16u,31u})for(unsigned rd:{0u,2u}){
  if(op>=6&&rm!=0)continue;NativeCode native(encoding(op,rn,rm,rd));
  for(unsigned mode=0;mode<16;++mode)for(unsigned sample=0;sample<64;++sample){
   const unsigned fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);Vectors v;
   for(unsigned r:{rn,rm,rd})v[r]={sample<15?values[(sample+r)%15]:random(),random()};
   unsigned flags=0;const auto expected=native.run(v,fpcr,flags);execute(prepared(v,fpcr),encoding(op,rn,rm,rd),expected,flags,false);++native_cases;
  }
 }
}
#else
void hardware_oracle(){}
#endif
}
int main(){try{allocated_forms();numeric_examples();traps_and_rollback();hardware_oracle();std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"native_hardware_cases\":"<<native_cases<<",\"exact_c52_opcode\":\"0x7ee13402\",\"android_boot\":false}\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
