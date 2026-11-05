#pragma once
// Portable RVPC2 session transport shared with the existing REVERSE-PC CLI.
#include "mundo_x/quantum_runtime.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace reverse_pc::rvpc2 {
using Runtime=mundo_x::QuantumRuntime;
constexpr std::size_t max_state=32u*1024*1024;
inline std::uint64_t fnv(const std::string& a,const std::string& b={}) {
  std::uint64_t h=UINT64_C(14695981039346656037);
  for(unsigned char c:a){h^=c;h*=UINT64_C(1099511628211);}
  for(unsigned char c:b){h^=c;h*=UINT64_C(1099511628211);}
  return h;
}
inline void put64(std::vector<std::uint8_t>& out,std::uint64_t n) {
  for(unsigned i=0;i<8;++i)out.push_back(static_cast<std::uint8_t>(n>>(i*8)));
}
inline std::uint64_t get64(const std::vector<std::uint8_t>& in,std::size_t& pos){
  if(pos>in.size()||in.size()-pos<8)throw std::invalid_argument("truncated RVPC header");
  std::uint64_t n=0;for(unsigned i=0;i<8;++i)n|=std::uint64_t(in[pos++])<<(i*8);return n;
}
inline std::vector<std::uint8_t> save(const Runtime& runtime){
  const auto core=runtime.core_snapshot(),mem=runtime.memory_persistent_snapshot();
  if(core.size()>max_state||mem.size()>max_state||core.size()>max_state-mem.size())
    throw std::length_error("RVPC2 size exceeds 32 MiB");
  std::vector<std::uint8_t> out;out.reserve(29+core.size()+mem.size());
  for(char c:std::string("RVPC2"))out.push_back(static_cast<std::uint8_t>(c));
  put64(out,core.size());put64(out,mem.size());put64(out,fnv(core,mem));
  out.insert(out.end(),core.begin(),core.end());
  out.insert(out.end(),mem.begin(),mem.end());
  return out;
}
inline Runtime restore(const std::vector<std::uint8_t>& in){
  if(in.empty())return Runtime{};
  if(in.size()<29)throw std::invalid_argument("RVPC state truncated");
  const std::string magic(reinterpret_cast<const char*>(in.data()),5);
  if(magic!="RVPC1"&&magic!="RVPC2")throw std::invalid_argument("Invalid RVPC state format");
  std::size_t pos=5;auto nc=get64(in,pos),nm=get64(in,pos),hash=get64(in,pos);
  if(nc>max_state||nm>max_state||nc>max_state-nm||nc>in.size()-pos||
     nm>in.size()-pos-nc||nc+nm!=in.size()-pos)
    throw std::invalid_argument("Invalid RVPC state dimensions");
  const std::string core(reinterpret_cast<const char*>(in.data()+pos),std::size_t(nc));
  pos+=std::size_t(nc);
  const std::string mem(reinterpret_cast<const char*>(in.data()+pos),std::size_t(nm));
  if(fnv(core,mem)!=hash)throw std::invalid_argument("RVPC checksum mismatch");
  Runtime result;result.restore(core,mem);
  if(result.core_snapshot()!=core||
    (magic=="RVPC2"?result.memory_persistent_snapshot():result.memory_snapshot())!=mem)
    throw std::invalid_argument("RVPC restore altered the original snapshot");
  return result;
}
inline void put32(std::vector<std::uint8_t>& out,std::uint32_t n){
  for(unsigned i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(n>>(i*8)));
}
inline std::vector<std::uint8_t> packet(const std::string& text,const std::vector<std::uint8_t>& state){
  if(text.size()>UINT32_MAX||state.size()>UINT32_MAX)throw std::length_error("JNI output too large");
  std::vector<std::uint8_t> out;out.reserve(13+text.size()+state.size());
  for(char c:std::string("RXOS1"))out.push_back(static_cast<std::uint8_t>(c));
  put32(out,std::uint32_t(text.size()));put32(out,std::uint32_t(state.size()));
  out.insert(out.end(),text.begin(),text.end());out.insert(out.end(),state.begin(),state.end());
  return out;
}
}
