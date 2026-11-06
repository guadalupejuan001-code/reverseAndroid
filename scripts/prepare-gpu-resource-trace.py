#!/usr/bin/env python3
"""Instrument a separate CLZ/IC build; retain every original guest transition."""
import hashlib
import json
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
out = pathlib.Path(sys.argv[1]).resolve()
subprocess.run([sys.executable, str(root/'scripts/prepare-ic-ivau-extension.py'), str(out)], check=True)
h = lambda data: hashlib.sha256(data).hexdigest()
prior = json.loads((out/'extension-manifest.json').read_text())

def patch(path, replacements):
    before = path.read_text()
    revised = before
    for old, new in replacements:
        if revised.count(old) != 1:
            raise RuntimeError('Ambiguous GPU trace anchor: '+str(path))
        revised = revised.replace(old, new)
    path.write_text(revised)
    return {'before_sha256': h(before.encode()), 'after_sha256': h(revised.encode())}

decoder = patch(out/'minimal_guest.hpp', [
    ('#include <vector>\n', '#include <vector>\n#include "reverseandroid_gpu_trace.hpp"\n'),
])
device = patch(out/'minimal_guest_pci.inc', [
    ('      if(!next){const auto bar=v.bar,command=v.command;',
     '      if(!next){if(d)gpu_resource_trace("reset",registers_.instructions,0,0,Bytes{},p);const auto bar=v.bar,command=v.command;'),
    ('      require(!(flags&~1u)&&get(input,16,4)==0,"Unsupported GPU context/fence flags");\n',
     '      require(!(flags&~1u)&&get(input,16,4)==0,"Unsupported GPU context/fence flags");\n'
     '      gpu_resource_trace("request",registers_.instructions,queue,type,input,p);\n'),
    ('      if(!response.empty()){out(0,result,4);',
     '      gpu_resource_trace("response",registers_.instructions,queue,type,input,p,result);\n'
     '      if(!response.empty()){out(0,result,4);'),
])
header = (root/'extensions/gpu-resource-trace.hpp').read_bytes()
(out/'reverseandroid_gpu_trace.hpp').write_bytes(header)
native = (root/'tools/guest_reverseandroid_epoch.cpp').read_text()
anchor = 'Bytes().swap(input);\n'
assert native.count(anchor) == 1
native = native.replace(anchor, anchor+'  gpu_resource_trace("source",guest.registers().instructions,0,0,Bytes{},guest.machine().pci);\n')
(out/'guest_reverseandroid_gpu_trace.cpp').write_text(native)
manifest = {
    'schema': 'reverseandroid-host-only-gpu-resource-trace-v1',
    'retained_ISA_extension': prior,
    'all_870_imported_files_unchanged': True,
    'trace_header_sha256': h(header),
    'build_copy_decoder': decoder,
    'build_copy_device': device,
    'native_source_sha256': h(native.encode()),
    'device_semantics_or_resource_ids_changed': False,
    'original_RVG_codecs_changed': False,
    'guest_RAM_disk_clock_seed_IRQ_or_input_changed': False,
    'events_provisional_until_original_admission': True,
    'trace_capacity': 16384,
}
(out/'gpu-trace-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
print(json.dumps(manifest))
