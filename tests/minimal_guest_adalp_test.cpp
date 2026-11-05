// Signed/unsigned pairwise long accumulation plus original non-accumulating neighbors.
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



struct Form {unsigned bits,width;bool uns;};
const Form forms[]{{8,64,false},{8,128,false},{16,64,false},{16,128,false},{32,64,false},{32,128,false},{8,64,true},{8,128,true},{16,64,true},{16,128,true},{32,64,true},{32,128,true}};
std::uint32_t encoding(Form f,unsigned rn,unsigned rd,bool accumulate=true){return (accumulate?0x0e206800u:0x0e202800u)|(f.uns?0x20000000u:0)|(f.width==128?0x40000000u:0)|((f.bits==8?0u:f.bits==16?1u:f.bits==32?2u:3u)<<22)|(rn<<5)|rd;}
// Independent widened mathematical sum, reduced modulo the destination size.
Vector128 reference(const Inputs& in,Form f,unsigned rn,unsigned rd,bool accumulate=true){
 Vector128 result;for(unsigned e=0;e<f.width/(2*f.bits);++e){__int128 a=in.vectors[rn].lane(2*e,f.bits),b=in.vectors[rn].lane(2*e+1,f.bits);if(!f.uns){const auto sign=UINT64_C(1)<<(f.bits-1);if(a&sign)a-=(__int128(1)<<f.bits);if(b&sign)b-=(__int128(1)<<f.bits);}const __int128 acc=accumulate?in.vectors[rd].lane(e,2*f.bits):0;result.lane(e,2*f.bits,std::uint64_t(acc+a+b));}return result;
}
std::uint64_t seed=0x2e606801;
std::uint64_t random_value(){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
Inputs inputs(){Inputs in;for(unsigned r=0;r<32;++r){in.vectors[r]={random_value(),random_value()};in.gpr[r]=random_value();}return in;}
void execute(MinimalGuest g,std::uint32_t op,Vector128 expected,unsigned flags,bool restore){
 g.write(g.registers().pc,op,4);const auto cpu=g.registers();const auto fp=g.floating();const auto memory=g.runtime().memory_persistent_snapshot(),core=g.runtime().core_snapshot();
 if(restore){auto resumed=MinimalGuest::from_snapshot(g.snapshot());g.run(1);resumed.run(1);check(g.snapshot()==resumed.snapshot(),"ADALP changed after original checkpoint decoding");}else g.run(1);
 const unsigned rd=op&31;check(g.floating().fpsr==(fp.fpsr|flags)&&g.floating().fpcr==fp.fpcr,"ADALP FPCR/FPSR differs");
 check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"ADALP did not advance exactly one instruction");
 check(g.registers().sp==cpu.sp&&g.registers().nzcv==cpu.nzcv,"ADALP changed SP or NZCV");
 check(g.runtime().memory_persistent_snapshot()==memory&&g.runtime().core_snapshot()==core,"ADALP changed XI/Ether memory/core");
 for(unsigned r=0;r<31;++r)check(g.registers().x[r]==cpu.x[r],"ADALP changed a GPR");
 for(unsigned r=0;r<32;++r){const auto value=r==rd?expected:fp.v[r];check(g.floating().v[r].low==value.low&&g.floating().v[r].high==value.high,"ADALP destination/alias/upper SIMD bits differ");}++cases;
}

