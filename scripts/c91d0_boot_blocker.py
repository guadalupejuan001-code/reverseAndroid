#!/usr/bin/env python3
"""C91D0: source-bound, read-only diagnosis of the C91 stage-5 Android boot stall.

This utility never executes the guest and never edits an RVG.  It validates the
pinned stage-5 artifact set, refuses to use it after a preserved C91 stage-6
artifact exists, and summarizes a deep kernel/Binder/scheduler observation.
"""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
PLAN_PATH = ROOT / "state/experiments/c91d0-plan.json"
EVIDENCE = ROOT / "evidence/c91d0"


def read(path):
    return json.loads(Path(path).read_text())


def save(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=False) + "\n")


def require_sha(value, length=64):
    assert isinstance(value, str) and len(value) == length
    int(value, 16)


def api(path):
    token = os.environ["GH_TOKEN"]
    req = Request(
        "https://api.github.com/repos/guadalupejuan001-code/reverseAndroid/" + path,
        headers={
            "Authorization": "Bearer " + token,
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
        },
    )
    with urlopen(req, timeout=60) as response:
        payload = response.read(8 * 1024 * 1024 + 1)
    assert len(payload) <= 8 * 1024 * 1024
    return json.loads(payload)


def validate_static(plan, lock, transport):
    assert plan["schema"] == "reverseandroid-c91d0-readonly-boot-blocker-v1"
    assert plan["cycle"] == "C91D0"
    assert plan["source_cycle"] == "C91" and plan["source_stage"] == 5
    assert plan["source_checkpoint_sha256"] == "62ebfa1c5dfc9a8ed081d8028ef3f73d07e1854ad085f6c6461e239ed692f210"
    assert plan["source_instructions"] == 170447727998
    assert plan["source_run_id"] == 42596365209
    assert plan["source_execution_commit"] == "a035f386b468c71ad11c32aee88252dbab0b91db"
    assert plan["no_guest_execution"] is True
    assert plan["no_guest_writes"] is True
    assert plan["no_new_uart"] is True
    assert plan["observer_flags"] == [
        "--live-properties",
        "--binder-probe",
        "--scheduler-probe",
        "--zygote-probe",
    ]
    require_sha(plan["source_checkpoint_sha256"])

    assert lock["repository"] == "guadalupejuan001-code/reverseAndroid"
    seed = lock["artifacts"]["seed"]
    assert seed["producer_run_id"] == plan["source_run_id"]
    assert seed["producer_execution_commit"] == plan["source_execution_commit"]
    assert seed["transport_manifest"] == plan["source_transport_manifest"]
    assert len(seed["pieces"]) == 4

    assert transport["schema"] == "reverseandroid-checkpoint-transport-v1"
    assert transport["compression"] == "tar+gzip"
    assert transport["source_contract"]["checkpoint_sha256"] == plan["source_checkpoint_sha256"]
    assert transport["source_contract"]["instructions"] == plan["source_instructions"]
    assert transport["source_contract"]["source_commit"] == plan["source_execution_commit"]
    assert transport["progress"]["source_boundary_verified"] is True
    assert transport["progress"]["system_server_alive_verified"] is True
    assert transport["progress"]["android_boot_completed_verified"] is False
    assert len(transport["parts"]) == len(seed["pieces"]) == 4
    for part, pin in zip(transport["parts"], seed["pieces"]):
        require_sha(part["sha256"])
        require_sha(pin["sha256"])
        assert pin["id"] > 0 and pin["repository"] == lock["repository"]
    return True


def pointer_is_newer_than_stage5(pointer):
    cycle = pointer.get("cycle") or pointer.get("active_cycle")
    if cycle == "C91":
        stage = pointer.get("stage", pointer.get("current_stage", 0))
        return isinstance(stage, int) and stage > 5
    return cycle not in {"C90", "C90R", "C91", None}


