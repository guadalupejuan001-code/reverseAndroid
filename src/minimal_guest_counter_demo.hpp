#pragma once
#include "minimal_guest.hpp"
namespace reverse_pc::guest {
// Actual MRS/STR instructions collect 64 physical/virtual counter samples.
// The fixed seed used by tests is never the production entropy source.
inline Bytes counter_witness_image(){
  Bytes b(4096);auto data=[&](unsigned at,std::uint64_t n,unsigned size){for(unsigned i=0;i<size;i++)b.at(at+i)=std::uint8_t(n>>(8*i));};
  data(0,0x14000040,4);data(8,0x80000,8);data(16,b.size(),8);data(56,0x644d5241,4);
  unsigned at=256;auto emit=[&](std::uint32_t n){data(at,n,4);at+=4;};
  auto mov=[&](unsigned r,std::uint64_t n){for(unsigned i=0;i<4;i++)emit((i?0xf2800000u:0xd2800000u)|(i<<21)|(unsigned((n>>(16*i))&65535)<<5)|r);};
  mov(1,0x44000000);emit(0xd53be002); // MRS X2,CNTFRQ_EL0
  for(unsigned i=0;i<64;i++){emit(i%2?0xd53be020u:0xd53be040u);emit(0xf9000020u|(i<<10));}
  for(const char c:std::string("REVERSE: COUNTER LATENCY OK\n")){
    mov(0,MachineState::uart_base);mov(3,std::uint8_t(c));emit(0x39000003);
  }
  mov(0,0x84000008);emit(0xd4000002);require(at<b.size(),"Counter witness exceeds immutable image");return b;
}
inline std::array<std::uint8_t,32> counter_test_seed(){std::array<std::uint8_t,32> s{};for(unsigned i=0;i<s.size();i++)s[i]=std::uint8_t(i);return s;}
inline MinimalGuest counter_witness(){MinimalGuest g(128*1024*1024,0);g.load_linux_image(counter_witness_image());g.enable_counter_latency(counter_test_seed());return g;}
inline MinimalGuest counter_prescaled_witness(){auto g=counter_witness();g.configure_counter_rate(64);return g;}
} // namespace reverse_pc::guest
