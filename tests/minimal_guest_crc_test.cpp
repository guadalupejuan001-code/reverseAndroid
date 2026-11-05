#include "minimal_guest.hpp"
#include <iostream>
using namespace reverse_pc::guest;
static unsigned checks=0;
static void check(bool ok,const char* why){++checks;require(ok,why);}
static Bytes elf(const std::vector<std::uint32_t>& words){
  Bytes b{0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
  put(b,2,2);put(b,183,2);put(b,1,4);put(b,0x1000);put(b,64);put(b,0);
  put(b,0,4);put(b,64,2);put(b,56,2);put(b,1,2);for(unsigned n=0;n<3;n++)put(b,0,2);
  put(b,1,4);put(b,7,4);put(b,256);put(b,0x1000);put(b,0x1000);put(b,words.size()*4);put(b,words.size()*4);put(b,256);
  b.resize(256);for(auto word:words)put(b,word,4);return b;
}
static void mov(std::vector<std::uint32_t>& words,unsigned rt,std::uint64_t n){for(unsigned i=0;i<4;i++)words.push_back((i?0xf2800000u:0xd2800000u)|(i<<21)|(unsigned((n>>(16*i))&65535)<<5)|rt);}
// Independent MSB-first polynomial long division, with explicit input/output
// reflection. The CPU implementation works LSB-first and consumes register bytes.
static std::uint32_t oracle(std::uint32_t crc,std::uint64_t data,unsigned bytes,bool castagnoli){
  auto reverse=[](std::uint32_t n){std::uint32_t r=0;for(unsigned i=0;i<32;i++)r=(r<<1)|((n>>i)&1);return r;};
  auto value=reverse(crc);const auto polynomial=castagnoli?UINT32_C(0x1edc6f41):UINT32_C(0x04c11db7);
  for(unsigned i=0;i<bytes*8;i++){const bool top=(value>>31)^((data>>i)&1);value<<=1;if(top)value^=polynomial;}
  return reverse(value);
}
int main(){try{
  for(bool castagnoli:{false,true})for(unsigned size=0;size<4;size++)for(unsigned alias=0;alias<3;alias++){
    const auto crc=UINT32_C(0xa751bd08);const auto data=UINT64_C(0xfedcba9876543210);const unsigned rd=alias==0?10:alias==1?8:9;
    std::vector<std::uint32_t> words;mov(words,8,UINT64_C(0xabcdef00a751bd08));mov(words,9,data);mov(words,10,UINT64_MAX);
    const auto opcode=0x1ac04000u|(size==3?0x80000000u:0u)|(castagnoli?0x1000u:0u)|(size<<10)|(9<<16)|(8<<5)|rd;words.push_back(opcode);
    MinimalGuest g(65536);g.load_elf(elf(words));g.run(12);const auto before=g.snapshot();auto restored=MinimalGuest::from_snapshot(before);
    const auto nzcv=g.registers().nzcv;g.run(1);restored.run(1);
    check(g.registers().x[rd]==oracle(crc,data,1u<<size,castagnoli),"CRC register width, polynomial, reflection or alias differs");
    check(g.snapshot()==restored.snapshot(),"CRC complete state differs after restore");
    check(g.registers().nzcv==nzcv&&g.runtime().memory().stats().overrides==0,"CRC changed flags or materialized guest RAM");
  }
  for(bool castagnoli:{false,true}){
    std::vector<std::uint32_t> words;mov(words,8,UINT32_MAX);
    for(unsigned char c:std::string("123456789")){mov(words,9,c);words.push_back(0x1ac94108u|(castagnoli?0x1000u:0u));}
    MinimalGuest g(65536);g.load_elf(elf(words));g.run(words.size());
    check((std::uint32_t(g.registers().x[8])^UINT32_MAX)==(castagnoli?UINT32_C(0xe3069283):UINT32_C(0xcbf43926)),"CRC failed the independent 123456789 standard check value");
  }
  for(auto invalid:{0x9ac9410au,0x1ac94d0au}){
    MinimalGuest g(65536);g.load_elf(elf({invalid}));const auto before=g.snapshot();bool rejected=false;try{g.run(1);}catch(const std::exception&){rejected=true;}
    check(rejected&&g.snapshot()==before,"Unallocated CRC size/sf encoding changed native state");
  }
  std::cout<<"{\"checks\":"<<checks<<",\"crc32\":true,\"crc32c\":true,\"standard_check_values\":true,\"independent_msb_oracle\":true,\"full_state_resume\":true}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
