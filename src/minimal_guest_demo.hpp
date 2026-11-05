#pragma once
#include "minimal_guest.hpp"
namespace reverse_pc::guest {
// Small, self-authored static AArch64 ELF. The matching assembly is compiled
// independently with clang/lld in CI; neither fixture contains Android/Riot code.
inline Bytes demo_elf(std::uint64_t logical_ram=UINT64_C(1)<<30) {
  const std::string message="REVERSE ARM64: 6*7 = 42\n";
  const std::array<std::uint32_t,13> instructions{
    0xd28000c0,0xd28000e1,0x9b017c02,0xd2900003,0xf9000062,
    0xd2800020,0x100000e1,std::uint32_t(0xd2800002|(message.size()<<5)),
    0xd2800808,0xd4000001,0xd2800000,0xd2800ba8,0xd4000001};
  Bytes code; for (auto instruction:instructions) put(code,instruction,4);
  code.insert(code.end(),message.begin(),message.end());
  Bytes bytes{0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
  put(bytes,2,2);put(bytes,183,2);put(bytes,1,4);put(bytes,0x1000);put(bytes,64);put(bytes,0);
  put(bytes,0,4);put(bytes,64,2);put(bytes,56,2);put(bytes,1,2);put(bytes,0,2);put(bytes,0,2);put(bytes,0,2);
  put(bytes,1,4);put(bytes,7,4);put(bytes,0x100);put(bytes,0x1000);put(bytes,0x1000);
  put(bytes,code.size());put(bytes,logical_ram-0x1000);put(bytes,0x100);
  bytes.resize(0x100,0);bytes.insert(bytes.end(),code.begin(),code.end());return bytes;
}
} // namespace reverse_pc::guest
