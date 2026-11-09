#!/usr/bin/env python3
"""C91F0: isolated, guarded cppreopts CPU-priority correction.

Source is the fully preserved C91 stage 6. Exactly one authenticated UART event
normalizes only root-owned cppreopts descendants that are still at nice -20.
No property, snapshot, disk image, RAM layout, ISA, HAL, GPU or QSSD edit is
performed. Stage 2 is allowed only after measured causal progress in stage 1.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT / "state/experiments/c91f0-plan.json"
REPORTS = ROOT / "evidence/c91f0-reports"
SOURCE = ROOT / "build/source-c91f0"
RUNTIME = ROOT / "build/host-observer-runtime"
CAP = 4026531840
SOURCE_SHA = "3259407725a7b8e2be5a6d874f9b59306610bc8d1cf38a81d1e6f6ae726616ea"
SOURCE_INSTRUCTIONS = 171447727998
SOURCE_SS_RUNTIME = 14949179000
SOURCE_SS_PID = 7354
NATIVE_SHA = "a31e3fd923f63071ebadeb6a78ee5aadf77111c88186e428927220f199710318"
TARGET_NAMES = {"cppreopts.sh", "preopt2cachenam", "mv", "cp", "fsync"}


def read(path):
    return json.loads(Path(path).read_text())


def save(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n")


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def validate_plan(p):
    assert p["schema"] == "reverseandroid-c91f0-cppreopts-priority-control-v1"
    assert p["cycle"] == "C91F0" and p["source_cycle"] == "C91" and p["source_stage"] == 6
    assert p["source_checkpoint_sha256"] == SOURCE_SHA
    assert p["source_instructions"] == SOURCE_INSTRUCTIONS
    assert p["source_run_id"] == 42596365209
    assert p["source_execution_commit"] == "a035f386b468c71ad11c32aee88252dbab0b91db"
    assert p["source_native_SHA256"] == NATIVE_SHA
    assert p["host_snapshot_capacity_bytes"] == CAP
    assert p["input_bytes"] <= 4096 and p["input_bytes"] > 0
    assert re.fullmatch(r"[0-9a-f]{64}", p["input_sha256"])
    assert p["new_event_phase"] == p["inherited_event_count"] == 8
    assert p["first_stage_instruction_budget"] == 500_000_000
    assert p["validation_stage_instruction_budget"] == 1_000_000_000
    assert p["host_wall_ms"] == 900000
    for key in [
        "guest_property_injection", "guest_snapshot_patching", "disk_modified_before_resume",
        "RAM_modified_before_resume", "ISA_modified", "HAL_modified", "GPU_or_QSSD_modified",
        "promotion_authority",
    ]:
        assert p[key] is False, key
    assert p["success_gates"]["minimum_changed_processes"] == 20
    assert p["success_gates"]["new_UART_events_exactly"] == 1


def parse_reply(raw):
    clean = re.sub(rb"\x1b\[[0-?]*[ -/]*[@-~]", b"", raw).replace(b"\r", b"")
    result = [int(x) for x in re.findall(rb"^REVERSE_C91F0_RESULT RC_([0-9]+)$", clean, re.M)]
    done = re.findall(rb"^REVERSE_C91F0_DONE CHANGED_([0-9]+) SEEN_([0-9]+) BAD_([0-9]+)$", clean, re.M)
    activity = re.findall(rb"^REVERSE_C91F0_ACTIVITY_REGISTERED AFTER_([0-9]+)$", clean, re.M)
    boot = re.findall(rb"^REVERSE_C91F0_BOOTPROP_([^\n]+)$", clean, re.M)
    assert len(result) <= 1 and len(done) <= 1 and len(activity) <= 1 and len(boot) <= 1
    changed = seen = bad = None
    if done:
        changed, seen, bad = map(int, done[0])
    return {
        "reply_seen": bool(result),
        "return_code": result[0] if result else None,
        "done_seen": bool(done),
        "changed": changed,
        "seen": seen,
        "bad": bad,
        "activity_registered": bool(activity),
        "activity_after_seconds": int(activity[0]) if activity else None,
        "boot_property": boot[0].decode("ascii", "strict") if boot else None,
        "guard_succeeded": result == [0] and bool(done) and bad == 0,
    }


def task_by_pid(kernel, pid):
    return next(t for t in kernel["tasks"] if t["pid"] == pid)


def source_target_counts(kernel):
    out = {}
    for name in sorted(TARGET_NAMES):
        out[name] = sum(1 for t in kernel["tasks"] if t["name"] == name)
    return out


def cfs_delta_ratios(before, after):
    b = {t["pid"]: t for t in before["tasks"]}
    rows = []
    for t in after["tasks"]:
        old = b.get(t["pid"])
        if not old or old["name"] != t["name"] or t["name"] not in {"mv", "cp", "fsync"}:
            continue
        try:
            de = int(t["accounting"]["sum_exec_runtime"]) - int(old["accounting"]["sum_exec_runtime"])
            dv = int(t["accounting"]["vruntime"]) - int(old["accounting"]["vruntime"])
        except (KeyError, ValueError):
            continue
        if de > 0 and dv >= 0:
            rows.append({"pid": t["pid"], "name": t["name"], "delta_exec": de, "delta_vruntime": dv, "ratio": dv / de})
    ratios = [r["ratio"] for r in rows]
    return rows, (statistics.median(ratios) if ratios else None)


def validate_source(p):
    lock = read(ROOT / p["source_artifact_lock"])
    transport = read(ROOT / lock["artifacts"]["seed"]["transport_manifest"])
    assert transport["source_contract"]["checkpoint_sha256"] == SOURCE_SHA
    assert transport["source_contract"]["instructions"] == SOURCE_INSTRUCTIONS
    assert transport["progress"]["source_boundary_verified"] is True
    assert transport["progress"]["system_server_alive_verified"] is True
    assert transport["progress"]["android_boot_completed_verified"] is False
    assert len(transport["parts"]) == len(lock["artifacts"]["seed"]["pieces"]) == 4

    c = read(SOURCE / "output-contract.json")
    g = read(SOURCE / "c46-progress.json")
    proof = read(SOURCE / p["source_proof_file"])
    kernel = read(SOURCE / "kernel-observation.json")
    current = read(SOURCE / "current.json")
    frame = read(SOURCE / "checkpoint-frame.json")
    assert digest(SOURCE / "current.rvg") == c["checkpoint_sha256"] == g["checkpoint_sha256"] == SOURCE_SHA
    assert c["instructions"] == g["instructions"] == kernel["instructions"] == SOURCE_INSTRUCTIONS
    assert digest(SOURCE / p["source_proof_file"]) == p["source_proof_SHA256"]
    assert digest(SOURCE / "kernel-observation.json") == p["source_kernel_SHA256"]
    assert digest(SOURCE / "checkpoint-frame.ppm") == p["source_frame_PPM_SHA256"]
    assert g["system_server_alive_verified"] is True and g["android_boot_completed_verified"] is False
    assert kernel["live_kernel_security"]["selinux_enforcing"] is True
    assert kernel["live_boot_property"]["boot_completed_property_live"] is False
    assert proof["events"] and len(proof["events"]) == p["inherited_event_count"]
    assert proof["phase"] == 8 and proof["actual_campaign_new_events"] == 0
    assert proof["current_campaign_stage"] == 6 and proof["system_server_alive"] is True
    assert proof["SELinux_enforcing"] is True and proof["boot_completed"] is False
    ss = task_by_pid(kernel, SOURCE_SS_PID)
    assert ss["name"] == "system_server" and ss["on_rq"] == 1 and ss["on_cpu"] == 0
    assert int(ss["accounting"]["sum_exec_runtime"]) == SOURCE_SS_RUNTIME
    counts = source_target_counts(kernel)
    assert counts["cppreopts.sh"] >= 100 and counts["mv"] >= 40 and counts["fsync"] >= 40
    assert "Since 'activity' could not be found" in current.get("console", "")
    assert frame["android_boot_verified"] is False and frame["android_display_verified"] is False
    return proof, kernel, counts


def prepare(p):
    validate_plan(p)
    body = ROOT / p["input_file"]
    assert body.stat().st_size == p["input_bytes"] and digest(body) == p["input_sha256"]
    subprocess.run(["sh", "-n", str(body)], check=True)
    assert digest(ROOT / "build/host-native/epoch-clz-ic-host") == NATIVE_SHA
    proof, kernel, counts = validate_source(p)
    REPORTS.mkdir(parents=True, exist_ok=True)
    state = {
        "schema": "reverseandroid-c91f0-control-state-v1",
        "source_checkpoint_sha256": SOURCE_SHA,
        "source_instructions": SOURCE_INSTRUCTIONS,
        "source_SystemServer_runtime": SOURCE_SS_RUNTIME,
        "source_target_counts": counts,
        "events": proof["events"],
        "new_event_sent": False,
        "actual_new_events": 0,
        "current_stage": 0,
        "source": str(SOURCE.relative_to(ROOT)),
        "failed": False,
        "causal_progress": False,
        "boot_completed": False,
        "activity_registered": False,
        "promotion_authority": False,
    }
    save(REPORTS / "control-state.json", state)
    save(REPORTS / "source-summary.json", {
        "checkpoint_sha256": SOURCE_SHA,
        "instructions": SOURCE_INSTRUCTIONS,
        "SystemServer": task_by_pid(kernel, SOURCE_SS_PID),
        "target_counts": counts,
        "boot_completed": False,
        "activity_service_missing_in_source_console": True,
        "CFS_nice_minus_20_evidence": read(ROOT / "docs/evidence/c91/c91f0-root-cause.json")["cppreopts"],
    })


def deep_observe(output, checkpoint):
    path = output / "kernel-deep.json"
    with path.open("x") as stream:
        subprocess.run([
            "node", "--max-old-space-size=5000",
            str(RUNTIME / "tools/reverseandroid-host-kernel-observer.mjs"),
            str(output / "current.rvg"), checkpoint, str(CAP),
            "--live-properties", "--binder-probe", "--scheduler-probe", "--zygote-probe",
        ], stdout=stream, check=True)
    return read(path)


def run_stage(p, stage):
    assert stage in (1, 2)
    st = read(REPORTS / "control-state.json")
    assert not st["failed"] and st["current_stage"] == stage - 1
    if stage == 1:
        assert not st["new_event_sent"] and st["actual_new_events"] == 0
        source = SOURCE
        input_plan = REPORTS / "stage1-input-plan.json"
        body = ROOT / p["input_file"]
        save(input_plan, {
            "schema": "reverseandroid-authenticated-console-input-plan-v1",
            "purpose": "diagnostic_console",
            "input_file": os.path.relpath(body, input_plan.parent),
            "input_bytes": p["input_bytes"],
            "input_sha256": p["input_sha256"],
            "source_checkpoint_sha256": SOURCE_SHA,
            "source_instructions": SOURCE_INSTRUCTIONS,
            "scope": "Guarded nice -20 to nice 0 normalization only for root-owned cppreopts descendants; plus bounded read-only activity/boot property watcher.",
        })
        budget = p["first_stage_instruction_budget"]
    else:
        assert st["new_event_sent"] and st["actual_new_events"] == 1 and st["causal_progress"]
        source = ROOT / "evidence/c91f0-1"
        input_plan = None
        budget = p["validation_stage_instruction_budget"]

    parent = read(source / "output-contract.json")
    before_kernel = read(source / "kernel-observation.json")
    output = ROOT / f"evidence/c91f0-{stage}"
    cmd = [
        "node", "--expose-gc", "--max-old-space-size=5000",
        str(RUNTIME / "tools/reverseandroid-epoch.mjs"), "run",
        str(source), str(output),
        str(ROOT / "build/host-native/epoch-clz-ic-host"),
        str(ROOT / "build/media/android-c27.raw"),
        str(ROOT / "build/host-native/frame"),
        os.environ["GITHUB_SHA"], str(budget),
        str(input_plan) if input_plan else "-", str(p["host_wall_ms"]),
    ]
    with (REPORTS / f"stage{stage}-run.json").open("x") as log:
        subprocess.run(cmd, stdout=log, check=True)

    gate = read(output / "c46-progress.json")
    kernel = read(output / "kernel-observation.json")
    current = read(output / "current.json")
    host = read(output / "process-outcome.json")
    policy = read(output / "host-policy.json")
    contract = read(output / "output-contract.json")
    checkpoint = digest(output / "current.rvg")
    assert checkpoint == gate["checkpoint_sha256"] == kernel["checkpoint_sha256"] == contract["checkpoint_sha256"]
    assert gate["source_boundary_verified"] is True and gate["system_server_alive_verified"] is True
    assert kernel["live_kernel_security"]["selinux_enforcing"] is True
    assert policy["native_binary_sha256"] == NATIVE_SHA
    assert host["status"] == "process_completed" and host["exit_code"] == 0 and host["forced_kill"] is False
    assert current["provenance"]["new_uart_input_bytes"] == (p["input_bytes"] if stage == 1 else 0)
    assert policy["external_input_plan_present"] is (stage == 1)

    transcript = REPORTS / "console-c91f0.bin"
    with transcript.open("ab") as stream:
        stream.write((output / "console-new.txt").read_bytes())
    reply = parse_reply(transcript.read_bytes())
    if stage == 1:
        assert current["provenance"]["external_uart_input"]["sha256"] == p["input_sha256"]
        assert reply["reply_seen"] and reply["guard_succeeded"]
        assert reply["changed"] >= p["success_gates"]["minimum_changed_processes"] and reply["bad"] == 0
        st["events"] = st["events"] + [{
            "phase": p["new_event_phase"],
            "input_plan": str(input_plan.relative_to(ROOT)),
            "actual_native_event": current["provenance"]["external_uart_input"],
        }]
        st["new_event_sent"] = True
        st["actual_new_events"] = 1
    else:
        assert "external_uart_input" not in current["provenance"]
        assert st["actual_new_events"] == 1

    ss = task_by_pid(kernel, SOURCE_SS_PID)
    ss_delta = int(ss["accounting"]["sum_exec_runtime"]) - SOURCE_SS_RUNTIME
    rows, median_ratio = cfs_delta_ratios(before_kernel if stage == 1 else read(SOURCE / "kernel-observation.json"), kernel)
    remaining = source_target_counts(kernel)
    remaining_work = remaining["mv"] + remaining["cp"] + remaining["fsync"]
    weight_normalized = (median_ratio is not None and median_ratio > 0.5 and len(rows) >= 5) or remaining_work < 20
    causal = (
        reply["guard_succeeded"]
        and ss_delta >= p["success_gates"]["SystemServer_runtime_delta_min"]
        and weight_normalized
        and gate["system_server_alive_verified"]
        and kernel["live_kernel_security"]["selinux_enforcing"]
    )
    deep = deep_observe(output, checkpoint)
    st.update({
        "source": str(output.relative_to(ROOT)),
        "current_stage": stage,
        "last_checkpoint_sha256": checkpoint,
        "last_instructions": gate["instructions"],
        "SystemServer_runtime": int(ss["accounting"]["sum_exec_runtime"]),
        "SystemServer_runtime_delta_from_source": ss_delta,
        "target_CFS_sample_count": len(rows),
        "target_CFS_median_delta_vruntime_over_delta_exec": median_ratio,
        "target_weight_normalized_or_work_completed": weight_normalized,
        "remaining_target_counts": remaining,
        "activity_registered": reply["activity_registered"],
        "activity_after_seconds": reply["activity_after_seconds"],
        "boot_property_watcher": reply["boot_property"],
        "boot_completed": gate["android_boot_completed_verified"],
        "causal_progress": causal,
        "SELinux_enforcing": True,
        "SystemServer_alive": True,
        "reply": reply,
        "deep_scheduler_probe": deep.get("scheduler_probe"),
        "deep_binder_probe": deep.get("binder_live_queues"),
    })
    if stage == 1 and not causal and not gate["android_boot_completed_verified"]:
        st["failed"] = True
        st["failure_reason"] = "Priority change acknowledged but measured SystemServer/CFS causal progress gate failed; do not continue blindly."
    save(REPORTS / "control-state.json", st)
    save(output / "c91f0-proof.json", st)
    shutil.rmtree(output / "native/journal")

    summary = {
        "stage": stage,
        "checkpoint_sha256": checkpoint,
        "instructions": gate["instructions"],
        "reply": reply,
        "SystemServer_runtime_delta_from_source": ss_delta,
        "target_CFS_sample_count": len(rows),
        "target_CFS_median_ratio": median_ratio,
        "weight_normalized_or_work_completed": weight_normalized,
        "activity_registered": reply["activity_registered"],
        "boot_completed": gate["android_boot_completed_verified"],
        "causal_progress": causal,
        "failed": st["failed"],
    }
    save(REPORTS / f"stage{stage}-summary.json", summary)
    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a") as out:
            out.write("continue=" + str(causal and not gate["android_boot_completed_verified"]).lower() + "\n")
            out.write("complete=" + str(gate["android_boot_completed_verified"]).lower() + "\n")
    print(json.dumps(summary))
    if st["failed"]:
        raise SystemExit(st["failure_reason"])


def finish(p):
    st = read(REPORTS / "control-state.json")
    assert st["actual_new_events"] == 1 and len(st["events"]) == 9
    summary = {
        "schema": "reverseandroid-c91f0-runtime-summary-v1",
        "source_checkpoint_sha256": SOURCE_SHA,
        "source_instructions": SOURCE_INSTRUCTIONS,
        "last_stage": st["current_stage"],
        "checkpoint_sha256": st.get("last_checkpoint_sha256"),
        "instructions": st.get("last_instructions"),
        "guarded_priority_change": st["reply"],
        "SystemServer_runtime_delta_from_source": st.get("SystemServer_runtime_delta_from_source"),
        "target_CFS_median_delta_vruntime_over_delta_exec": st.get("target_CFS_median_delta_vruntime_over_delta_exec"),
        "target_weight_normalized_or_work_completed": st.get("target_weight_normalized_or_work_completed"),
        "activity_registered": st.get("activity_registered"),
        "boot_property_watcher": st.get("boot_property_watcher"),
        "android_boot_completed": st.get("boot_completed"),
        "SystemServer_alive": st.get("SystemServer_alive"),
        "SELinux_enforcing": st.get("SELinux_enforcing"),
        "causal_progress": st.get("causal_progress"),
        "failed": st.get("failed"),
        "promotion_authority": False,
        "GPU_QSSD_RAM_disk_ISA_HAL_changed": False,
        "next_rule": (
            "If causal_progress is true, preserve this as correction evidence and create a source-bound canonical continuation only from a fully admitted corrected checkpoint."
            if st.get("causal_progress")
            else "Do not adopt the correction; investigate the next demonstrated blocker."
        ),
    }
    save(REPORTS / "final-summary.json", summary)
    print(json.dumps(summary))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["prepare", "run", "finish"])
    parser.add_argument("--stage", type=int)
    args = parser.parse_args()
    os.chdir(ROOT)
    p = read(PLAN)
    validate_plan(p)
    if args.command == "prepare":
        prepare(p)
    elif args.command == "run":
        assert args.stage in (1, 2)
        run_stage(p, args.stage)
    else:
        finish(p)


if __name__ == "__main__":
    main()
