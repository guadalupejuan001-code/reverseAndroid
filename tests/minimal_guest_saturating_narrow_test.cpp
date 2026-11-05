#include <fstream>
// Saturating narrowing conversions, with an independent mathematical oracle.
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





struct Form {unsigned bits,kind;bool scalar,upper;};
const Form forms[]{{8,0,false,false},{8,0,false,true},{8,0,true,false},{16,0,false,false},{16,0,false,true},{16,0,true,false},{32,0,false,false},{32,0,false,true},{32,0,true,false},{8,1,false,false},{8,1,false,true},{8,1,true,false},{16,1,false,false},{16,1,false,true},{16,1,true,false},{32,1,false,false},{32,1,false,true},{32,1,true,false},{8,2,false,false},{8,2,false,true},{8,2,true,false},{16,2,false,false},{16,2,false,true},{16,2,true,false},{32,2,false,false},{32,2,false,true},{32,2,true,false}};
static_assert(sizeof(forms)/sizeof(forms[0])==27,"Saturating narrow form count changed");
constexpr unsigned QC=0x08000000;
std::uint64_t ones(unsigned bits){return bits==64?UINT64_MAX:(UINT64_C(1)<<bits)-1;}
std::uint32_t encoding(Form f,unsigned rn,unsigned rd){
 const unsigned size=f.bits==8?0:f.bits==16?1:2;
 return (f.kind==2?0x2e212800u:0x0e214800u|(f.kind==1?0x20000000u:0))|
  (f.scalar?0x50000000u:f.upper?0x40000000u:0)|(size<<22)|(rn<<5)|rd;
}
struct Reference {Vector128 value;unsigned raised=0;};
// Independent mathematical signed-128 oracle: decode an integer, clamp to
// the destination interval, then encode its residue. No magnitude algorithm.
Reference reference(const Inputs& in,Form f,unsigned rn,unsigned rd){
 Reference out;Vector128 packed;const __int128 source_mod=__int128(1)<<(2*f.bits),dest_mod=__int128(1)<<f.bits;
 const __int128 minimum=f.kind==0?-dest_mod/2:0,maximum=f.kind==0?dest_mod/2-1:dest_mod-1;
 for(unsigned n=0;n<(f.scalar?1u:64/f.bits);++n){
  __int128 value=in.vectors[rn].lane(n,2*f.bits);
  if(f.kind!=1&&value>=source_mod/2)value-=source_mod;
  if(value<minimum){value=minimum;out.raised=QC;}else if(value>maximum){value=maximum;out.raised=QC;}
  if(value<0)value+=dest_mod;packed.lane(n,f.bits,std::uint64_t(value));
 }
 out.value=f.upper?Vector128{in.vectors[rd].low,packed.low}:packed;return out;
}
std::uint64_t seed=0x0e214841;
std::uint64_t random_value(){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
Inputs inputs(){Inputs in;for(unsigned r=0;r<32;++r){in.vectors[r]={random_value(),random_value()};in.gpr[r]=random_value();}return in;}
unsigned mode(unsigned n){return ((n&3)<<22)|((n&4)?1u<<24:0)|((n&8)?1u<<25:0);}
std::array<std::uint64_t,18> edges(unsigned bits){
 const auto mask=ones(2*bits),sign=UINT64_C(1)<<(2*bits-1),dest_sign=UINT64_C(1)<<(bits-1),dest_mask=ones(bits);
 return {0,1,2,mask,mask-1,sign,sign-1,sign+1,dest_sign-1,dest_sign,dest_sign+1,dest_mask,dest_mask+1,
  (UINT64_C(0)-dest_sign)&mask,(UINT64_C(0)-dest_sign-1)&mask,(UINT64_C(0)-dest_sign+1)&mask,
  UINT64_C(0x5555555555555555)&mask,UINT64_C(0xaaaaaaaaaaaaaaaa)&mask};
}

#ifdef __aarch64__
struct NativeCode {
 void* page=nullptr;std::size_t bytes=0;
 explicit NativeCode(std::uint32_t op){
  bytes=std::size_t(::sysconf(_SC_PAGESIZE));page=::mmap(nullptr,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);check(page!=MAP_FAILED,"Cannot allocate actual AArch64 rounding right shift oracle");auto* code=static_cast<std::uint32_t*>(page);unsigned at=0;
  code[at++]=0xd10203ffu;for(unsigned r=8;r<16;r+=2)code[at++]=0xad000000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;
  code[at++]=0xaa0003efu;code[at++]=0xaa0103eeu;const unsigned rn=(op>>5)&31,rm=(op>>16)&31,rd=op&31;
  for(unsigned r=0;r<32;++r)code[at++]=0x3dc00000u|(r<<10)|(15<<5)|r;
  code[at++]=op;code[at++]=0x3d800000u|(14<<5)|rd;
  for(unsigned r=8;r<16;r+=2)code[at++]=0xad400000u|((r-8)<<15)|((r+1)<<10)|(31<<5)|r;code[at++]=0x910203ffu;code[at++]=0xd65f03c0u;
  __builtin___clear_cache(static_cast<char*>(page),static_cast<char*>(page)+at*4);check(::mprotect(page,bytes,PROT_READ|PROT_EXEC)==0,"Cannot publish actual AArch64 rounding right shift oracle");
 }
 ~NativeCode(){if(page&&page!=MAP_FAILED)::munmap(page,bytes);}
 Vector128 run(const Inputs& values,unsigned fpcr,unsigned initial_sr,unsigned& flags){
  std::uint64_t oldcr,oldsr,sr;asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr":"=r"(oldcr),"=r"(oldsr));const std::uint64_t cr=fpcr,initial=initial_sr;asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(cr),"r"(initial):"memory");Vector128 out;reinterpret_cast<void(*)(const Inputs*,Vector128*)>(page)(&values,&out);asm volatile("mrs %0, fpsr":"=r"(sr));asm volatile("msr fpcr, %0\n\tmsr fpsr, %1"::"r"(oldcr),"r"(oldsr):"memory");flags=unsigned(sr);return out;
 }
};


#endif
void execute(MinimalGuest g,std::uint32_t op,Vector128 expected,unsigned flags,bool restore){
 g.write(g.registers().pc,op,4);const auto cpu=g.registers();const auto fp=g.floating();const auto memory=g.runtime().memory_persistent_snapshot(),core=g.runtime().core_snapshot();
 if(restore){auto resumed=MinimalGuest::from_snapshot(g.snapshot());g.run(1);resumed.run(1);check(g.snapshot()==resumed.snapshot(),"saturating narrowing changed after original checkpoint decoding");}else g.run(1);
 const unsigned rd=op&31;check(g.floating().fpsr==(fp.fpsr|flags)&&g.floating().fpcr==fp.fpcr,"saturating narrowing FPCR/FPSR differs");
 check(g.registers().pc==cpu.pc+4&&g.registers().instructions==cpu.instructions+1,"saturating narrowing did not advance exactly one instruction");
 check(g.registers().sp==cpu.sp&&g.registers().nzcv==cpu.nzcv,"saturating narrowing changed SP or NZCV");
 check(g.runtime().memory_persistent_snapshot()==memory&&g.runtime().core_snapshot()==core,"saturating narrowing changed XI/Ether memory/core");
 for(unsigned r=0;r<31;++r)check(g.registers().x[r]==cpu.x[r],"saturating narrowing changed a GPR");
 for(unsigned r=0;r<32;++r){const auto value=r==rd?expected:fp.v[r];check(g.floating().v[r].low==value.low&&g.floating().v[r].high==value.high,"saturating narrowing destination/alias/upper SIMD bits differ");}++cases;
}

void case_for(const Inputs& in,const MinimalGuest& base,Form f,unsigned rn,unsigned rd,unsigned cr,unsigned sr,bool restore=false){
 const auto op=encoding(f,rn,rd);const auto expected=reference(in,f,rn,rd);
#ifdef __aarch64__
 NativeCode hardware(op);unsigned flags=0;const auto actual=hardware.run(in,cr,sr,flags);
 check(actual.low==expected.value.low&&actual.high==expected.value.high,"Actual ARM narrow differs from signed-128 clamp oracle");
 check(flags==(sr|expected.raised),"Actual ARM cumulative saturation flag differs");++native_cases;
#endif
 execute(base,op,expected.value,expected.raised,restore);
}
void allocated_forms(){
 for(auto f:forms){const auto in=inputs();const auto base=prepared(in,0,0);
  for(unsigned rn=0;rn<32;++rn)for(unsigned rd=0;rd<32;++rd)case_for(in,base,f,rn,rd,0,0,rn==rd);
 }
 auto in=inputs();in.vectors[2]={UINT64_C(0xff800080007f0000),UINT64_C(0x7fff8000ffffff7f)};
 const Form f{8,0,false,false};check(encoding(f,2,1)==0x0e214841u,"Observed SQXTN operand encoding differs");
 case_for(in,prepared(in,0,16),f,2,1,0,16,true);
 // Exact original pre-SQXTN operands; all other registers remain independent fixtures.
 in=inputs();in.vectors[2]=Vector128{UINT64_C(0xffff0000ffff),UINT64_C(0x0)};in.vectors[1]=Vector128{UINT64_C(0x0),UINT64_C(0x0)};
 case_for(in,prepared(in,0,16),f,2,1,0,16,true);
}
void exhaustive_halfwords(){
 for(auto f:forms)if(f.bits==8){
  const unsigned count=f.scalar?1:8;
  for(unsigned start=0;start<65536;start+=count){
   auto in=inputs();for(unsigned n=0;n<count;++n)in.vectors[10].lane(n,16,start+n);
   const unsigned cr=mode(start/count),sr=(start/count)&1?16:0;
   case_for(in,prepared(in,cr,sr),f,10,8,cr,sr,start==0);
  }
 }
}
void edge_and_random_cases(){
 for(auto f:forms){const auto values=edges(f.bits);
  for(unsigned a=0;a<values.size();++a)for(unsigned state=0;state<3;++state){
   auto in=inputs();for(unsigned n=0;n<128/(2*f.bits);++n)in.vectors[10].lane(n,2*f.bits,values[(a+n)%values.size()]);
   const unsigned cr=mode(a+state),sr=state==0?0:state==1?16:QC|0x9f;
   case_for(in,prepared(in,cr,sr),f,10,8,cr,sr,state==0);
  }
  for(unsigned n=0;n<2048;++n){
   const auto in=inputs();const unsigned rn=random_value()%32,rd=n%3==0?rn:n%3==1?31:random_value()%32,cr=mode(n),sr=n&1?16:QC|0x9f;
   case_for(in,prepared(in,cr,sr),f,rn,rd,cr,sr,n==0);
  }
  // Inactive scalar source lanes cannot raise QC. All unused destination bits clear.
  if(f.scalar){auto in=inputs();in.vectors[10]={UINT64_MAX,UINT64_MAX};in.vectors[10].lane(0,2*f.bits,1);
   case_for(in,prepared(in,0,0),f,10,10,0,0,true);
  }
 }
}
void traps_and_rollback(){
 for(auto f:forms){
  const auto op=encoding(f,10,8);MinimalGuest system(128*1024*1024);system.load_elf(elf({op},true),true);system.run(1);
  check(system.machine().exceptions==1&&(system.machine().esr>>26)==7&&!system.floating().nonzero(),"Saturating narrow bypassed FP/SIMD access trap");
  const auto bad=(op&~(3u<<22))|(3u<<22);
  for(bool privileged:{false,true}){MinimalGuest g(privileged?128*1024*1024:65536);g.load_elf(elf({bad},privileged),privileged);const auto before=g.snapshot();bool rejected=false;
   try{g.run(1);}catch(const std::exception&){rejected=true;}
   check(rejected&&g.snapshot()==before,"Reserved narrowing width changed state or trapped before rejection");
  }
 }
}
}
int main(int argc,char** argv){try{
 if(argc==3&&std::string(argv[1])=="--admission"){
  std::ifstream list(argv[2]);require(bool(list),"Cannot read opcode list");Inputs in;in.gpr.fill(0x2000);const auto base=prepared(in,0,0);std::string text;
  while(list>>text){auto guest=base;const auto word=std::uint32_t(std::stoul(text,nullptr,16));guest.write(guest.registers().pc,word,4);std::string status="admitted",error;
   try{guest.run(1);}catch(const std::exception& e){error=e.what();status=error.find("Unsupported AArch64 instruction")!=std::string::npos?"unsupported":"recognized_operand_fault";}
   std::cout<<"{\"word\":"<<quote(text)<<",\"status\":"<<quote(status)<<",\"error\":"<<quote(error)<<"}\n";
  }return 0;
 }
 require(argc==1,"Unknown saturating narrowing test argument");
 allocated_forms();exhaustive_halfwords();edge_and_random_cases();traps_and_rollback();
 std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"native_hardware_cases\":"<<native_cases<<",\"saturating_narrow_forms\":27,\"exact_android_opcode\":\"0x0e214841\",\"android_boot\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
