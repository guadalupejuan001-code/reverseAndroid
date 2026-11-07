#!/usr/bin/env python3
"""Derive a bounded host transport build; never edit the imported source tree."""
import hashlib,json,pathlib,shutil,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[1]
out=pathlib.Path(sys.argv[1]).resolve();assert not out.exists()
src=out/'src';tools=out/'tools';cap=3840*1024**2
subprocess.run([sys.executable,str(root/'scripts/verify-reverseandroid-import.py')],cwd=root,check=True,stdout=subprocess.DEVNULL)
subprocess.run([sys.executable,str(root/'scripts/prepare-ic-ivau-extension.py'),str(src)],cwd=root,check=True,stdout=subprocess.DEVNULL)
sha=lambda b:hashlib.sha256(b).hexdigest()
changes=[]
def replace(path,old,new):
    before=path.read_bytes();assert before.count(old)==1,(path.name,old)
    after=before.replace(old,new);path.write_bytes(after)
    changes.append(dict(file=str(path.relative_to(out)),before_SHA256=sha(before),after_SHA256=sha(after),old=old.decode(),new=new.decode()))
base_manifest=json.loads((src/'extension-manifest.json').read_text())
replace(src/'minimal_guest.hpp',b'max_view_snapshot=2ull*1024*1024*1024;',b'max_view_snapshot=3840ull*1024*1024;')
entry=src/'guest_reverseandroid_epoch_host.cpp';entry.write_bytes((root/'tools/guest_reverseandroid_epoch.cpp').read_bytes())
replace(entry,b'host_snapshot_transport_bytes\\\":2147483648',b'host_snapshot_transport_bytes\\\":4026531840')
tools.mkdir()
for p in (root/'tools').iterdir():
    if p.is_file():shutil.copy2(p,tools/p.name)
for name in ['state','tests']:(out/name).symlink_to(root/name,target_is_directory=True)
replace(tools/'audit_gki_rvg.mjs',b'maxSnapshotBytes<=2*1024*1024*1024',b'maxSnapshotBytes<=4026531840')
replace(tools/'reverseandroid-epoch.mjs',b'const CAP=2147483648;',b'const CAP=4026531840;')
replace(tools/'reverseandroid-epoch.mjs',b"new URL('./android_kernel_observer.mjs',import.meta.url).pathname",b"new URL('./reverseandroid-host-kernel-observer.mjs',import.meta.url).pathname")
for name in ['reverseandroid-epoch.mjs','reverseandroid-peripheral-probe.mjs','reverseandroid-thread-probe.mjs','reverseandroid-lights-probe.mjs','reverseandroid-mmap-lock-probe.mjs']:
    path=tools/name;before=path.read_bytes();path.write_bytes(b"import './reverseandroid-host-transport-preload.mjs';\n"+before)
    if name!='reverseandroid-epoch.mjs':
        if b'statSync(path).size<=2147483648' in path.read_bytes():replace(path,b'statSync(path).size<=2147483648',b'statSync(path).size<=4026531840')
        replace(path,b'maxSnapshotBytes:2147483648',b'maxSnapshotBytes:4026531840')
replace(tools/'android_gpu_backing_evidence.mjs',b'n<=2147483648n',b'n<=4026531840n')
# The observer itself, its BTF/identity/property logic, checksum and every codec
# source are byte-identical. Only callers and the audit size policy differ.
for name in ['android_kernel_observer.mjs','android_live_property.mjs','rvg_memory_lines.mjs','rvg_checksum.mjs','rvg_checksum_wasm.mjs','android_c46_continue.mjs','android_framework_evidence.mjs']:
    assert (tools/name).read_bytes()==(root/'tools'/name).read_bytes()
modified_headers={'minimal_guest.hpp','minimal_guest_simd.inc'}
for original in (root/'src').rglob('*'):
    if original.is_file() and original.name not in modified_headers:assert (src/original.relative_to(root/'src')).read_bytes()==original.read_bytes()
manifest=dict(schema='reverseandroid-explicit-host-transport-extension-v1',host_RVG_file_capacity_bytes=cap,
    unchanged_total_unpacked_transport_bound_bytes=4*1024**3,reserved_metadata_headroom_bytes=256*1024**2,
    original_870_files_unchanged=True,retained_IC_CLZ_extension=base_manifest,
    CPU_instruction_decoder_RAM_disk_clock_seed_cadence_IRQ_RVG_RVI_MXIM_and_original_milestones_unchanged=True,
    native_epoch_original_source_SHA256=sha((root/'tools/guest_reverseandroid_epoch.cpp').read_bytes()),
    exact_capacity_replacements=changes,
    derived_tool_files={p.name:dict(source_SHA256=sha((root/'tools'/p.name).read_bytes()),build_SHA256=sha(p.read_bytes())) for p in tools.iterdir() if p.is_file()})
(out/'host-transport-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(dict(host_transport_bytes=cap,original_870_files_unchanged=True,manifest=str(out/'host-transport-manifest.json'))))
