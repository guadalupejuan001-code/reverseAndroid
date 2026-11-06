#!/usr/bin/env python3
"""Keep all 870 imported files immutable; extend only a separate build tree."""
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
subprocess.run([sys.executable, str(root / 'scripts/verify-reverseandroid-import.py')], check=True)
out = pathlib.Path(sys.argv[1]).resolve()
assert out != root / 'src' and not out.exists(), 'Extension output must be a new build directory'
shutil.copytree(root / 'src', out)
decoder = out / 'minimal_guest_simd.inc'
before = decoder.read_bytes()
anchor = b'  const auto reverse=i&0xbf3ffc00u;\n'
assert before.count(anchor) == 1
after = before.replace(anchor, b'#include "reverseandroid_clz.inc"\n' + anchor)
decoder.write_bytes(after)
extension = (root / 'extensions/aarch64-clz.inc').read_bytes()
(out / 'reverseandroid_clz.inc').write_bytes(extension)
h = lambda b: hashlib.sha256(b).hexdigest()
manifest = {
    'schema': 'reverseandroid-explicit-ISA-extension-v1',
    'original_source_commit': '37f5b4610c5c31981e359fcf8fd0b303c14f01fd',
    'all_870_imported_files_unchanged': True,
    'original_engine_binary_unchanged_claimed': False,
    'extension': 'AArch64 AdvSIMD CLZ (vector), all six allocated forms',
    'observed_blocking_opcode': '0x6ea04800',
    'baseline_decoder_SHA256': h(before),
    'extended_decoder_SHA256': h(after),
    'extension_SHA256': h(extension),
    'changed_original_build_copy_files': ['minimal_guest_simd.inc'],
    'snapshot_codecs_RAM_disk_clock_seed_and_cadence_unchanged': True,
}
for source in (root / 'src').rglob('*'):
    if source.is_file() and source.name != 'minimal_guest_simd.inc':
        assert (out / source.relative_to(root / 'src')).read_bytes() == source.read_bytes()
(out / 'extension-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps(manifest))