def preflight():
    plan = read(PLAN_PATH)
    lock = read(ROOT / plan["source_artifact_lock"])
    transport = read(ROOT / plan["source_transport_manifest"])
    validate_static(plan, lock, transport)

    latest = read(ROOT / "state/latest.json")
    continuation = read(ROOT / "state/continuation.json")
    assert not pointer_is_newer_than_stage5(latest), "main already points beyond C91 stage 5"
    assert not pointer_is_newer_than_stage5(continuation), "continuation already points beyond C91 stage 5"

    response = api(f"actions/runs/{plan['source_run_id']}/artifacts?per_page=100")
    artifacts = response.get("artifacts", [])
    stale = [a["name"] for a in artifacts if a["name"].startswith(plan["stale_if_artifact_prefix"])]
    assert not stale, "C91 stage 6 is already preserved; refuse stale C91D0 source: " + ", ".join(stale)

    by_id = {a["id"]: a for a in artifacts}
    checked = []
    for pin in lock["artifacts"]["seed"]["pieces"] + [lock["artifacts"]["metadata"]]:
        actual = by_id.get(pin["id"])
        assert actual is not None, f"missing pinned artifact {pin['id']}"
        assert actual.get("expired") is False
        digest = (actual.get("digest") or "").removeprefix("sha256:")
        assert digest == pin["sha256"], f"artifact digest mismatch for {pin['id']}"
        checked.append({"id": pin["id"], "name": actual["name"], "sha256": digest})

    record = {
        "schema": "reverseandroid-c91d0-source-lease-v1",
        "observed_UTC": datetime.now(timezone.utc).isoformat(),
        "source_checkpoint_sha256": plan["source_checkpoint_sha256"],
        "source_instructions": plan["source_instructions"],
        "source_run_id": plan["source_run_id"],
        "source_execution_commit": plan["source_execution_commit"],
        "main_latest_cycle": latest.get("cycle"),
        "main_latest_stage": latest.get("stage"),
        "main_latest_checkpoint_sha256": latest.get("checkpoint_sha256"),
        "active_run_stage6_preserved": False,
        "pinned_artifacts_verified": checked,
        "guest_execution_permitted": False,
        "guest_mutation_permitted": False,
    }
    save(EVIDENCE / "source-lease.json", record)
    print(json.dumps(record, indent=2))


def int_accounting(task):
    return {k: int(v) for k, v in (task.get("accounting") or {}).items()}


def accounting_delta(before, after):
    keys = sorted(set(before) | set(after))
    return {k: int(after.get(k, 0)) - int(before.get(k, 0)) for k in keys}


def classify(*, boot_completed, ss_task, delta, console, bootanimation_alive, binder, tasks):
    lockup = "workqueue lockup" in console
    binder_work = "rust_binder" in console
    vmap_work = "drain_vmap_area_work" in console
    nearly_no_ss_runtime = delta.get("sum_exec_runtime", 0) <= 10_000_000
    ss_runnable = ss_task.get("on_rq") == 1 and ss_task.get("on_cpu") == 0
    preopt_names = {"cppreopts.sh", "preopt2cachenam", "mv", "cp", "fsync"}
    preopt = [t for t in tasks if t.get("name") in preopt_names]
    runnable_preopt = [t for t in preopt if t.get("on_rq") == 1]

    supported = (
        not boot_completed
        and bootanimation_alive
        and lockup
        and binder_work
        and vmap_work
        and nearly_no_ss_runtime
        and ss_runnable
        and len(preopt) >= 8
    )
    return {
        "H1_supported": supported,
        "system_server_runnable_not_running": ss_runnable,
        "system_server_nearly_no_runtime_since_stage1": nearly_no_ss_runtime,
        "workqueue_lockup_present": lockup,
        "rust_binder_work_in_lockup": binder_work,
        "vmap_drain_work_in_lockup": vmap_work,
        "preopt_family_process_count": len(preopt),
        "runnable_preopt_family_process_count": len(runnable_preopt),
        "binder_probe_error": binder.get("error") if isinstance(binder, dict) else "binder probe missing",
    }


