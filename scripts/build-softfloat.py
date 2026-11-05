#!/usr/bin/env python3
"""Reproducible portable SoftFloat build for direct-compiler tests (CMake also supported)."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess

p=argparse.ArgumentParser();p.add_argument('--cc',default='cc');p.add_argument('--ar',default='ar');p.add_argument('--out',default='build/softfloat');p.add_argument('--verify-only',action='store_true');args=p.parse_args()
root=Path(__file__).resolve().parents[1]/'third_party/softfloat'
for path,expected in json.loads((root/'UPSTREAM.json').read_text())['files'].items():
    assert hashlib.sha256((root/path).read_bytes()).hexdigest()==expected,'Unreviewed SoftFloat source '+path
if args.verify_only:
    print('Pinned SoftFloat source hashes verified')
    raise SystemExit(0)
out=Path(args.out);out.mkdir(parents=True,exist_ok=True)
sources=sorted((root/'source').glob('*.c'))+sorted((root/'source/ARM-VFPv2').glob('*.c'))
def compile(source):
    target=out/(source.stem+'.o')
    subprocess.run([args.cc,'-std=c11','-O2','-fPIC','-DSOFTFLOAT_FAST_INT64','-DSOFTFLOAT_ROUND_ODD','-DINLINE_LEVEL=5','-DTHREAD_LOCAL=_Thread_local','-I'+str(root),'-I'+str(root/'source/include'),'-I'+str(root/'source/ARM-VFPv2'),'-c',str(source),'-o',str(target)],check=True)
    return target
with ThreadPoolExecutor(max_workers=4) as pool:objects=list(pool.map(compile,sources))
library=out/'libreverse_softfloat.a';subprocess.run([args.ar,'rcs',str(library),*map(str,objects)],check=True)
print(library)
