// Full-register structure transfers, checked with an independent byte oracle.
// Synthetic ISA tests, not Android boot or display evidence.
#include "minimal_guest.hpp"
#include <iostream>
using namespace reverse_pc::guest;
namespace {
unsigned checks=0,cases=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
Bytes elf(const std::vector<std::uint32_t>& words,bool system=false){
  Bytes code;for(auto x:words)put(code,x,4);const auto entry=system?0x40080000:0x1000;
  Bytes b{0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
  put(b,2,2);put(b,183,2);put(b,1,4);put(b,entry);put(b,64);put(b,0);
  put(b,0,4);put(b,64,2);put(b,56,2);put(b,1,2);put(b,0,2);put(b,0,2);put(b,0,2);
  put(b,1,4);put(b,7,4);put(b,0x100);put(b,entry);put(b,entry);put(b,code.size());put(b,code.size());put(b,0x100);
  b.resize(0x100);b.insert(b.end(),code.begin(),code.end());return b;
}
std::uint32_t mov(unsigned r,unsigned n){return 0xd2800000u|(n<<5)|r;}
unsigned count_for(unsigned op){return op==0?4:op==4?3:op==8?2:op==7?1:op==10?2:op==6?3:4;}
bool inter(unsigned op){return op==0||op==4||op==8;}
std::uint32_t encoding(unsigned op,unsigned size,bool wide,bool load,unsigned post,unsigned rd,unsigned rn,unsigned rm){
  return 0x0c000000u|(wide?0x40000000u:0)|(load?0x00400000u:0)|
    (post?0x00800000u|((post==1?31:rm)<<16):0)|(op<<12)|(size<<10)|(rn<<5)|rd;
}
MinimalGuest prepared(unsigned rn,unsigned rm,unsigned base,unsigned increment){
  std::vector<std::uint32_t> code{mov(2,0x2000)};
  for(unsigned n=0;n<32;n++)code.push_back(0x3dc00040u|(n<<10)|n);
  code.push_back(mov(rm,increment));
  if(rn==31){code.push_back(mov(15,base));code.push_back(0x910001ffu);} // ADD SP,X15,#0
  else code.push_back(mov(rn,base));
  const auto setup=code.size();code.push_back(0xd503201fu);
  MinimalGuest g(65536);g.load_elf(elf(code));
  for(unsigned r=0;r<32;r++)for(unsigned b=0;b<16;b++)g.write(0x2000+r*16+b,(r*61+b*37+0x83)&255,1);
  for(unsigned b=0;b<80;b++)g.write(base+b,(b*43+0x59)&255,1);
  g.write(base-1,0xa5,1);g.run(setup);return g;
}
void execute(MinimalGuest g,unsigned op,unsigned size,bool wide,bool load,unsigned post,unsigned rd,unsigned rn,unsigned rm,bool restore){
  const unsigned count=count_for(op),bytes=wide?16:8,eb=1u<<size;
  const auto base=rn==31?g.registers().sp:g.registers().x[rn];
  const auto increment=post==1?count*bytes:post==2?g.registers().x[rm]:0;
  g.write(g.registers().pc,encoding(op,size,wide,load,post,rd,rn,rm),4);
  const auto before=g.registers();const auto fp=g.floating();
  auto expected=fp;Bytes memory;for(unsigned b=0;b<count*bytes+16;b++)memory.push_back(g.read(base+b,1));
  // Oracle iterates registers and individual bytes, unlike the implementation's
  // flat element transfer loop. It independently maps each byte to memory.
  for(unsigned r=0;r<count;r++){
    if(load)expected.v[(rd+r)&31]={};
    for(unsigned b=0;b<bytes;b++){
      const unsigned address=inter(op)?((b/eb)*count+r)*eb+b%eb:r*bytes+b;
      if(load)expected.v[(rd+r)&31].lane(b,8,memory[address]);
      else memory[address]=fp.v[(rd+r)&31].lane(b,8);
    }
  }
  const auto qm=g.runtime().core_snapshot();
  if(restore){auto recovered=MinimalGuest::from_snapshot(g.snapshot());g.run(1);recovered.run(1);check(g.snapshot()==recovered.snapshot(),"Structure transfer changed after snapshot restore");}
  else g.run(1);
  for(unsigned r=0;r<32;r++)check(g.floating().v[r].low==expected.v[r].low&&g.floating().v[r].high==expected.v[r].high,"Structure register/lane order or upper half differs");
  for(unsigned b=0;b<memory.size();b++)check(g.read(base+b,1)==memory[b],"Structure memory differs from byte oracle or overwrote guard");
  check(g.read(base-1,1)==0xa5,"Structure wrote before base");
  for(unsigned r=0;r<31;r++)check(g.registers().x[r]==before.x[r]+((post&&r==rn)?increment:0),"Structure GPR or writeback differs");
  check(g.registers().sp==before.sp+((post&&rn==31)?increment:0),"Structure SP writeback differs");
  check(g.registers().nzcv==before.nzcv&&g.floating().fpcr==fp.fpcr&&g.floating().fpsr==fp.fpsr,"Structure changed flags or FP control");
  check(g.registers().instructions==before.instructions+1&&g.registers().pc==before.pc+4,"Structure did not execute exactly one instruction");
  check(g.runtime().core_snapshot()==qm,"Structure changed quantum state");++cases;
}
void sweep(){
  const auto g=prepared(12,17,0x3001,93);
  for(unsigned op:{0u,4u,8u,7u,10u,6u,2u})for(unsigned size=0;size<4;size++)for(bool wide:{false,true}){
    if(inter(op)&&!wide&&size==3)continue;
    for(bool load:{false,true})for(unsigned post=0;post<3;post++)for(unsigned rd=0;rd<32;rd++)
      execute(g,op,size,wide,load,post,rd,12,17,rd==31);
  }
  // Base and index overlap, all bases including SP, vector-register wrap.
  for(unsigned rn=0;rn<32;rn++)for(unsigned op:{0u,4u,8u})for(bool load:{false,true}){
    const unsigned rm=rn==31?17:rn;auto alias=prepared(rn,rm,0x3000,127);
    execute(alias,op,2,true,load,2,30,rn,rm,true);
  }
  auto exact=prepared(12,17,0x3000,93);exact.apply_xi_delta("h 4095\n");
  check(encoding(0,0,true,false,1,18,12,17)==0x4c9f0192u,"Exact C10 ST4 regression encoding differs");
  execute(exact,0,0,true,false,1,18,12,17,true);
}
void reject(std::uint32_t op){
  MinimalGuest g(65536);g.load_elf(elf({op}));const auto before=g.snapshot();bool failed=false;
  try{g.run(1);}catch(const std::exception&){failed=true;}
  check(failed&&g.snapshot()==before,"Reserved structure encoding accepted or changed state");
}
void controls(){
  for(unsigned op=0;op<16;op++)if(op!=0&&op!=2&&op!=4&&op!=6&&op!=7&&op!=8&&op!=10)
    for(bool load:{false,true})for(bool wide:{false,true})for(unsigned size=0;size<4;size++)
      reject(encoding(op,size,wide,load,1,31,12,17));
  for(unsigned op:{0u,4u,8u})for(bool load:{false,true})for(unsigned post=0;post<3;post++)
    reject(encoding(op,3,false,load,post,31,12,17)); // .1D is unallocated for interleaved structures.
  for(unsigned op:{0u,4u,8u,7u,10u,6u,2u})for(bool load:{false,true})
    reject(encoding(op,0,true,load,0,31,12,17)|(17u<<16)); // No-offset Rm must be zero.
  for(unsigned op:{0u,4u,8u})for(bool load:{false,true}){
    // Last element falls outside RAM: no partial store, vector, base or clock update.
    MinimalGuest bad(65536);bad.load_elf(elf({mov(12,65520),encoding(op,0,true,load,1,31,12,17)}));bad.run(1);
    const auto before=bad.snapshot();bool failed=false;try{bad.run(1);}catch(const std::exception&){failed=true;}
    check(failed&&bad.snapshot()==before,"Faulting structure transfer committed partial state");
    // FP-disabled system state must trap before any memory access or writeback.
    MinimalGuest trapped(128*1024*1024);trapped.load_elf(elf({encoding(op,0,true,load,1,31,12,17)},true),true);
    trapped.run(1);check(trapped.machine().exceptions==1&&(trapped.machine().esr>>26)==7&&!trapped.floating().nonzero()&&trapped.registers().x[12]==0,"Structure bypassed FP access trap");
  }
}
}
int main(){try{sweep();controls();std::cout<<"{\"checks\":"<<checks<<",\"structure_cases\":"<<cases<<",\"exact_c10_opcode\":\"0x4c9f0192\",\"android_boot\":false}\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
