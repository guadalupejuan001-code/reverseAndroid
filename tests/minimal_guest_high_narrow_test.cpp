#include <fstream>
// Narrowing high-half addition and subtraction, with an independent mathematical oracle.
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




struct Form {unsigned bits;bool upper,rounded,subtract;};
const Form forms[]{{8,false,false,false},{16,false,false,false},{32,false,false,false},{8,false,false,true},{16,false,false,true},{32,false,false,true},{8,false,true,false},{16,false,true,false},{32,false,true,false},{8,false,true,true},{16,false,true,true},{32,false,true,true},{8,true,false,false},{16,true,false,false},{32,true,false,false},{8,true,false,true},{16,true,false,true},{32,true,false,true},{8,true,true,false},{16,true,true,false},{32,true,true,false},{8,true,true,true},{16,true,true,true},{32,true,true,true}};
static_assert(sizeof(forms)/sizeof(forms[0])==24,"High narrowing form count changed");
std::uint64_t ones(unsigned bits){return bits==64?UINT64_MAX:(UINT64_C(1)<<bits)-1;}
std::uint32_t encoding(Form f,unsigned rn,unsigned rm,unsigned rd){
 const unsigned size=f.bits==8?0:f.bits==16?1:f.bits==32?2:3;
 return 0x0e204000u|(size<<22)|(f.upper?0x40000000u:0)|(f.rounded?0x20000000u:0)|(f.subtract?0x2000u:0)|(rm<<16)|(rn<<5)|rd;
}
// Independent signed 128-bit mathematical sum/difference, reduced modulo
// the full source width before selecting the high narrow half.
Vector128 reference(const Inputs& in,Form f,unsigned rn,unsigned rm,unsigned rd){
 Vector128 out=f.upper?in.vectors[rd]:Vector128{};const __int128 modulus=__int128(1)<<(2*f.bits),divisor=__int128(1)<<f.bits,bias=f.rounded?__int128(1)<<(f.bits-1):0;
 for(unsigned n=0;n<64/f.bits;++n){const __int128 a=in.vectors[rn].lane(n,2*f.bits),b=in.vectors[rm].lane(n,2*f.bits);__int128 value=(f.subtract?a-b:a+b)+bias;value%=modulus;if(value<0)value+=modulus;out.lane((f.upper?64/f.bits:0)+n,f.bits,std::uint64_t(value/divisor));}return out;
}
std::uint64_t seed=0x2e254082;
std::uint64_t random_value(){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
Inputs inputs(){Inputs in;for(unsigned r=0;r<32;++r){in.vectors[r]={random_value(),random_value()};in.gpr[r]=random_value();}return in;}
unsigned mode(unsigned n){return ((n&3)<<22)|((n&4)?1u<<24:0)|((n&8)?1u<<25:0);}
std::array<std::uint64_t,12> edges(unsigned bits){const auto mask=ones(bits),sign=UINT64_C(1)<<(bits-1);return {0,1,2,mask,mask-1,sign,sign-1,sign+1,UINT64_C(0x5555555555555555)&mask,UINT64_C(0xaaaaaaaaaaaaaaaa)&mask,UINT64_C(0xff00ff00ff00ff00)&mask,UINT64_C(0x0080008000800080)&mask};}
Inputs android_input(){auto in=inputs();in.vectors[2]={0,0};in.vectors[4]={0,0};in.vectors[5]={0,0};return in;}

void execute(MinimalGuest g,std::uint32_t op,Vector128 expected,unsigned flags,bool restore){
 g.write(g.registers().pc,op,4);const auto cpu=g.registers();const auto fp=g.floating();const auto memory=g.runtime().memory_persistent_snapshot(),core=g.runtime().core_snapshot();
 if(restore){auto resumed=MinimalGuest::from_snapshot(g.snapshot());g.run(1);resumed.run(1);check(g.snapshot()==resumed.snapshot(),"high narrowing arithmetic changed after original checkpoint decoding");}else g.run(1);
 const unsigned rd=op&31;check(g.floating().fpsr==(fp.fpsr|flags)&&g.floating().fpcr==fp.fpcr,"high narrowing arithmetic FPCR/FPSR differs");
 check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"high narrowing arithmetic did not advance exactly one instruction");
 check(g.registers().sp==cpu.sp&&g.registers().nzcv==cpu.nzcv,"high narrowing arithmetic changed SP or NZCV");
 check(g.runtime().memory_persistent_snapshot()==memory&&g.runtime().core_snapshot()==core,"high narrowing arithmetic changed XI/Ether memory/core");
 for(unsigned r=0;r<31;++r)check(g.registers().x[r]==cpu.x[r],"high narrowing arithmetic changed a GPR");
 for(unsigned r=0;r<32;++r){const auto value=r==rd?expected:fp.v[r];check(g.floating().v[r].low==value.low&&g.floating().v[r].high==value.high,"high narrowing arithmetic destination/alias/upper SIMD bits differ");}++cases;
}


