#!/usr/bin/env python3
"""Apply the exact EL0 cache-operation correction to a separate CLZ build."""
import hashlib,json,pathlib,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[1]
out=pathlib.Path(sys.argv[1]).resolve()
subprocess.run([sys.executable,str(root/'scripts/prepare-clz-extension.py'),str(out)],check=True)
decoder=out/'minimal_guest.hpp';before=decoder.read_bytes()
anchor=b'  else if ((instruction&0xfff80000)==0xd5080000) { // selected SYS operations\n'
assert before.count(anchor)==1
after=before.replace(anchor,b'#include "reverseandroid_el0_ic_ivau.inc"\n'+anchor)
decoder.write_bytes(after)
extension=(root/'extensions/aarch64-el0-ic-ivau.inc').read_bytes()
(out/'reverseandroid_el0_ic_ivau.inc').write_bytes(extension)
h=lambda b:hashlib.sha256(b).hexdigest()
prior=json.loads((out/'extension-manifest.json').read_text())
manifest={'schema':'reverseandroid-explicit-IC-IVAU-correction-v1',
 'original_source_commit':prior['original_source_commit'],'all_870_imported_files_unchanged':True,
 'original_engine_binary_unchanged_claimed':False,'retained_CLZ_extension':prior,
 'extension':'Exact IC IVAU, Xt at EL0 with SCTLR_EL1.UCI control',
 'observed_SIGILL_opcode':'0xd50b7529','observed_fault_address':'0x7544ce7e64',
 'baseline_decoder_SHA256':h(before),'extended_decoder_SHA256':h(after),'extension_SHA256':h(extension),
 'changed_original_build_copy_files':['minimal_guest.hpp','minimal_guest_simd.inc'],
 'snapshot_codecs_RAM_disk_clock_seed_cadence_IRQ_and_security_gates_unchanged':True}
for source in (root/'src').rglob('*'):
 if source.is_file() and source.name not in manifest['changed_original_build_copy_files']:
  assert (out/source.relative_to(root/'src')).read_bytes()==source.read_bytes()
(out/'extension-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest))
