// C39 differential witness: ARM64 execution, physical XI read/write,
// conditional branches, MADD and self-modifying code, with a byte-for-byte
// restorable RVG snapshot. Compile with reference macro flags and without.
#include "minimal_guest.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
using namespace reverse_pc::guest;
static Bytes program(){
  static constexpr std::uint32_t code[]={
    0xd2800000,0xd2824681,0xd2840003,0xf2a80003,0xf9000060,
    0xf9400064,0x91000484,0xf9000064,0xaa0403e5,0xca0100a5,
    0x9b017ca6,0xeb01009f,0x54ffff21,0x14000000
  };
  Bytes out(0x100+sizeof(code),0);
  out[0]=0x7f;out[1]='E';out[2]='L';out[3]='F';out[4]=2;out[5]=1;out[6]=1;
  auto set=[&](std::size_t at,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)out[at+i]=std::uint8_t(v>>(8*i));};
  set(16,2,2);set(18,183,2);set(20,1,4);set(24,0x40001000,8);set(32,64,8);
  set(52,64,2);set(54,56,2);set(56,1,2);
  set(64,1,4);set(68,5,4);set(72,0x100,8);set(80,0x40001000,8);set(88,0x40001000,8);
  set(96,sizeof(code),8);set(104,sizeof(code),8);set(112,0x100,8);
  for(unsigned i=0;i<sizeof(code)/4;i++)set(0x100+4*i,code[i],4);
  return out;
}
int main(int argc,char**argv){
  unsigned total=500000;if(argc>=2)total=unsigned(std::stoul(argv[1]));
  unsigned chunk=50000;if(argc>=3)chunk=unsigned(std::stoul(argv[2]));
  if(!total||!chunk||total%chunk)throw std::runtime_error("budget and chunk must be positive and divisible");
  MinimalGuest guest(64*1024*1024);guest.load_elf(program(),true);
  const auto start=std::chrono::steady_clock::now();
  for(unsigned i=0;i<total;i+=chunk){
    const auto r=guest.run(chunk);
    if(r.halted||r.executed!=chunk)throw std::runtime_error("unexpected stop");
  }
  if(guest.read(0x40002000,8)!=4660)throw std::runtime_error("XI/Ether read/write differs");
  if(guest.registers().pc!=0x40001034)throw std::runtime_error("branch target differs");
  const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
  const auto before=guest.snapshot();
  const auto restored=MinimalGuest::from_snapshot(before);
  if(restored.snapshot()!=before)throw std::runtime_error("nonpersistent cache leaked into RVG");
  guest.write(0x40001034,0xd503201f,4);
  const auto single=guest.run(1);
  if(single.executed!=1||guest.registers().pc!=0x40001038)throw std::runtime_error("stale decoded self-modifying code");
  const auto state=guest.snapshot();
  std::cout<<"{\"instructions\":"<<guest.registers().instructions
      <<",\"counter\":"<<guest.machine().ticks<<",\"memory\":"<<guest.read(0x40002000,8)
      <<",\"pc\":\"0x"<<std::hex<<guest.registers().pc
      <<"\",\"snapshot_hash\":\"0x"<<std::hex<<digest(state,state.size())
      <<"\",\"elapsed_seconds\":"<<std::dec<<seconds<<"}\n";
}