void allocated_forms(){
 for(auto f:forms){const auto in=inputs();const auto base=prepared(in,0,0x0800009f);for(unsigned rn=0;rn<32;++rn)for(unsigned rm=0;rm<32;++rm)for(unsigned rd=0;rd<32;++rd)execute(base,encoding(f,rn,rm,rd),reference(in,f,rn,rm,rd),0,rn==rm&&rm==rd);}
 const auto in=android_input();check(encoding({8,false,true,false},4,5,2)==0x2e254082u,"Observed RADDHN encoding differs");execute(prepared(in,0,16),0x2e254082u,{0,0},0,true);
}
void edge_and_random_cases(){
 for(auto f:forms){const auto edge=edges(2*f.bits);const auto mask=ones(f.bits),sign=UINT64_C(1)<<(f.bits-1);
  for(unsigned a=0;a<edge.size();++a)for(unsigned b=0;b<edge.size();++b)for(unsigned old=0;old<4;++old){auto in=inputs();for(unsigned n=0;n<64/f.bits;++n){in.vectors[10].lane(n,2*f.bits,edge[(a+n)%edge.size()]);in.vectors[11].lane(n,2*f.bits,edge[(b+n)%edge.size()]);}for(unsigned n=0;n<128/f.bits;++n)in.vectors[8].lane(n,f.bits,old==0?0:old==1?mask:old==2?sign:sign-1);execute(prepared(in,mode(a+b+old),0x0800009f),encoding(f,10,11,8),reference(in,f,10,11,8),0,a==0&&b==0&&old==0);}
  for(unsigned n=0;n<2048;++n){const auto in=inputs();const unsigned rn=random_value()%32,rm=random_value()%32,rd=n%4==0?rn:n%4==1?rm:n%4==2?31:random_value()%32;execute(prepared(in,mode(n),n&1?2:0x0800009f),encoding(f,rn,rm,rd),reference(in,f,rn,rm,rd),0,n==0);}
 }
}
void traps_and_rollback(){
 for(auto f:forms){MinimalGuest g(128*1024*1024);g.load_elf(elf({encoding(f,10,11,8)},true),true);g.run(1);check(g.machine().exceptions==1&&(g.machine().esr>>26)==7&&!g.floating().nonzero(),"High narrowing arithmetic bypassed FP access trap");}
 for(auto f:forms){f.bits=64;MinimalGuest g(65536);g.load_elf(elf({encoding(f,10,11,8)}));const auto before=g.snapshot();bool rejected=false;try{g.run(1);}catch(const std::exception&){rejected=true;}check(rejected&&g.snapshot()==before,"Reserved narrowing size changed original state");}
}

#ifdef __aarch64__
struct NativeCode {
 void* page=nullptr;std::size_t bytes=0;
 explicit NativeCode(std::uint32_t op){
  bytes=std::size_t(::sysconf(_SC_PAGESIZE));page=::mmap(nullptr,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);check(page!=MAP_FAILED,"Cannot allocate actual AArch64 high narrowing arithmetic oracle");auto* code=static_cast<std::uint32_t*>(page);unsigned at=0;
  code[at++]=0xd10203ffu;for(unsigned r=8;r<16;r+=2)code[at++]=0xad000000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;
  code[at++]=0xaa0003efu;code[at++]=0xaa0103eeu;const unsigned rn=(op>>5)&31,rm=(op>>16)&31,rd=op&31;
  for(unsigned r=0;r<32;++r)code[at++]=0x3dc00000u|(r<<10)|(15<<5)|r;
  code[at++]=op;code[at++]=0x3d800000u|(14<<5)|rd;
  for(unsigned r=8;r<16;r+=2)code[at++]=0xad400000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;code[at++]=0x910203ffu;code[at++]=0xd65f03c0u;
  __builtin___clear_cache(static_cast<char*>(page),static_cast<char*>(page)+at*4);check(::mprotect(page,bytes,PROT_READ|PROT_EXEC)==0,"Cannot publish actual AArch64 high narrowing arithmetic oracle");
 }
 ~NativeCode(){if(page&&page!=MAP_FAILED)::munmap(page,bytes);}
 Vector128 run(const Inputs& values,unsigned fpcr,unsigned initial_sr,unsigned& flags){
  std::uint64_t oldcr,oldsr,sr;asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr":"=r"(oldcr),"=r"(oldsr));const std::uint64_t cr=fpcr,initial=initial_sr;asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(cr),"r"(initial):"memory");Vector128 out;reinterpret_cast<void(*)(const Inputs*,Vector128*)>(page)(&values,&out);asm volatile("mrs %0, fpsr":"=r"(sr));asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(oldcr),"r"(oldsr):"memory");flags=unsigned(sr);return out;
 }
};

