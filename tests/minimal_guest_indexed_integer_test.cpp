// Integer by-element multiply/accumulate/subtract, with an independent mathematical oracle.
#include "minimal_guest.hpp"
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
using Vectors=std::array<Vector128,32>;
struct Inputs {Vectors vectors{};std::array<std::uint64_t,32> gpr{};};
static_assert(offsetof(Inputs,gpr)==512,"Native oracle input layout changed");
MinimalGuest prepared(const Inputs& values,unsigned fpcr=0,unsigned fpsr=2){
 std::vector<std::uint32_t> code{0xd2840002u};for(unsigned r=0;r<32;++r)code.push_back(0x3dc00040u|(r<<10)|r);
 constant(code,3,fpcr);code.push_back(0xd51b4403u);constant(code,3,fpsr);code.push_back(0xd51b4423u);code.push_back(0xf100045fu);
 for(unsigned r=0;r<31;++r)constant(code,r,values.gpr[r]);code.push_back(0xd503201fu);
 MinimalGuest g(65536);g.load_elf(elf(code));for(unsigned r=0;r<32;++r){g.write(0x2000+r*16,values.vectors[r].low,8);g.write(0x2008+r*16,values.vectors[r].high,8);}g.run(code.size()-1);return g;
}




enum Operation {Multiply,Add,Subtract};
struct Form {unsigned bits,width;Operation operation;bool widening=false,unsigned_form=false;};
const Form forms[]{{16,64,Multiply},{16,128,Multiply},{32,64,Multiply},{32,128,Multiply},{16,64,Add},{16,128,Add},{32,64,Add},{32,128,Add},{16,64,Subtract},{16,128,Subtract},{32,64,Subtract},{32,128,Subtract},{16,64,Multiply,true,false},{16,128,Multiply,true,false},{32,64,Multiply,true,false},{32,128,Multiply,true,false},{16,64,Multiply,true,true},{16,128,Multiply,true,true},{32,64,Multiply,true,true},{32,128,Multiply,true,true},{16,64,Add,true,false},{16,128,Add,true,false},{32,64,Add,true,false},{32,128,Add,true,false},{16,64,Add,true,true},{16,128,Add,true,true},{32,64,Add,true,true},{32,128,Add,true,true},{16,64,Subtract,true,false},{16,128,Subtract,true,false},{32,64,Subtract,true,false},{32,128,Subtract,true,false},{16,64,Subtract,true,true},{16,128,Subtract,true,true},{32,64,Subtract,true,true},{32,128,Subtract,true,true}};
unsigned destination_bits(Form f){return f.widening?2*f.bits:f.bits;}
unsigned elements(Form f){return f.widening?64/f.bits:f.width/f.bits;}
std::uint64_t ones(unsigned bits){return bits==64?UINT64_MAX:(UINT64_C(1)<<bits)-1;}
unsigned indices(Form f){return 128/f.bits;}
unsigned registers(Form f){return f.bits==16?16:32;}
std::uint32_t encoding(Form f,unsigned rn,unsigned rm,unsigned rd,unsigned index){
 const auto base=f.widening?((f.operation==Multiply?0x0f00a000u:f.operation==Add?0x0f002000u:0x0f006000u)|(f.unsigned_form?0x20000000u:0)):(f.operation==Multiply?0x0f008000u:f.operation==Add?0x2f000000u:0x2f004000u);
 const unsigned h=f.bits==16?index>>2:index>>1,l=f.bits==16?(index>>1)&1:index&1,m=f.bits==16?index&1:rm>>4;
 return base|(f.width==128?0x40000000u:0)|((f.bits==16?1u:f.bits==32?2u:f.bits==8?0u:3u)<<22)|(l<<21)|(m<<20)|((rm&15)<<16)|(h<<11)|(rn<<5)|rd;
}
// Independent signed 128-bit mathematical product/accumulator, explicitly reduced modulo output width.
Vector128 reference(const Inputs& in,Form f,unsigned rn,unsigned rm,unsigned rd,unsigned index){
 Vector128 out{};const __int128 modulus=__int128(1)<<destination_bits(f),input_modulus=__int128(1)<<f.bits;__int128 factor=in.vectors[rm].lane(index,f.bits);if(f.widening&&!f.unsigned_form&&factor&(input_modulus>>1))factor-=input_modulus;
 const unsigned start=f.widening&&f.width==128?64/f.bits:0;
 for(unsigned lane=0;lane<elements(f);++lane){__int128 a=in.vectors[rn].lane(start+lane,f.bits);if(f.widening&&!f.unsigned_form&&a&(input_modulus>>1))a-=input_modulus;const __int128 product=a*factor,old=in.vectors[rd].lane(lane,destination_bits(f));__int128 value=f.operation==Multiply?product:f.operation==Add?old+product:old-product;value%=modulus;if(value<0)value+=modulus;out.lane(lane,destination_bits(f),std::uint64_t(value));}return out;
}

