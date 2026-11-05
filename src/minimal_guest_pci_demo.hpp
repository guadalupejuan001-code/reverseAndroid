#pragma once
#include "minimal_guest.hpp"
namespace reverse_pc::guest {
// A real AArch64 test program, not Android and not a pre-rendered screenshot.
// The CPU negotiates PCI virtio, submits DMA commands and powers itself off.
inline Bytes pci_graphics_witness_image(){
  constexpr std::uint64_t base=0x40080000,bar=0x10010000,desc=base+0x2000,avail=base+0x3000,used=base+0x4000,pixels=base+0x6000;
  Bytes image(65536);auto data=[&](unsigned at,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)image.at(at+i)=std::uint8_t(v>>(8*i));};
  data(0,0x14000040,4);data(8,0x80000,8);data(16,image.size(),8);data(56,0x644d5241,4);
  unsigned at=256;auto emit=[&](std::uint32_t v){data(at,v,4);at+=4;};
  auto mov=[&](unsigned r,std::uint64_t v){for(unsigned i=0;i<4;i++)emit((i?0xf2800000u:0xd2800000u)|(i<<21)|(unsigned((v>>(i*16))&65535)<<5)|r);};
  auto write=[&](std::uint64_t a,std::uint64_t v,unsigned n=4){mov(0,a);mov(1,v);emit(n==1?0x39000001u:n==2?0x79000001u:n==4?0xb9000001u:0xf9000001u);};
  write(GuestPciState::ecam+0x10004,6,2);write(bar+20,3,1);write(bar+8,1);write(bar+12,1);write(bar+20,11,1);
  write(bar+24,8,2);write(bar+32,desc,8);write(bar+40,avail,8);write(bar+48,used,8);write(bar+28,1,2);write(bar+20,15,1);
  const unsigned types[6]={0x100,0x101,0x106,0x105,0x103,0x104};const unsigned lengths[6]={24,40,48,56,48,48};
  data(0x2000+12,1,2);data(0x2000+14,1,2);data(0x2010,base+0x5000,8);data(0x2018,512,4);data(0x201c,2,2);
  for(unsigned i=0;i<6;i++){
    const unsigned c=0x1000+i*128;data(c,types[i],4);
    if(i==1){data(c+24,1,4);data(c+28,2,4);data(c+32,96,4);data(c+36,64,4);}
    if(i==2){data(c+24,1,4);data(c+28,1,4);data(c+32,pixels,8);data(c+40,96*64*4,4);}
    if(i>=3){data(c+32,96,4);data(c+36,64,4);data(c+(i==3?48:i==4?44:40),1,4);}
    write(desc,base+c,8);write(desc+8,lengths[i]);write(avail+2,i+1,2);write(bar+0x1000,0,2);
  }
  for(const char c:std::string("REVERSE: PCI GPU 2D OK\n"))write(MachineState::uart_base,std::uint8_t(c),1);
  mov(0,0x84000008);emit(0xd4000002);require(at<0x1000,"PCI witness code overlaps its DMA data");
  for(unsigned y=0;y<64;y++)for(unsigned x=0;x<96;x++){const unsigned color=x<48?(y<32?0xff4040:0x4080ff):(y<32?0x40ff80:0xffffff);data(0x6000+4*(y*96+x),color,4);}
  return image;
}
} // namespace reverse_pc::guest
