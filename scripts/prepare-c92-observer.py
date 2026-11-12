#!/usr/bin/env python3
"""Derive and authenticate C92 host-only observer and read-only thread probe.

Original imported tools, RVG/MXIM codecs, 2 GiB guest RAM, native binary, disk,
input events and checkpoint bytes remain unchanged. Fail closed if the expected
baseline is missing or a third derived tool differs.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import sys

NEEDLE="check(!seen.has(node.toString())&&tasks.length<512,'task-list cycle or limit');seen.add(node.toString());"
REPLACEMENT=("check(!seen.has(node.toString()),'task-list cycle');"
             "check(tasks.length<4096,'task-list exceeds 4096 task bound');seen.add(node.toString());")

PROBE_NEEDLE=r"const blob=createHash('sha1').update('blob '+Buffer.byteLength(source)+'\0').update(source).digest('hex');assert.equal(blob,pin.git_blob_sha1);"
PROBE_REPLACEMENT=r"""const blob=createHash('sha1').update('blob '+Buffer.byteLength(source)+'\0').update(source).digest('hex');
   const c92=JSON.parse(readFileSync(new URL('../c92-tasklist-observer-manifest.json',import.meta.url)));
   assert.equal(c92.schema,'reverseandroid-c92-tasklist-observer-v1');
   assert.equal(c92.original_observer_git_blob_sha1,pin.git_blob_sha1);
   assert.equal(c92.derived_observer_git_blob_sha1,blob);
   assert.equal(c92.derived_observer_SHA256,createHash('sha256').update(source).digest('hex'));
   assert.equal(c92.task_limit,4096);
   assert.equal(c92.guest_CPU_RAM_disk_ISA_changed,false);
   assert.equal(c92.original_870_files_unchanged,true);"""
RESULT_NEEDLE="observer_source_git_blob_sha1:blob,original_observer_file_unchanged:true"
RESULT_REPLACEMENT="observer_source_git_blob_sha1:blob,original_observer_file_unchanged:false,derived_observer_verified:true"

def sha(data:bytes)->str:
    return hashlib.sha256(data).hexdigest()

def git_blob(data:bytes)->str:
    return hashlib.sha1(b"blob "+str(len(data)).encode()+b"\x00"+data).hexdigest()

def patch(source:str)->str:
    assert source.count(NEEDLE)==1, "Unexpected original observer; refusing patch"
    assert "task-list exceeds 4096" not in source
    return source.replace(NEEDLE, REPLACEMENT)

def patch_probe(source:str)->str:
    assert source.count(PROBE_NEEDLE)==1, "Unexpected read-only thread probe"
    assert source.count(RESULT_NEEDLE)==1, "Missing original observer assertion"
    return source.replace(PROBE_NEEDLE, PROBE_REPLACEMENT).replace(RESULT_NEEDLE, RESULT_REPLACEMENT)

def main():
    root=Path(sys.argv[1]).resolve()
    repo=Path(__file__).resolve().parents[1]
    manifest_file=root/"host-observer-manifest.json"
    base=json.loads(manifest_file.read_text())
    assert base["schema"]=="reverseandroid-explicit-host-memory-line-reader-v1"
    assert base["changed_tool_files"]==["rvg_memory_lines.mjs"] and base["original_870_files_unchanged"] is True

    source_path=repo/"tools/android_kernel_observer.mjs"
    baseline=source_path.read_bytes()
    upstream=json.loads((repo/"state/upstream-snapshot.json").read_text())
    imported=next(x for x in upstream["files"] if x["path"]=="tools/android_kernel_observer.mjs")
    assert git_blob(baseline)==imported["git_blob_sha1"], "Imported upstream observer changed"

    observer=root/"tools/android_kernel_observer.mjs"
    raw=observer.read_bytes()
    expected=base["derived_tool_files"]["android_kernel_observer.mjs"]["build_SHA256"]
    assert sha(raw)==expected==sha(baseline), "Derived baseline differs from imported source"
    changed=patch(raw.decode("utf-8")).encode("utf-8")

    probe=root/"tools/reverseandroid-thread-probe-4g.mjs"
    original_probe=(repo/"extensions/host-thread-probe/reverseandroid-thread-probe-4g.mjs").read_bytes()
    assert sha(probe.read_bytes())==sha(original_probe), "Unexpected read-only probe before patch"
    patched_probe=patch_probe(original_probe.decode("utf-8")).encode("utf-8")

    # Validate the complete derived runtime BEFORE writing either file.
    for name,entry in base["derived_tool_files"].items():
        assert sha((root/"tools"/name).read_bytes())==entry["build_SHA256"], name
    report={
      "schema":"reverseandroid-c92-tasklist-observer-v1",
      "base_host_observer_manifest_sha256":sha(manifest_file.read_bytes()),
      "original_observer_SHA256":sha(raw),
      "original_observer_git_blob_sha1":git_blob(baseline),
      "derived_observer_SHA256":sha(changed),
      "derived_observer_git_blob_sha1":git_blob(changed),
      "original_thread_probe_SHA256":sha(original_probe),
      "derived_thread_probe_SHA256":sha(patched_probe),
      "task_limit":4096,
      "cycle_check_separate":True,
      "changed_derived_tools":["android_kernel_observer.mjs","reverseandroid-thread-probe-4g.mjs"],
      "original_870_files_unchanged":True,
      "guest_CPU_RAM_disk_ISA_changed":False,
      "original_snapshot_modified":False,
      "native_binary_changed":False,
      "guest_property_injection":False,
    }
    observer.write_bytes(changed)
    probe.write_bytes(patched_probe)
    (root/"c92-tasklist-observer-manifest.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
    for name,entry in base["derived_tool_files"].items():
        actual=sha((root/"tools"/name).read_bytes())
        assert actual==(sha(changed) if name=="android_kernel_observer.mjs" else entry["build_SHA256"]),name
    assert sha(probe.read_bytes())==report["derived_thread_probe_SHA256"]
    print(json.dumps(report,sort_keys=True))
if __name__=="__main__":
    main()
