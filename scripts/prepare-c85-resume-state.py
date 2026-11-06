#!/usr/bin/env python3
"""Restore the exact ongoing C84 ledger for a validated native CPU correction.

Only ordinary controller metadata is written. Waiting phases stay waiting, and
every old input and failure remains in the authenticated accumulated console.
"""
import copy
import hashlib
import json
import pathlib
import sys


def read(path):return json.loads(pathlib.Path(path).read_text())
def digest(path):
    with pathlib.Path(path).open("rb") as f:return hashlib.file_digest(f,"sha256").hexdigest()


def prepare(source, base_console, metadata_parent, reports, plan):
    source,base_console,metadata_parent,reports=map(pathlib.Path,(source,base_console,metadata_parent,reports))
    source=source.resolve()
    assert not (reports/"control-state.json").exists() and not (reports/"console-all.bin").exists()
    contract=read(source/"output-contract.json");gate=read(source/"c46-progress.json")
    host=read(source/"process-outcome.json");kernel=read(source/"kernel-observation.json")
    original=(source/plan["source_proof_file"]).read_bytes();state=json.loads(original)
    assert hashlib.sha256(original).hexdigest()==plan["source_control_proof_SHA256"]
    assert contract["checkpoint_sha256"]==gate["checkpoint_sha256"]==state["last_admitted_sha256"]==plan["source_checkpoint_sha256"]
    assert contract["instructions"]==gate["instructions"]==state["last_admitted_instructions"]==plan["source_instructions"]
    assert digest(source/"current.rvg")==contract["checkpoint_sha256"]
    assert gate["source_boundary_verified"] and kernel["live_kernel_security"]["selinux_enforcing"]
    assert host["status"]=="process_completed" and host["exit_code"]==0 and not host["forced_kill"]
    assert read(source/"host-policy.json")["native_binary_sha256"]==plan["source_native_SHA256"]
    assert state["corrective_campaign"]=="C84" and state["prior_C83_campaign_failed"]
    assert state["prior_failed_control_proof_preserved"] and state["receiver_and_base64_data_not_retransferred"]
    assert state["original_admission_and_codecs_unchanged"] and not state["failed"] and not state["complete"]
    assert state["events"]==plan["inherited_source_events"]
    assert state["events"][:4]==plan["inherited_events"]
    assert state["phase"]==plan["inherited_source_phase"] and state["waiting"] is plan["inherited_source_waiting"]
    assert len({e["phase"] for e in state["events"]})==len(state["events"])
    raw=base_console.read_bytes()
    assert hashlib.sha256(raw).hexdigest()==plan["source_base_C83_console_SHA256"]
    assert [x["stage"] for x in plan["source_console_stages"]]==list(range(1,plan["source_stage"]+1))
    for item in plan["source_console_stages"]:
        b=metadata_parent/item["artifact_alias"] / "evidence" / f"c84-{item['stage']}"
        proof=(b/"c84-proof.json").read_bytes();part=(b/"console-new.txt").read_bytes()
        assert hashlib.sha256(proof).hexdigest()==item["control_proof_SHA256"]
        assert hashlib.sha256(part).hexdigest()==item["new_console_SHA256"]
        assert (b/"source-commit.txt").read_text().strip()==plan["source_commit"]
        assert read(b/"c46-progress.json")["source_boundary_verified"]
        raw+=part
        assert hashlib.sha256(raw).hexdigest()==json.loads(proof)["combined_original_console_bytes_sha256"]==item["combined_console_SHA256"]
    assert len(raw)==plan["source_combined_console_bytes"]
    assert hashlib.sha256(raw).hexdigest()==state["combined_original_console_bytes_sha256"]==plan["source_combined_console_SHA256"]
    # No old phase, failure, event or waiting flag is reset by this engine fix.
    resumed=copy.deepcopy(state);resumed.update(source=str(source),C85_resume_from_C84=True,
        C85_source_control_proof_SHA256=plan["source_control_proof_SHA256"],
        pending_C85_new_native_admission=True,next_native_binary_SHA256=plan["expected_extended_native_SHA256"],
        C84_is_reference_after_explicit_C85_native_correction=True)
    assert resumed["events"]==state["events"] and resumed["phase"]==state["phase"] and resumed["waiting"] is state["waiting"]
    reports.mkdir(parents=True,exist_ok=True)
    for name,data in [("source-c84-proof.json",original),("console-all.bin",raw),
        ("control-state.json",(json.dumps(resumed,indent=2)+"\n").encode())]:
        with (reports/name).open("xb") as f:f.write(data)
    result={"schema":"reverseandroid-C85-exact-ongoing-campaign-resume-v1",
        "source_checkpoint_sha256":contract["checkpoint_sha256"],"instructions":contract["instructions"],
        "original_proof_bytes_preserved":True,"phase":state["phase"],"waiting":state["waiting"],
        "old_events_preserved_without_replay":len(state["events"]),"combined_console_SHA256":hashlib.sha256(raw).hexdigest(),
        "new_UART_input_bytes":0,"snapshot_bytes_unchanged":True,"new_native_not_yet_executed":True}
    (reports/"resume-proof.json").write_text(json.dumps(result,indent=2)+"\n")
    return result


if __name__=="__main__":
    assert len(sys.argv)==6,"Expected SOURCE BASE_C83_CONSOLE METADATA_PARENT REPORTS PLAN"
    print(json.dumps(prepare(*sys.argv[1:5],read(sys.argv[5]))))