std::uint64_t seed=0x4f858210;
std::uint64_t random_value(){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
Inputs inputs(){Inputs in;for(unsigned r=0;r<32;++r){in.vectors[r]={random_value(),random_value()};in.gpr[r]=random_value();}return in;}
unsigned mode(unsigned n){return ((n&3)<<22)|((n&4)?1u<<24:0)|((n&8)?1u<<25:0);}

void execute(MinimalGuest g,std::uint32_t op,Vector128 expected,unsigned flags,bool restore){
 g.write(g.registers().pc,op,4);const auto cpu=g.registers();const auto fp=g.floating();const auto memory=g.runtime().memory_persistent_snapshot(),core=g.runtime().core_snapshot();
 if(restore){auto resumed=MinimalGuest::from_snapshot(g.snapshot());g.run(1);resumed.run(1);check(g.snapshot()==resumed.snapshot(),"indexed integer multiply changed after original checkpoint decoding");}else g.run(1);
 const unsigned rd=op&31;check(g.floating().fpsr==(fp.fpsr|flags)&&g.floating().fpcr==fp.fpcr,"indexed integer multiply FPCR/FPSR differs");
 check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"indexed integer multiply did not advance exactly one instruction");
 check(g.registers().sp==cpu.sp&&g.registers().nzcv==cpu.nzcv,"indexed integer multiply changed SP or NZCV");
 check(g.runtime().memory_persistent_snapshot()==memory&&g.runtime().core_snapshot()==core,"indexed integer multiply changed XI/Ether memory/core");
 for(unsigned r=0;r<31;++r)check(g.registers().x[r]==cpu.x[r],"indexed integer multiply changed a GPR");
 for(unsigned r=0;r<32;++r){const auto value=r==rd?expected:fp.v[r];check(g.floating().v[r].low==value.low&&g.floating().v[r].high==value.high,"indexed integer multiply destination/alias/upper SIMD bits differ");}++cases;
}


void allocated_forms(){
 for(auto f:forms){auto in=inputs();const auto base=prepared(in,0,0x0800009f);for(unsigned rn=0;rn<32;++rn)for(unsigned rm=0;rm<registers(f);++rm)for(unsigned index=0;index<indices(f);++index)for(unsigned rd:{rn,rm,31u,(rn*7+rm*3+index+11)&31u})execute(base,encoding(f,rn,rm,rd,index),reference(in,f,rn,rm,rd,index),0,rn==rm&&rd==rn&&index==0);}
 auto actual=inputs();actual.vectors[5]={2508,0};actual.vectors[16]={UINT64_C(0x00000bbe00001373),UINT64_C(0x0000159800001980)};check(encoding({32,128,Multiply},16,5,16,0)==0x4f858210u,"Observed MUL encoding differs");execute(prepared(actual,0,17),0x4f858210u,{UINT64_C(0x0073096800be8aa4),UINT64_C(0x00d38d2000f9d200)},0,true);
}
void edge_and_random_cases(){
 for(auto f:forms){const auto mask=(UINT64_C(1)<<f.bits)-1,sign=UINT64_C(1)<<(f.bits-1);const std::uint64_t edge[]{0,1,2,mask,mask-1,sign,sign-1,sign+1,0x55555555&mask,0xaaaaaaaa&mask,0x8001,0x7fff};
  for(unsigned a=0;a<12;++a)for(unsigned b=0;b<12;++b)for(unsigned acc=0;acc<4;++acc)for(unsigned index=0;index<indices(f);++index){auto in=inputs();for(unsigned e=0;e<128/f.bits;++e)in.vectors[10].lane(e,f.bits,edge[(a+e)%12]);const auto destination_mask=ones(destination_bits(f)),destination_sign=UINT64_C(1)<<(destination_bits(f)-1);for(unsigned e=0;e<elements(f);++e)in.vectors[8].lane(e,destination_bits(f),acc==0?0:acc==1?destination_mask:acc==2?destination_sign:destination_sign-1);in.vectors[11].lane(index,f.bits,edge[b]);execute(prepared(in,mode(a+b+acc),0x0800009f),encoding(f,10,11,8,index),reference(in,f,10,11,8,index),0,a==0&&b==0&&acc==0);}
  for(unsigned n=0;n<2048;++n){auto in=inputs();const unsigned rn=n%32,rm=(n*7+11)%registers(f),rd=n%4==0?rn:n%4==1?rm:n%4==2?31:(n*5+3)&31,index=n%indices(f);execute(prepared(in,mode(n),0x0800009f),encoding(f,rn,rm,rd,index),reference(in,f,rn,rm,rd,index),0,n==0);}
 }
}
void traps_and_rollback(){
 for(auto f:forms){MinimalGuest g(128*1024*1024);g.load_elf(elf({encoding(f,10,11,8,0)},true),true);g.run(1);check(g.machine().exceptions==1&&(g.machine().esr>>26)==7&&!g.floating().nonzero(),"Indexed integer multiply bypassed FP/SIMD access trap");}
 for(auto f:forms)for(unsigned bits:{8u,64u}){f.bits=bits;MinimalGuest g(65536);g.load_elf(elf({encoding(f,10,11,8,0)}));const auto before=g.snapshot();bool rejected=false;try{g.run(1);}catch(const std::exception&){rejected=true;}check(rejected&&g.snapshot()==before,"Reserved indexed integer multiply size changed original state");}
}