void allocated_forms(){
 for(auto f:forms){auto in=inputs();const auto base=prepared(in,0,0x0800009f);for(bool accumulate:{false,true})for(unsigned rn=0;rn<32;++rn)for(unsigned rd=0;rd<32;++rd)execute(base,encoding(f,rn,rd,accumulate),reference(in,f,rn,rd,accumulate),0,rn==rd);}
 Inputs actual;actual.vectors[0]={UINT64_C(0x8000b0007),0};actual.vectors[1]={UINT64_C(0x800000008),0};execute(prepared(actual,0,17),0x2e606801u,{UINT64_C(0x100000001a),0},0,true);
}
void exhaustive_and_edges(){
 for(auto f:forms){const unsigned elements=f.width/f.bits;const unsigned values=f.bits==8?256:f.bits==16?65536:8192;
  for(unsigned first=0;first<values;first+=elements){auto in=inputs();for(unsigned e=0;e<elements;++e){auto value=f.bits==32?random_value():first+e;in.vectors[10].lane(e,f.bits,value);}const unsigned mode=(first/elements)%16,fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);execute(prepared(in,fpcr,0x0800009f),encoding(f,10,8),reference(in,f,10,8),0,first==0);}
  for(unsigned pattern=0;pattern<32;++pattern){auto in=inputs();const auto sign=UINT64_C(1)<<(f.bits-1),mask=(UINT64_C(1)<<f.bits)-1;const std::uint64_t edge[]{0,1,mask,sign,sign-1,sign+1,mask-1,2};for(unsigned e=0;e<elements;++e)in.vectors[10].lane(e,f.bits,edge[(pattern+e)%8]);for(unsigned e=0;e<elements/2;++e)in.vectors[8].lane(e,2*f.bits,pattern&1?UINT64_MAX:pattern&2?UINT64_C(1)<<(2*f.bits-1):0);execute(prepared(in,0,0x0800009f),encoding(f,10,8),reference(in,f,10,8),0,true);}
 }
}
void traps_and_rollback(){
 for(auto f:forms){MinimalGuest g(128*1024*1024);g.load_elf(elf({encoding(f,10,8)},true),true);g.run(1);check(g.machine().exceptions==1&&(g.machine().esr>>26)==7&&!g.floating().nonzero(),"ADALP bypassed FP/SIMD access trap");}
 for(bool uns:{false,true})for(unsigned width:{64u,128u}){MinimalGuest g(65536);g.load_elf(elf({encoding({64,width,uns},10,8)}));const auto before=g.snapshot();bool rejected=false;try{g.run(1);}catch(const std::exception&){rejected=true;}check(rejected&&g.snapshot()==before,"Reserved ADALP size changed state");}
}
#ifdef __aarch64__
struct NativeCode {
 void* page=nullptr;std::size_t bytes=0;
 explicit NativeCode(std::uint32_t op){
  bytes=std::size_t(::sysconf(_SC_PAGESIZE));page=::mmap(nullptr,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);check(page!=MAP_FAILED,"Cannot allocate actual AArch64 ADALP oracle");auto* code=static_cast<std::uint32_t*>(page);unsigned at=0;
  code[at++]=0xd10203ffu;for(unsigned r=8;r<16;r+=2)code[at++]=0xad000000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;
  code[at++]=0xaa0003efu;code[at++]=0xaa0103eeu;const unsigned rn=(op>>5)&31,rm=(op>>16)&31,rd=op&31;
  for(unsigned r=0;r<32;++r)code[at++]=0x3dc00000u|(r<<10)|(15<<5)|r;
  code[at++]=op;code[at++]=0x3d800000u|(14<<5)|rd;
  for(unsigned r=8;r<16;r+=2)code[at++]=0xad400000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;code[at++]=0x910203ffu;code[at++]=0xd65f03c0u;
  __builtin___clear_cache(static_cast<char*>(page),static_cast<char*>(page)+at*4);check(::mprotect(page,bytes,PROT_READ|PROT_EXEC)==0,"Cannot publish actual AArch64 ADALP oracle");
 }
 ~NativeCode(){if(page&&page!=MAP_FAILED)::munmap(page,bytes);}
 Vector128 run(const Inputs& values,unsigned fpcr,unsigned& flags){
  std::uint64_t oldcr,oldsr,sr;asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr":"=r"(oldcr),"=r"(oldsr));const std::uint64_t cr=fpcr,initial=2;asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(cr),"r"(initial):"memory");Vector128 out;reinterpret_cast<void(*)(const Inputs*,Vector128*)>(page)(&values,&out);asm volatile("mrs %0, fpsr":"=r"(sr));asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(oldcr),"r"(oldsr):"memory");flags=unsigned(sr);return out;
 }
};
void hardware_oracle(){
 for(auto f:forms){auto in=inputs();const auto base=prepared(in);for(unsigned rn=0;rn<32;++rn)for(unsigned rd=0;rd<32;++rd){const auto op=encoding(f,rn,rd);NativeCode native(op);unsigned flags;auto output=native.run(in,0,flags);check(flags==2,"Actual ADALP changed FPSR");execute(base,op,output,0,false);++native_cases;}
 for(unsigned sample=0;sample<2048;++sample){auto in=inputs();const unsigned rn=(sample*3+10)&31,rd=(sample*5+8)&31,mode=sample%16,fpcr=((mode&3)<<22)|((mode&4)?1u<<24:0)|((mode&8)?1u<<25:0);const auto op=encoding(f,rn,rd);NativeCode native(op);unsigned flags;auto output=native.run(in,fpcr,flags);check(flags==2,"Actual ADALP changed FPSR");execute(prepared(in,fpcr),op,output,0,sample==0);++native_cases;}}
}
#else
void hardware_oracle(){}
#endif
}
int main(){try{allocated_forms();exhaustive_and_edges();traps_and_rollback();hardware_oracle();std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"native_hardware_cases\":"<<native_cases<<",\"exact_android_opcode\":\"0x2e606801\",\"android_boot\":false}\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
