#pragma once
// Generated from independently assembled examples/guest-system-aarch64.S.
// Regenerate/check with scripts/embed-guest-system-demo.py; no emulator in product.
#include "minimal_guest.hpp"
namespace reverse_pc::guest {
inline Bytes system_demo_elf() {
  const std::pair<unsigned,std::uint32_t> runs[]{
    {1,0xd5034fdf},{1,0xd2a82000},{1,0x9100001f},{1,0x10003fa0},
    {1,0xd518c000},{1,0xd2a12014},{1,0xd2a10015},{1,0xd2a10036},
    {1,0xd2800017},{1,0xd2800018},{1,0xd2a80020},{1,0xd2808021},
    {1,0xf9000001},{1,0xd280e0a1},{1,0xf2a80001},{1,0xf9000401},
    {1,0xd280f8a1},{1,0xf2a80001},{1,0xf9000801},{1,0xd5182000},
    {1,0xd5182020},{1,0xd29fe001},{1,0xd518a201},{1,0xd2800321},
    {1,0xf2b00321},{1,0xd5182041},{1,0xd5033f9f},{1,0xd2810021},
    {1,0xf2a61a01},{1,0xd5181001},{1,0xd5033fdf},{1,0x52800020},
    {1,0xb90002a0},{1,0xb90002c0},{1,0x52801fe0},{1,0xb90006c0},
    {1,0x52a80000},{1,0xb90102a0},{1,0xd2807d00},{1,0xd51be200},
    {1,0xd2800020},{1,0xd51be220},{1,0xd50342df},{1,0xb5000097},
    {1,0xd503207f},{1,0xd50342ff},{1,0x17fffffc},{1,0xd50342ff},
    {1,0xd2820000},{1,0xf2a82000},{1,0xd5184100},{1,0x100000c0},
    {1,0xd2a80001},{1,0x8b010000},{1,0xd5184020},{1,0xd518401f},
    {1,0xd69f03e0},{1,0xd28000e0},{1,0xd4000541},{1,0xf100a81f},
    {1,0x540000a1},{1,0xd2920001},{1,0xf2b00001},{1,0xf9000020},
    {1,0xd4000561},{1,0xd4000581},{1,0x17ffffff},{1,0xf10006ff},
    {1,0x54000241},{1,0xf100071f},{1,0x54000201},{1,0xd2920001},
    {1,0xf2a80001},{1,0xf9400020},{1,0xb5000180},{1,0xd2800540},
    {1,0xf9000420},{1,0x10007661},{1,0x38401420},{1,0x34000060},
    {1,0x39000280},{1,0x17fffffd},{1,0xd2800100},{1,0xf2b08000},
    {1,0xd4000002},{1,0x17fffffd},{1,0x528008c0},{1,0x39000280},
    {1,0x17fffffa},{1,0xd5385200},{1,0xd35afc01},{1,0xf100543f},
    {1,0x54000200},{1,0xf100903f},{1,0x54ffff01},{1,0x92401800},
    {1,0xf101341f},{1,0x54fffea1},{1,0xd5386000},{1,0xd2920001},
    {1,0xf2b00001},{1,0xeb01001f},{1,0x54fffe01},{1,0xd2800038},
    {1,0xd5384020},{1,0x91001000},{1,0xd5184020},{1,0xd69f03e0},
    {1,0x92403c00},{1,0xf100a81f},{1,0x54000061},{1,0xd2800540},
    {1,0xd69f03e0},{1,0xf100ac1f},{1,0x54fffc81},{1,0x10fffa00},
    {1,0xd5184020},{1,0xd28078a0},{1,0xd5184000},{1,0xd69f03e0},
    {1,0xb9400ec0},{1,0x7100781f},{1,0x54fffb81},{1,0xd51be23f},
    {1,0xb90012c0},{1,0xd2800037},{1,0xd69f03e0},{385,0xd503201f},
    {1,0x17fffe56},{31,0xd503201f},{1,0x17fffe58},{31,0xd503201f},
    {1,0x17fffe16},{31,0xd503201f},{1,0x17fffdf6},{31,0xd503201f},
    {1,0x17fffdd9},{31,0xd503201f},{1,0x17fffdd8},{31,0xd503201f},
    {1,0x17fffd96},{31,0xd503201f},{1,0x17fffd76},{31,0xd503201f},
    {1,0x17fffd59},{31,0xd503201f},{1,0x17fffd58},{31,0xd503201f},
    {1,0x17fffd16},{31,0xd503201f},{1,0x17fffcf6},{31,0xd503201f},
    {1,0x17fffcd6},{31,0xd503201f},{1,0x17fffcb6},{31,0xd503201f},
    {1,0x17fffc96},{31,0xd503201f},{1,0x17fffc76},{31,0xd503201f},
    {1,0x45564552},{1,0x3a455352},{1,0x314c4520},{1,0x554d4d20},
    {1,0x51524920},{1,0x304c4520},{1,0x43565320},{1,0x55414620},
    {1,0x4f20544c},
  };
  Bytes code;for(const auto& run:runs)for(unsigned i=0;i<run.first;++i)put(code,run.second,4);
  const Bytes tail{75,10,0};code.insert(code.end(),tail.begin(),tail.end());
  Bytes bytes{0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
  put(bytes,2,2);put(bytes,183,2);put(bytes,1,4);put(bytes,0x40080000);put(bytes,64);put(bytes,0);
  put(bytes,0,4);put(bytes,64,2);put(bytes,56,2);put(bytes,1,2);put(bytes,0,2);put(bytes,0,2);put(bytes,0,2);
  put(bytes,1,4);put(bytes,5,4);put(bytes,0x100);put(bytes,0x40080000);put(bytes,0x40080000);
  put(bytes,code.size());put(bytes,code.size());put(bytes,0x100);
  bytes.resize(0x100,0);bytes.insert(bytes.end(),code.begin(),code.end());return bytes;
}
} // namespace reverse_pc::guest
