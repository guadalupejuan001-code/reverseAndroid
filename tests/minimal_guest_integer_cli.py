#!/usr/bin/env python3
"""Architectural outcomes and independent compiler/QEMU integer ISA oracle."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

binary,elf=map(lambda p:Path(p).resolve(),sys.argv[1:3])
mask=(1<<64)-1
expected=[0x00ff00ff00ff00ff,0x80000001,-4,1,-1,-9,1,7,0x40000abc,-128,0xffffffffff00ffff,6,-2,-128,-2147483648,51,60,0x80000000,0x3412000000000000,-16,0x200000001,-1,0xfffffffffffffffe]
expected += [128,-2147483648,128,-2147483648,8,128,-2147483648,0]
with tempfile.TemporaryDirectory() as directory:
    initial=Path(directory)/'initial.rvg';final=Path(directory)/'final.rvg'
    subprocess.run([str(binary),'init',str(elf),str(initial)],check=True,capture_output=True)
    result=subprocess.run([str(binary),'run',str(initial),str(final),'10000'],check=True,capture_output=True,text=True)
    actual=json.loads(result.stdout)
    raw=actual['stdout'].encode('latin1')
    values=list(struct.unpack('<%dQ'%len(expected),raw))
    assert values==[value&mask for value in expected],(values,expected)
    reference=subprocess.run(['qemu-aarch64',str(elf)],capture_output=True,check=True,timeout=15)
    assert reference.stdout==raw and actual['guest']['exit_code']==0
    print(json.dumps({'architectural_results':len(values),'oracle':'clang/lld + qemu-aarch64','instructions':actual['guest']['instructions']}))