void hardware_case(const Inputs& in,const MinimalGuest& base,Form f,unsigned rn,unsigned rm,unsigned rd,unsigned fpcr,unsigned fpsr,bool restore=false){
 const auto op=encoding(f,rn,rm,rd);NativeCode native(op);unsigned flags;const auto actual=native.run(in,fpcr,fpsr,flags),expected=reference(in,f,rn,rm,rd);check(flags==fpsr,"Actual narrowing arithmetic changed FPSR");check(actual.low==expected.low&&actual.high==expected.high,"Actual ARM differs from independent narrowing oracle");execute(base,op,actual,0,restore);++native_cases;
}
void hardware_oracle(){
 {const auto in=android_input();hardware_case(in,prepared(in,0,16),{8,false,true,false},4,5,2,0,16,true);}
 for(auto f:forms){const auto in=inputs();const auto base=prepared(in,0,0x0800009f);
  for(unsigned rn=0;rn<32;++rn)for(unsigned rm=0;rm<32;++rm)for(unsigned rd=0;rd<32;++rd)hardware_case(in,base,f,rn,rm,rd,0,0x0800009f);
  const auto edge=edges(2*f.bits);for(unsigned a=0;a<edge.size();++a)for(unsigned b=0;b<edge.size();++b){auto in=inputs();for(unsigned n=0;n<64/f.bits;++n){in.vectors[10].lane(n,2*f.bits,edge[(a+n)%edge.size()]);in.vectors[11].lane(n,2*f.bits,edge[(b+n)%edge.size()]);}const unsigned cr=mode(a+b),sr=a&1?2:0x0800009f;hardware_case(in,prepared(in,cr,sr),f,10,11,8,cr,sr);}
  for(unsigned n=0;n<1024;++n){const auto in=inputs();const unsigned rn=random_value()%32,rm=random_value()%32,rd=n%4==0?rn:n%4==1?rm:n%4==2?31:random_value()%32,cr=mode(n),sr=n&1?2:0x0800009f;hardware_case(in,prepared(in,cr,sr),f,rn,rm,rd,cr,sr,n==0);}
 }
}
#else
void hardware_oracle(){}
#endif
}
int main(int argc,char** argv){try{
 if(argc==3&&std::string(argv[1])=="--admission"){
  std::ifstream list(argv[2]);require(bool(list),"Cannot read opcode list");Inputs values;values.gpr.fill(0x2000);const auto base=prepared(values,0,0);std::string word;
  while(list>>word){auto guest=base;guest.write(guest.registers().pc,std::uint32_t(std::stoul(word,nullptr,16)),4);std::string status="admitted",error;try{guest.run(1);}catch(const std::exception& e){error=e.what();status=error.find("Unsupported AArch64 instruction")!=std::string::npos?"unsupported":"recognized_operand_fault";}std::cout<<"{\"word\":"<<quote(word)<<",\"status\":"<<quote(status)<<",\"error\":"<<quote(error)<<"}\n";}return 0;
 }
 require(argc==1,"Unexpected test arguments");allocated_forms();edge_and_random_cases();traps_and_rollback();hardware_oracle();std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"native_hardware_cases\":"<<native_cases<<",\"high_narrow_forms\":24,\"exact_android_opcode\":\"0x2e254082\",\"android_boot\":false}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
