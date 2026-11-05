#!/usr/bin/env python3
"""Exact raw FP/SIMD bits, flags and whole-vs-sliced state against an independent CPU."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

binary,elf=map(lambda p:Path(p).resolve(),sys.argv[1:3])


def run_guest(arguments, checkpoint):
    """Keep native diagnostics and the exact input state when a checked run fails."""
    result=subprocess.run([str(binary),*map(str,arguments)],capture_output=True,text=True,timeout=30)
    if result.returncode:
        evidence={'command':[str(binary),*map(str,arguments)],'returncode':result.returncode,
                  'stdout':result.stdout,'stderr':result.stderr,
                  'input_checkpoint_sha256':hashlib.sha256(checkpoint.read_bytes()).hexdigest()}
        elf.with_suffix('.failure.json').write_text(json.dumps(evidence,indent=2)+'\n')
        elf.with_suffix('.failure-input.rvg').write_bytes(checkpoint.read_bytes())
        print(json.dumps(evidence,indent=2),file=sys.stderr)
        result.check_returncode()
    return result


with tempfile.TemporaryDirectory() as directory:
    initial=Path(directory)/'initial.rvg';final=Path(directory)/'final.rvg'
    subprocess.run([str(binary),'init',str(elf),str(initial)],check=True,capture_output=True,timeout=30)
    result=run_guest(['run',initial,final,'100000'],initial)
    actual=json.loads(result.stdout)
    raw=actual['stdout'].encode('latin1')
    reference=subprocess.run(['qemu-aarch64',str(elf)],capture_output=True,check=True,timeout=15)
    elf.with_suffix('.own.bin').write_bytes(raw)
    elf.with_suffix('.qemu.bin').write_bytes(reference.stdout)
    if reference.stdout!=raw:
        offset=next((i for i,(x,y) in enumerate(zip(raw,reference.stdout)) if x!=y),min(len(raw),len(reference.stdout)))
        raise AssertionError(f'FP/SIMD differs at byte {offset}: own={raw[offset:offset+24].hex()}, QEMU={reference.stdout[offset:offset+24].hex()}; sizes={len(raw)}/{len(reference.stdout)}')
    assert actual['guest']['exit_code']==0 and actual['guest']['halted']
    assert raw and len(raw)>15000 and final.read_bytes()[3:4]==b'3'
    current=initial;output=bytearray()
    for block in range(200):
        target=Path(directory)/('sliced-%d.rvg'%(block%2))
        run=json.loads(run_guest(['run',current,target,'137'],current).stdout)
        output.extend(run['stdout'].encode('latin1'));current=target
        if run['guest']['halted']:break
    assert run['guest']['halted'] and bytes(output)==raw and current.read_bytes()==final.read_bytes(),'Checkpoint changed FP flags, vectors, memory or instruction order'
    print(json.dumps({'oracle':'clang/lld + qemu-aarch64','compared_bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest(),'instructions':actual['guest']['instructions'],'sliced_blocks':block+1,'checkpoint':'RVG3 exact'}))