def diagnose(source_dir, deep_file):
    plan = read(PLAN_PATH)
    source = Path(source_dir)
    deep = read(deep_file)
    contract = read(source / "output-contract.json")
    current = read(source / "current.json")
    result = read(source / "result.json")
    proof = read(source / "zero-input-proof.json")

    assert contract["checkpoint_sha256"] == deep["checkpoint_sha256"] == plan["source_checkpoint_sha256"]
    assert contract["instructions"] == deep["instructions"] == plan["source_instructions"]
    assert result["checkpoint_sha256"] == plan["source_checkpoint_sha256"]
    assert result["android_boot_verified"] is False
    assert proof["boot_completed"] is False
    assert proof["system_server_alive"] is True
    assert deep["input_unchanged"] is True
    assert deep["live_kernel_security"]["selinux_enforcing"] is True

    tasks = deep["tasks"]
    ss = next(t for t in tasks if t.get("pid") == 7354 and t.get("name") == "system_server")
    surfaceflinger = next(t for t in tasks if t.get("pid") == 669 and t.get("name") == "surfaceflinger")
    bootanim = next((t for t in tasks if t.get("name") == "bootanimation"), None)
    zygote = next(t for t in tasks if t.get("pid") == 5629)

    stage1 = read(ROOT / "docs/evidence/c91/stage1-original-CI-authority.json")
    stage1_ss = int_accounting(stage1["original_SystemServer"])
    stage5_ss = int_accounting(ss)
    delta = accounting_delta(stage1_ss, stage5_ss)

    console = current.get("console", "")
    binder = deep.get("binder_live_queues", {})
    scheduler = deep.get("scheduler_probe", {})
    verdict = classify(
        boot_completed=deep.get("live_boot_property", {}).get("boot_completed_property_live", False),
        ss_task=ss,
        delta=delta,
        console=console,
        bootanimation_alive=bootanim is not None and bootanim.get("exit_state", 0) == 0,
        binder=binder,
        tasks=tasks,
    )

    counts = Counter(t.get("name", "") for t in tasks)
    runnable = [t for t in tasks if t.get("on_rq") == 1]
    top_names = counts.most_common(20)
    lockup_lines = [
        line for line in console.splitlines()
        if "workqueue lockup" in line
        or "in-flight:" in line
        or "Workqueue:" in line
    ]

    report = {
        "schema": "reverseandroid-c91d0-boot-blocker-diagnosis-v1",
        "observed_UTC": datetime.now(timezone.utc).isoformat(),
        "source": {
            "cycle": "C91",
            "stage": 5,
            "checkpoint_sha256": plan["source_checkpoint_sha256"],
            "instructions": plan["source_instructions"],
            "guest_execution_during_diagnostic": False,
            "guest_writes_during_diagnostic": False,
        },
        "android": {
            "sys_boot_completed": deep.get("live_boot_property", {}).get("boot_completed_property_live", False),
            "selinux_enforcing": deep["live_kernel_security"]["selinux_enforcing"],
            "system_server": ss,
            "system_server_accounting_stage1": stage1_ss,
            "system_server_accounting_stage5": stage5_ss,
            "system_server_accounting_delta_stage1_to_stage5": delta,
            "zygote": zygote,
            "surfaceflinger": surfaceflinger,
            "bootanimation": bootanim,
            "live_process_count": deep.get("task_count"),
        },
        "scheduler": {
            "probe": scheduler,
            "runnable_process_count": len(runnable),
            "runnable_process_names": Counter(t.get("name", "") for t in runnable).most_common(30),
            "process_name_counts": top_names,
        },
        "binder": binder,
        "console_blocker_lines": lockup_lines,
        "hypothesis": {
            "id": "H1",
            **verdict,
            "probable_root_cause": (
                "CPU0 scheduling/workqueue starvation during cppreopts/file-preopt activity; "
                "a locked workqueue contains rust_binder process work and drain_vmap_area_work, "
                "while SystemServer remains runnable but receives effectively no runtime."
                if verdict["H1_supported"]
                else "H1 not fully confirmed; inspect Binder/scheduler probe fields before correction."
            ),
            "minimal_next_correction_rule": (
                "Do not alter GPU, QSSD, RAM, disk, ISA or HALs. First run an isolated scheduling/preopt "
                "control from this exact source that changes only the demonstrated starving workload or "
                "its scheduling relationship, then require SystemServer runtime and boot milestones to advance."
            ),
        },
        "classification": [
            {"finding": "SystemServer runtime starvation", "severity": "probably_blocking"},
            {"finding": "CPU0 workqueue lockup", "severity": "probably_blocking"},
            {"finding": "bootanimation still alive", "severity": "secondary_symptom"},
            {"finding": "sys.boot_completed absent", "severity": "boot_milestone_not_reached"},
        ],
    }
    save(EVIDENCE / "diagnosis.json", report)
    print(json.dumps(report, indent=2))


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="mode", required=True)
    sub.add_parser("preflight")
    d = sub.add_parser("diagnose")
    d.add_argument("source")
    d.add_argument("deep_observation")
    args = parser.parse_args()
    if args.mode == "preflight":
        preflight()
    else:
        diagnose(args.source, args.deep_observation)


if __name__ == "__main__":
    main()
