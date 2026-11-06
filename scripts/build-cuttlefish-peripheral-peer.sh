#!/usr/bin/env bash
set -euo pipefail
task_root=$(cd "$(dirname "$0")/.." && pwd)
task_build=${1:-"$task_root/build/cuttlefish-peripheral-peer"}
mkdir -p "$task_build"
cd "$task_root"
cc -std=c11 -O2 -Wall -Wextra -Werror tests/cuttlefish-peripheral-protocol-test.c -o "$task_build/protocol-test"
"$task_build/protocol-test" > "$task_build/protocol-test.json"
cc -std=c11 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation -DRA_HOST_TEST native/cuttlefish-peripheral-peer.c -o "$task_build/host-peer"
python3 tests/cuttlefish-peripheral-pty-test.py "$task_build/host-peer" > "$task_build/pty-test.json"
if [[ ${RA_BUILD_AARCH64:-1} == 1 ]]; then
  clang --target=aarch64-linux-gnu -std=c11 -Os -Wall -Wextra -Werror -Wno-misleading-indentation \
    -ffreestanding -fno-builtin -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables \
    -fno-pie -fdata-sections -ffunction-sections -nostdlib -static -fuse-ld=lld \
    -Wl,--gc-sections,--build-id=none,-z,max-page-size=4096,-e,_start \
    native/cuttlefish-peripheral-peer.c -o "$task_build/cuttlefish-peripheral-peer-aarch64"
  python3 - "$task_build" <<'PY'
import gzip,hashlib,json,pathlib,struct,sys
p=pathlib.Path(sys.argv[1]); f=p/'cuttlefish-peripheral-peer-aarch64'; b=f.read_bytes()
assert b[:7]==b'\x7fELF\x02\x01\x01' and struct.unpack_from('<H',b,18)[0]==183
assert struct.unpack_from('<H',b,16)[0]==2
phoff=struct.unpack_from('<Q',b,32)[0]; phsize,phnum=struct.unpack_from('<HH',b,54)
assert not any(struct.unpack_from('<I',b,phoff+i*phsize)[0] in (2,3) for i in range(phnum)), 'No dynamic loader or dynamic sections permitted'
c=gzip.compress(b,compresslevel=9,mtime=0);(p/'peer-aarch64.gz').write_bytes(c)
inputs=['native/cuttlefish-peripheral-protocol.h','native/cuttlefish-peripheral-peer.c','tests/cuttlefish-peripheral-protocol-test.c','tests/cuttlefish-peripheral-pty-test.py','scripts/build-cuttlefish-peripheral-peer.sh']
manifest={'schema':'reverseandroid-peripheral-peer-build-v1','source_sha256':{x:hashlib.sha256(pathlib.Path(x).read_bytes()).hexdigest() for x in inputs},'ELF_SHA256':hashlib.sha256(b).hexdigest(),'ELF_bytes':len(b),'gzip_SHA256':hashlib.sha256(c).hexdigest(),'gzip_bytes':len(c),'static_AArch64_ELF_verified':True,'original_CPU_and_codecs_modified':False,'Android_HAL_integration_claimed':False,'host_tests':['protocol-test.json','pty-test.json']}
(p/'build-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest))
PY
fi
