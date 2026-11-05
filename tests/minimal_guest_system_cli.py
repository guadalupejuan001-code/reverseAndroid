#!/usr/bin/env python3
"""Persistent system execution and optional independent QEMU-system oracle."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

binary=Path(sys.argv[1]).resolve()
elf=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else None
checks=0
def check(condition,message):
    global checks
    checks+=1
    if not condition:raise AssertionError(message)
def run(*arguments,ok=True):
    result=subprocess.run([str(binary),*map(str,arguments)],capture_output=True,text=True,timeout=60)
    check((result.returncode==0)==ok,result.stderr)
    return json.loads(result.stdout) if ok else result

with tempfile.TemporaryDirectory() as directory:
    root=Path(directory);initial=root/'initial.rvg';whole=root/'whole.rvg';part=root/'part.rvg';final=root/'final.rvg'
    run('system-init',elf,initial) if elf else run('system-demo-init',initial)
    all=run('run',initial,whole,10000)
    check(all['stdout']=='REVERSE: EL1 MMU IRQ EL0 SVC FAULT OK\n','System witness output differs')
    check(all['status']=='exited' and all['guest']['exceptions']==4 and all['guest']['mmu_enabled'],'System architecture transitions failed')
    first=run('run',initial,part,30)
    last=run('run',part,final,10000)
    check(first['stdout']+last['stdout']==all['stdout'] and whole.read_bytes()==final.read_bytes(),'Persistent IRQ/MMU state differs from whole run')
    check(run('peek',final,0x40009000,16)['bytes']==[0]*8+[42]+[0]*7,'Denied/allowed physical stores differ')
    source=root/'change.xi';source.write_text('mem_store 1073778696 43\n')
    changed=root/'changed.rvg';run('xi',final,source,changed)
    check(run('peek',changed,0x40009008)['bytes']==[43],'XI could not change the actual ARM64 physical byte')
    tree=root/'board.dtb';run('device-tree',tree)
    check(tree.read_bytes()[:4]==b'\xd0\x0d\xfe\xed','FDT magic differs from specification')
    if elf:
        trace=elf.parent/'qemu-system-trace.txt'
        try:
            reference=subprocess.run(['qemu-system-aarch64','-M','virt','-cpu','cortex-a53','-m','128M','-nographic','-monitor','none','-kernel',str(elf),'-no-reboot','-d','in_asm,int,guest_errors','-D',str(trace)],capture_output=True,timeout=20)
        except subprocess.TimeoutExpired as error:
            if trace.exists() and trace.stat().st_size>1024*1024:
                with trace.open('rb') as stream:
                    start=stream.read(512*1024);stream.seek(-512*1024,2);end=stream.read()
                trace.write_bytes(start+b'\n[repeated middle trace omitted]\n'+end)
            print('Independent system oracle timed out; trace:',trace,file=sys.stderr)
            print('Oracle partial output:',(error.stdout or b'')[:4096],file=sys.stderr)
            raise
        check(reference.returncode==0 and reference.stdout.decode()==all['stdout'],'Independent QEMU system execution differs: '+reference.stderr.decode())
        oracle='clang/lld ELF + qemu-system-aarch64'
    else:oracle='embedded independently assembled bare-metal witness'
print(json.dumps({'checks':checks,'oracle':oracle,'android_boot':'not_implemented'}))
