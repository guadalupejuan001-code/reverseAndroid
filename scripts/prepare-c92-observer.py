#!/usr/bin/env python3
"""Derived host observer only. Do not mutate the original guest or imported files."""
import hashlib, json, sys
from pathlib import Path
NEEDLE="check(!seen.has(node.toString())&&tasks.length<512,'task-list cycle or limit');seen.add(node.toString());"
REPLACEMENT=("check(!seen.has(node.toString()),'task-list cycle');"
             "check(tasks.length<4096,'task-list exceeds 4096 task bound');seen.add(node.toString());")
def sha(data): return hashlib.sha256(data).hexdigest()
def patch(source):
    assert source.count(NEEDLE)==1, "Unexpected original observer; refusing patch"
    assert "task-list exceeds 4096" not in source
    return source.replace(NEEDLE,REPLACEMENT)
def main():
    root=Path(sys.argv[1]).resolve();p=root/"host-observer-manifest.json"
    meta=json.loads(p.read_text());assert meta["schema"]=="reverseandroid-explicit-host-memory-line-reader-v1"
    assert meta["changed_tool_files"]==["rvg_memory_lines.mjs"] and meta["original_870_files_unchanged"]
    tool=root/"tools/android_kernel_observer.mjs"; original=tool.read_bytes()
    expected=meta["derived_tool_files"]["android_kernel_observer.mjs"]["build_SHA256"]
    assert sha(original)==expected, "Observer differs from authenticated baseline"
    changed=patch(original.decode("utf-8")).encode("utf-8");tool.write_bytes(changed)
    for name,entry in meta["derived_tool_files"].items():
        now=sha((root/"tools"/name).read_bytes())
        assert now==(sha(changed) if name=="android_kernel_observer.mjs" else entry["build_SHA256"]),name
    report={"schema":"reverseandroid-c92-tasklist-observer-v1","baseline_manifest_SHA256":sha(p.read_bytes()),
      "original_observer_SHA256":expected,"derived_observer_SHA256":sha(changed),
      "task_limit":4096,"cycle_check_separate":True,"changed_derived_tools":["android_kernel_observer.mjs"],
      "original_870_files_unchanged":True,"guest_CPU_RAM_disk_ISA_changed":False,
      "original_snapshot_modified":False,"native_binary_changed":False}
    (root/"c92-tasklist-observer-manifest.json").write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(report,sort_keys=True))
if __name__=="__main__": main()