#ifdef __aarch64__
struct NativeCode {
 void* page=nullptr;std::size_t bytes=0;
 explicit NativeCode(std::uint32_t op){
  bytes=std::size_t(::sysconf(_SC_PAGESIZE));page=::mmap(nullptr,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);check(page!=MAP_FAILED,"Cannot allocate actual AArch64 indexed integer multiply oracle");auto* code=static_cast<std::uint32_t*>(page);unsigned at=0;
  code[at++]=0xd10203ffu;for(unsigned r=8;r<16;r+=2)code[at++]=0xad000000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;
  code[at++]=0xaa0003efu;code[at++]=0xaa0103eeu;const unsigned rn=(op>>5)&31,rm=(op>>16)&31,rd=op&31;
  for(unsigned r=0;r<32;++r)code[at++]=0x3dc00000u|(r<<10)|(15<<5)|r;
  code[at++]=op;code[at++]=0x3d800000u|(14<<5)|rd;
  for(unsigned r=8;r<16;r+=2)code[at++]=0xad400000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;code[at++]=0x910203ffu;code[at++]=0xd65f03c0u;
  __builtin___clear_cache(static_cast<char*>(page),static_cast<char*>(page)+at*4);check(::mprotect(page,bytes,PROT_READ|PROT_EXEC)==0,"Cannot publish actual AArch64 indexed integer multiply oracle");
 }
 ~NativeCode(){if(page&&page!=MAP_FAILED)::munmap(page,bytes);}
 Vector128 run(const Inputs& values,unsigned fpcr,unsigned initial_sr,unsigned& flags){
  std::uint64_t oldcr,oldsr,sr;asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr":"=r"(oldcr),"=r"(oldsr));const std::uint64_t cr=fpcr,initial=initial_sr;asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(cr),"r"(initial):"memory");Vector128 out;reinterpret_cast<void(*)(const Inputs*,Vector128*)>(page)(&values,&out);asm volatile("mrs %0, fpsr":"=r"(sr));asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(oldcr),"r"(oldsr):"memory");flags=unsigned(sr);return out;
 }
};

void hardware_oracle(){
 {auto in=inputs();in.vectors[5]={2508,0};in.vectors[16]={UINT64_C(0x00000bbe00001373),UINT64_C(0x0000159800001980)};NativeCode native(0x4f858210u);unsigned flags;const auto actual=native.run(in,0,17,flags);check(flags==17&&actual.low==UINT64_C(0x0073096800be8aa4)&&actual.high==UINT64_C(0x00d38d2000f9d200),"Actual Android opcode or operands differ on physical ARM");execute(prepared(in,0,17),0x4f858210u,actual,0,true);++native_cases;}
 for(auto f:forms){auto in=inputs();const auto base=prepared(in,0,0x0800009f);for(unsigned rn=0;rn<32;++rn)for(unsigned rm=0;rm<registers(f);++rm)for(unsigned index=0;index<indices(f);++index){const unsigned choice=(rn+rm+index)%4,rd=choice==0?rn:choice==1?rm:choice==2?31:(rn*7+rm*3+index+11)&31;const auto op=encoding(f,rn,rm,rd,index);NativeCode native(op);unsigned flags;const auto actual=native.run(in,0,0x0800009f,flags),expected=reference(in,f,rn,rm,rd,index);check(flags==0x0800009f,"Actual indexed integer multiply changed FPSR");check(actual.low==expected.low&&actual.high==expected.high,"Actual ARM differs from independent mathematical oracle");execute(base,op,actual,0,false);++native_cases;}
  for(unsigned n=0;n<1024;++n){auto in=inputs();const unsigned rn=n%32,rm=(n*7+11)%registers(f),rd=n%4==0?rn:n%4==1?rm:n%4==2?31:(n*5+3)&31,index=n%indices(f),fpcr=mode(n),fpsr=n&1?2:0x0800009f;const auto op=encoding(f,rn,rm,rd,index);NativeCode native(op);unsigned flags;const auto actual=native.run(in,fpcr,fpsr,flags),expected=reference(in,f,rn,rm,rd,index);check(flags==fpsr,"Actual random indexed integer multiply changed FPSR");check(actual.low==expected.low&&actual.high==expected.high,"Actual random ARM differs from mathematical oracle");execute(prepared(in,fpcr,fpsr),op,actual,0,n==0);++native_cases;}
 }
}
#else
void hardware_oracle(){}
#endif
}
int main(){try{allocated_forms();edge_and_random_cases();traps_and_rollback();hardware_oracle();std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"native_hardware_cases\":"<<native_cases<<",\"indexed_integer_forms\":36,\"exact_android_opcode\":\"0x4f858210\",\"android_boot\":false}\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
