#pragma once
// Opt-in microarchitectural counter-read latency. A host-supplied seed is
// persistent hardware state; the cache below is derived, never guest RAM.
#include "minimal_guest_block.hpp"
namespace reverse_pc::guest {
struct GuestCounterLatency {
  bool enabled=false;
  std::array<std::uint8_t,32> seed{};
  std::uint64_t reads=0;
  std::uint64_t instructions_per_tick=1,subtick=0;
  unsigned instruction_tick(){
    if(!enabled||instructions_per_tick==1)return 1;
    if(++subtick==instructions_per_tick){subtick=0;return 1;}return 0;
  }
  unsigned next(){
    if(!enabled||reads==UINT64_MAX)throw std::runtime_error("Counter latency source is disabled or exhausted");
    const auto index=reads/32;
    if(!cached_||index!=cached_index_||seed!=cached_seed_){
      // HMAC-SHA256(seed, domain || little-endian block index). The source
      // seed comes from the host OS, not a fixed/predictable production key.
      std::vector<std::uint8_t> inner(64,0x36),outer(64,0x5c);
      for(unsigned i=0;i<32;i++){inner[i]^=seed[i];outer[i]^=seed[i];}
      const std::string domain="REVERSE-CNT-v1";inner.insert(inner.end(),domain.begin(),domain.end());
      for(unsigned i=0;i<8;i++)inner.push_back(std::uint8_t(index>>(8*i)));
      const auto digest=decode(GuestSha256::hash(inner));outer.insert(outer.end(),digest.begin(),digest.end());
      cache_=decode(GuestSha256::hash(outer));cached_=true;cached_index_=index;cached_seed_=seed;
    }
    return cache_[std::size_t(reads++%32)]&15u;
  }
  std::vector<std::uint8_t> save()const{
    if(!enabled)throw std::runtime_error("Cannot serialize disabled counter latency");
    if(instructions_per_tick<1||instructions_per_tick>4096||subtick>=instructions_per_tick)throw std::runtime_error("Invalid counter instruction cadence");
    const std::uint64_t version=instructions_per_tick==1?1:2;
    std::vector<std::uint8_t> b;auto put=[&](std::uint64_t n){for(unsigned i=0;i<8;i++)b.push_back(std::uint8_t(n>>(8*i)));};
    put(version);b.insert(b.end(),seed.begin(),seed.end());put(reads);
    if(version==2){put(instructions_per_tick);put(subtick);}return b;
  }
  static GuestCounterLatency restore(const std::vector<std::uint8_t>& b){
    if((b.size()!=48&&b.size()!=64)||(b[0]!=1&&b[0]!=2)||std::any_of(b.begin()+1,b.begin()+8,[](std::uint8_t v){return v!=0;})||(b[0]==1)!=(b.size()==48))throw std::runtime_error("Invalid counter latency state");
    GuestCounterLatency c;c.enabled=true;std::copy(b.begin()+8,b.begin()+40,c.seed.begin());
    for(unsigned i=0;i<8;i++)c.reads|=std::uint64_t(b[40+i])<<(8*i);
    if(b[0]==2){c.instructions_per_tick=0;for(unsigned i=0;i<8;i++){c.instructions_per_tick|=std::uint64_t(b[48+i])<<(8*i);c.subtick|=std::uint64_t(b[56+i])<<(8*i);}
      if(c.instructions_per_tick<2||c.instructions_per_tick>4096||c.subtick>=c.instructions_per_tick)throw std::runtime_error("Invalid counter instruction cadence");}
    return c;
  }
 private:
  static std::array<std::uint8_t,32> decode(const std::string& text){
    std::array<std::uint8_t,32> b{};auto digit=[](char c){return unsigned(c<='9'?c-'0':c-'a'+10);};
    for(unsigned i=0;i<32;i++)b[i]=std::uint8_t(digit(text[2*i])*16+digit(text[2*i+1]));return b;
  }
  bool cached_=false;std::uint64_t cached_index_=0;
  std::array<std::uint8_t,32> cached_seed_{},cache_{};
};
} // namespace reverse_pc::guest
