#pragma once
// Generated from independently assembled examples/guest-fp-demo.S.
// Regenerate/check with scripts/embed-guest-system-demo.py; no emulator in product.
#include "minimal_guest.hpp"
namespace reverse_pc::guest {
inline Bytes fp_demo_elf() {
  const std::pair<unsigned,std::uint32_t> runs[]{
    {1,0xd10083ff},{1,0x1e2e1000},{1,0x1e201001},{1,0x1e212802},
    {1,0x1e260040},{1,0x52a80801},{1,0x6b01001f},{1,0x54000241},
    {1,0x1e621000},{1,0x1e61c001},{1,0x9e660020},{1,0xd2e80001},
    {1,0xeb01001f},{1,0x54000181},{1,0x4f01e542},{1,0x4e228442},
    {1,0x3d8003e2},{1,0x3dc003e3},{1,0x0e1f3c60},{1,0x7101501f},
    {1,0x540000a1},{1,0x100001a1},{1,0xd2800362},{1,0xd2800013},
    {1,0x14000004},{1,0x700001e1},{1,0xd2800302},{1,0xd2800033},
    {1,0xd2800020},{1,0xd2800808},{1,0xd4000001},{1,0xaa1303e0},
    {1,0xd2800ba8},{1,0xd4000001},{1,0x45564552},{1,0x3a455352},
    {1,0x33504620},{1,0x50462032},{1,0x53203436},{1,0x20444d49},
    {1,0x520a4b4f},{1,0x52455645},{1,0x203a4553},{1,0x53205046},
    {1,0x20444d49},{1,0x4c494146},
  };
  Bytes code;for(const auto& run:runs)for(unsigned i=0;i<run.first;++i)put(code,run.second,4);
  const Bytes tail{69,68,10};code.insert(code.end(),tail.begin(),tail.end());
  Bytes bytes{0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
  put(bytes,2,2);put(bytes,183,2);put(bytes,1,4);put(bytes,0x1000);put(bytes,64);put(bytes,0);
  put(bytes,0,4);put(bytes,64,2);put(bytes,56,2);put(bytes,1,2);put(bytes,0,2);put(bytes,0,2);put(bytes,0,2);
  put(bytes,1,4);put(bytes,5,4);put(bytes,0x100);put(bytes,0x1000);put(bytes,0x1000);
  put(bytes,code.size());put(bytes,code.size());put(bytes,0x100);
  bytes.resize(0x100,0);bytes.insert(bytes.end(),code.begin(),code.end());return bytes;
}
} // namespace reverse_pc::guest
