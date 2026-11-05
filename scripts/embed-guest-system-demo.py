#!/usr/bin/env python3
"""Embed .text assembled from examples/guest-system-aarch64.S; CI checks it.

Usage: clang --target=aarch64-none-elf -c examples/guest-system-aarch64.S -o demo.o
       python3 scripts/embed-guest-system-demo.py demo.o [--check]
"""
from pathlib import Path
import itertools
import struct
import sys

fp='--fp' in sys.argv
source=Path(sys.argv[1]).read_bytes()
assert source[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<H',source,18)[0]==183
offset=struct.unpack_from('<Q',source,40)[0]
width,count,string_index=struct.unpack_from('<HHH',source,58)
sections=[struct.unpack_from('<IIQQQQIIQQ',source,offset+n*width) for n in range(count)]
strings=source[sections[string_index][4]:sections[string_index][4]+sections[string_index][5]]
code=None
for section in sections:
    name=strings[section[0]:].split(b'\0')[0]
    assert section[1] not in [4,9] or not section[5], 'Unresolved relocation in witness'
    if name==b'.text':code=source[section[4]:section[4]+section[5]]
assert code
words=[word[0] for word in struct.iter_unpack('<I',code[:len(code)//4*4])]
runs=[(len(list(group)),value) for value,group in itertools.groupby(words)]
lines=['    '+','.join('{%d,0x%08x}'%(count,value) for count,value in runs[i:i+4])+',' for i in range(0,len(runs),4)]
tail=','.join(str(value) for value in code[len(code)//4*4:])
header='''#pragma once
// Generated from independently assembled examples/guest-system-aarch64.S.
// Regenerate/check with scripts/embed-guest-system-demo.py; no emulator in product.
#include "minimal_guest.hpp"
namespace reverse_pc::guest {
inline Bytes system_demo_elf() {
  const std::pair<unsigned,std::uint32_t> runs[]{
'''+ '\n'.join(lines)+'''
  };
  Bytes code;for(const auto& run:runs)for(unsigned i=0;i<run.first;++i)put(code,run.second,4);
  const Bytes tail{'''+tail+'''};code.insert(code.end(),tail.begin(),tail.end());
  Bytes bytes{0x7f,'E','L','F',2,1,1,0,0,0,0,0,0,0,0,0};
  put(bytes,2,2);put(bytes,183,2);put(bytes,1,4);put(bytes,0x40080000);put(bytes,64);put(bytes,0);
  put(bytes,0,4);put(bytes,64,2);put(bytes,56,2);put(bytes,1,2);put(bytes,0,2);put(bytes,0,2);put(bytes,0,2);
  put(bytes,1,4);put(bytes,5,4);put(bytes,0x100);put(bytes,0x40080000);put(bytes,0x40080000);
  put(bytes,code.size());put(bytes,code.size());put(bytes,0x100);
  bytes.resize(0x100,0);bytes.insert(bytes.end(),code.begin(),code.end());return bytes;
}
} // namespace reverse_pc::guest
'''
if fp:
    header=header.replace('guest-system-aarch64.S','guest-fp-demo.S').replace('system_demo_elf','fp_demo_elf').replace('0x40080000','0x1000')
target=Path(__file__).resolve().parents[1]/('src/minimal_guest_fp_demo.hpp' if fp else 'src/minimal_guest_system_demo.hpp')
if '--check' in sys.argv:
    assert target.read_text()==header,'Embedded system witness differs from independently assembled source'
else:target.write_text(header)
print('System witness: %d bytes, %d instruction runs; embedded source verified'%(len(code),len(runs)))
