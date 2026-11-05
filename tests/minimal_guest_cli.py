#!/usr/bin/env python3
"""End-to-end guest checkpoints and independent clang/QEMU execution oracle."""
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

binary=Path(sys.argv[1]).resolve()
elf=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else None
checks=0
def check(condition,message):
    global checks
    checks+=1
    if not condition: raise AssertionError(message)
def run(*arguments,ok=True):
    result=subprocess.run([str(binary),*map(str,arguments)],capture_output=True,text=True,timeout=60)
    check((result.returncode==0)==ok,result.stderr)
    return json.loads(result.stdout) if ok else result

with tempfile.TemporaryDirectory() as temporary:
    root=Path(temporary)
    initial=root/'initial.rvg'
    if elf:
        run('init',elf,initial)
    else:
        run('demo-init',initial)
    complete=root/'complete.rvg';sliced=root/'sliced.rvg';resumed=root/'resumed.rvg'
    full=run('run',initial,complete,10000)
    part=run('run',initial,sliced,4)
    remaining=run('run',sliced,resumed,10000)
    check(full['status']=='exited' and full['guest']['exit_code']==0,'Guest did not exit')
    check(full['stdout']=='REVERSE ARM64: 6*7 = 42\n','Guest stdout differs')
    check(part['status']=='yielded' and part['guest']['instructions']==4,'Instruction slice not persisted')
    check(full['stdout']==part['stdout']+remaining['stdout'],'Checkpoint repeated/lost output')
    check(complete.read_bytes()==resumed.read_bytes(),'Sliced run state differs from whole run')
    check(full['guest']['delta_overrides']==1,'Redundant guest bytes were stored')
    address=0x8000
    if elf:
        symbols=subprocess.check_output(['readelf','--symbols','--wide',str(elf)],text=True)
        match=next(line.split() for line in symbols.splitlines() if re.search(r'\bguest_result$',line))
        address=int(match[1],16)
    check(run('peek',complete,address,8)['bytes']==[42,0,0,0,0,0,0,0],'Result bytes are incorrect')
    corrupt=root/'bad.rvg';data=bytearray(complete.read_bytes());data[-1]^=1;corrupt.write_bytes(data)
    destination=root/'preserved.rvg';destination.write_bytes(b'preserve-existing-destination')
    run('run',corrupt,destination,100,ok=False)
    check(destination.read_bytes()==b'preserve-existing-destination','Rejected state overwrote destination')
    if elf and shutil.which('qemu-aarch64'):
        reference=subprocess.run(['qemu-aarch64',str(elf)],capture_output=True,timeout=15)
        check(reference.returncode==full['guest']['exit_code'],'Independent emulator exit differs')
        check(reference.stdout==full['stdout'].encode('latin1'),'Independent emulator output differs')
        oracle='clang-assembled-ELF/qemu-aarch64'
    elif elf:
        raise RuntimeError('qemu-aarch64 is required for the independent ELF oracle')
    else:
        oracle='self-authored-ELF (independent clang/QEMU oracle runs in CI)'
print(json.dumps({'checks':checks,'oracle':oracle,'checkpoint_equivalence':True,'android_boot':'not_implemented'}))
