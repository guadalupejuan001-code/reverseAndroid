#!/usr/bin/env python3
"""Reproduce both programs on the real Ether backend; test durable state."""
import json
from pathlib import Path
import subprocess
import sys

exe=Path(sys.argv[1]).resolve()
out=Path(sys.argv[2]).resolve()
root=Path(__file__).resolve().parents[1]

def launch(arguments,ok=True):
    p=subprocess.run([str(exe),*map(str,arguments)],cwd=root,
                     capture_output=True,text=True,timeout=90)
    if ok and p.returncode:
        raise AssertionError(f"execution failed: {arguments}\n{p.stdout}\n{p.stderr}")
    if not ok and not p.returncode:
        raise AssertionError(f"invalid command succeeded: {arguments}")
    return p

def row(block,kind,address):
    match=[x for x in block["observations"] if x.get("type")==kind and x.get("address")==address]
    assert len(match)==1,match
    return match[0]

def validate_report(report):
    assert report["experiment"]=="one-space-two-programs"
    assert report["upstream_commit"]=="e2778d962af22f6f1a0794058dd4685fc2573966"
    assert report["identical_initial_states"]
    assert report["transactional_rollback"]
    assert report["repeat_runs_verified"]==3
    assert report["saved_and_restored"] and report["reversed_both_programs"]
    assert report["no_dense_domain_allocation"]
    assert not report["physical_quantum_hardware"]
    assert not report["universal_programming_demonstrated"]
    before,a,b=[report[k] for k in ("initial","program_a","program_b")]
    far=report["far_address"]
    assert far.isdecimal() and len(far)==64,len(far)
    assert row(before,"memory_read","9")["value"]==5
    assert row(a,"memory_read","9")["value"]==7
    assert row(b,"memory_read","9")=={
        "type":"memory_read","address":"9","deterministic":False,
        "handle":"qbyte:9@100"}
    assert row(before,"memory_read",far)["value"]==0
    assert row(a,"probz",far)["probability_one"]==0
    assert row(b,"probz",far)["probability_one"]==.5
    assert before["active_qubits"]==b["active_qubits"]
    assert a["global_ether_rules"]==0 and b["global_ether_rules"]==1
    assert before["overrides"]==a["overrides"]==b["overrides"]==2
    assert before["quantum_bindings"]==a["quantum_bindings"]==b["quantum_bindings"]==1
    assert before["memory_fnv64"]==a["memory_fnv64"]==b["memory_fnv64"]
    assert a["core_fnv64"]!=b["core_fnv64"]

p=launch(["demo",out])
assert "REVERSE_PC_DEMO_PASS" in p.stdout,p.stdout
validate_report(json.loads((out/"report.json").read_text()))

for name,expected in [("program-a",7),("program-b",None)]:
    state=out/(name+".rvp")
    assert state.is_file() and state.stat().st_size>0
    following=out/(name+"-inspected.rvp")
    p=launch(["run",root/"examples"/"inspect.xi",state,following])
    records=[json.loads(s) for s in p.stdout.splitlines() if s.strip()]
    matched=[x for x in records if x.get("type")=="memory_read" and x.get("address")=="9"]
    assert len(matched)==1
    if expected is None:
        assert matched[0]["deterministic"] is False
    else:
        assert matched[0]["deterministic"] and matched[0]["value"]==expected
    assert state.read_bytes()==following.read_bytes(), "read-only inspection mutated state"

for branch in ("program-a","program-b"):
    s=out/(branch+".rvp")
    target=out/(branch+"-reversed.rvp")
    p=launch(["run",root/"examples"/(branch+".xi"),s,target])
    rows=[json.loads(v) for v in p.stdout.splitlines() if v.strip()]
    values=[v for v in rows if v.get("type")=="memory_read" and v.get("address")=="9"]
    assert len(values)==1 and values[0]["deterministic"] and values[0]["value"]==5

invalid=out/"invalid.xi"
invalid.write_text("mem_store 7 77\nunknown_instruction\n")
never=out/"never-saved.rvp"
launch(["run",invalid,out/"program-a.rvp",never],False)
assert not never.exists()

corrupted=out/"corrupted.rvp"
bytes_=bytearray((out/"program-a.rvp").read_bytes())
bytes_[-1]^=1
corrupted.write_bytes(bytes_)
launch(["run",root/"examples"/"inspect.xi",corrupted,never],False)
assert not never.exists()

repeat=out/"repeat"
launch(["demo",repeat])
validate_report(json.loads((repeat/"report.json").read_text()))
for file in ("initial.rvp","program-a.rvp","program-b.rvp"):
    assert (repeat/file).read_bytes()==(out/file).read_bytes(),file

print("REVERSE_PC_TEST_PASS initial_equal=1 data_transform=1 global_rule=1")
print("REVERSE_PC_TEST_PASS replay=1 persistence=1 corruption_rejection=1")
print("REVERSE_PC_TEST_PASS transactional_rollback=1 no_domain_materialization=1")
