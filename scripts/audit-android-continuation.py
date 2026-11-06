#!/usr/bin/env python3
"""Read-only, bounded audit of already admitted Android continuation results.

This consumes original producer evidence. It never replaces epoch preflight,
executes a guest, changes input, promotes a checkpoint, or validates usable UI.
Snapshot verification streams bytes instead of making another dense RAM copy.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re


HEX = re.compile(r"[0-9a-f]{64}")
MAX_METADATA_BYTES = 16 * 1024 * 1024


def require(condition, message):
    if not condition:
        raise ValueError(message)


def file_sha(path):
    before = path.stat()
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    after = path.stat()
    require((before.st_ino, before.st_size, before.st_mtime_ns) ==
            (after.st_ino, after.st_size, after.st_mtime_ns),
            "File changed during observation: " + str(path))
    return digest


def identity(task):
    value = task.get("zygote_startup") or task.get("system_server_runtime")
    return value if isinstance(value, dict) else {}


def task_role(task):
    value = identity(task)
    if (task.get("exit_state") != 0 or value.get("argv_verified") is not True
            or value.get("exe_path") != "/system/bin/app_process64"):
        return None
    args = value.get("argv", [])
    if args == ["system_server"] and value.get("comm") == "system_server":
        return "system_server"
    if args == ["zygote64"] or ("--zygote" in args and "--start-system-server" in args):
        return "zygote"
    return None


def summarize_task(task):
    value = identity(task)
    accounting = task.get("accounting", {})
    counters = {}
    for key in ["utime", "stime", "sum_exec_runtime", "nvcsw", "nivcsw"]:
        if key in accounting:
            raw = accounting[key]
            require(type(raw) in (str, int) and str(raw).isdigit(), "Invalid task counter")
            counters[key] = int(raw)
    require(isinstance(task.get("task"), str) and task["task"].startswith("0xffff"),
            "Missing kernel task identity")
    return {"pid": task["pid"], "task": task["task"],
            "exe_path": value["exe_path"], "argv": value["argv"],
            "parent_pid": value.get("parent_pid"), "state": task.get("state"),
            "on_rq": task.get("on_rq"), "on_cpu": task.get("on_cpu"),
            "accounting": counters}


def transition(previous, current):
    old, new = previous.get("zygote"), current.get("zygote")
    if new is None:
        return {"kind": "zygote_absent", "permanent_deadlock_proven": False}
    if old is None:
        return {"kind": "zygote_present", "permanent_deadlock_proven": False}
    same = all(old.get(k) == new.get(k) for k in ["pid", "task", "exe_path", "parent_pid"])
    if not same:
        return {"kind": "zygote_identity_changed", "old_pid": old["pid"],
                "new_pid": new["pid"], "counter_delta_not_compared": True,
                "permanent_deadlock_proven": False}
    deltas = {}
    for key in ["utime", "stime", "sum_exec_runtime"]:
        if key in old["accounting"] and key in new["accounting"]:
            value = new["accounting"][key] - old["accounting"][key]
            require(value >= 0, "Counter regressed for the same authenticated task")
            deltas[key] = value
    require(deltas, "No task counters available for progress comparison")
    return {"kind": "zygote_cpu_progress" if any(deltas.values()) else "no_zygote_cpu_progress_observed",
            "zygote_pid": new["pid"], "delta_ns": deltas,
            "permanent_deadlock_proven": False}


def audit(pin, config, verify_snapshot=False):
    root = Path(pin["directory"]).resolve()
    require(HEX.fullmatch(pin["checkpoint_sha256"]) is not None, "Expected checkpoint SHA-256 required")
    require(type(pin["instructions"]) is int and pin["instructions"] > 0, "Invalid instruction pin")

    def read(name):
        require(Path(name).name == name and name not in (".", ".."), "Unsafe metadata path")
        path = root / name
        require(path.stat().st_size <= MAX_METADATA_BYTES, "Metadata exceeds observation bound")
        return json.loads(path.read_text())

    contract = read("output-contract.json")
    require(contract["checkpoint_sha256"] == pin["checkpoint_sha256"], "Checkpoint pin mismatch")
    require(contract["instructions"] == pin["instructions"], "Instruction pin mismatch")
    require(contract["epoch_sha256"] == config["expected_epoch_sha256"], "Epoch pin mismatch")
    report_name = contract["report_file"]
    require(Path(report_name).name == report_name and report_name not in (".", ".."), "Unsafe report path")
    for filename, field in [(report_name, "report_sha256"), ("result.json", "result_sha256"),
                            ("process-outcome.json", "process_sha256")]:
        require(file_sha(root / filename) == contract[field], "Bound evidence hash mismatch: " + filename)
    report, result, host = read(report_name), read("result.json"), read("process-outcome.json")
    gate, kernel, runtime = read("c46-progress.json"), read("kernel-observation.json"), read("runtime-evidence.json")
    proof = read(config["proof_file"])
    for name, item in [("gate", gate), ("kernel", kernel), ("runtime", runtime), ("result", result)]:
        require(item["checkpoint_sha256"] == pin["checkpoint_sha256"] and
                item["instructions"] == pin["instructions"], "Unbound " + name)
    require(report["checkpoint_sha256"] == pin["checkpoint_sha256"] and
            report["guest"]["instructions"] == pin["instructions"], "Unbound native report")
    require(gate["source_boundary_verified"] is True, "Original admission missing")
    require(host["status"] == "process_completed" and host["exit_code"] == 0
            and host["forced_kill"] is False and host["trigger"] is None, "Unclean native process")
    require(report["error"] == "" and report["stop_reason"] in ("instruction_budget", "host_wall_budget"),
            "Native terminal outcome")
    require(kernel["input_unchanged"] is True and
            kernel["live_kernel_security"]["selinux_enforcing"] is True, "Original read-only/security evidence missing")
    require(read("host-policy.json")["native_binary_sha256"] == config["expected_native_sha256"], "Unexpected native binary")
    require(report["guest"].get("logical_ram_bytes") == 2147483648 and
            report["guest"].get("counter_instructions_per_tick") == 64,
            "Original RAM or clock cadence changed")
    provenance = report["provenance"]
    require(provenance["new_uart_input_bytes"] == 0 and "external_uart_input" not in provenance,
            "Unexpected new guest input")
    require(proof["last_admitted_sha256"] == pin["checkpoint_sha256"] and
            proof["last_admitted_instructions"] == pin["instructions"], "Unbound continuation proof")
    require(proof["events"] == config["expected_events"] and proof["phase"] == len(config["expected_events"])
            and proof["waiting"] is False and proof["failed"] is False, "Event ledger or phase mismatch")
    roles = {"zygote": [], "system_server": []}
    for task in kernel["tasks"]:
        role = task_role(task)
        if role:
            roles[role].append(summarize_task(task))
    require(len(roles["zygote"]) <= 1, "Ambiguous live Zygote identity")
    if gate["system_server_alive_verified"]:
        require(roles["system_server"], "SystemServer gate lacks live executable/argument evidence")
    if gate["android_boot_completed_verified"]:
        require(gate["system_server_alive_verified"] and runtime["sys_boot_completed_live"] is True
                and runtime["framework_runtime_verified"] is True, "Incomplete original boot evidence")
    complete = gate["android_boot_completed_verified"] is True
    snapshot = root / "current.rvg"
    if verify_snapshot:
        require(snapshot.is_file(), "Complete snapshot file missing")
        require(file_sha(snapshot) == pin["checkpoint_sha256"], "Whole snapshot hash mismatch")
    return {"checkpoint_sha256": pin["checkpoint_sha256"], "instructions": pin["instructions"],
            "parent_checkpoint_sha256": provenance["input_checkpoint_sha256"],
            "whole_snapshot_sha256_verified_locally": bool(verify_snapshot),
            "whole_snapshot_file_present": snapshot.is_file(),
            "whole_snapshot_bytes": snapshot.stat().st_size if snapshot.is_file() else None,
            "epoch_sha256": contract["epoch_sha256"],
            "native_sha256": config["expected_native_sha256"], "original_source_boundary_verified": True,
            "new_UART_bytes": 0, "inherited_events": len(proof["events"]),
            "zygote": roles["zygote"][0] if roles["zygote"] else None,
            "system_server_candidates": roles["system_server"],
            "original_system_server_verified": gate["system_server_alive_verified"],
            "original_boot_completed_verified": complete, "live_peripherals_verified": proof["live_peripherals_verified"],
            "usable_UI_verified_by_this_tool": False, "host_elapsed_ms": report["host_elapsed_ms"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pins", type=Path)
    parser.add_argument("output", type=Path)
    verify = parser.add_mutually_exclusive_group()
    verify.add_argument("--verify-snapshots", action="store_true")
    verify.add_argument("--verify-latest-snapshot", action="store_true")
    args = parser.parse_args()
    config = json.loads(args.pins.read_text())
    require(1 <= len(config["boundaries"]) <= 32, "Expected 1 to 32 bounded observations")
    results = [audit(pin, config, args.verify_snapshots or
                     (args.verify_latest_snapshot and index == len(config["boundaries"])-1))
               for index, pin in enumerate(config["boundaries"])]
    transitions = []
    for previous, current in zip(results, results[1:]):
        require(current["instructions"] > previous["instructions"], "Non-forward boundary")
        require(current["parent_checkpoint_sha256"] == previous["checkpoint_sha256"], "Broken checkpoint ancestry")
        transitions.append({"from_instructions": previous["instructions"], "to_instructions": current["instructions"],
                            **transition(previous, current)})
    output = {"schema": "reverseandroid-bound-progress-audit-v1", "read_only": True,
              "original_admission_replaced": False, "guest_execution_performed": False,
              "boundaries": results, "transitions": transitions}
    with args.output.open("x") as stream:
        json.dump(output, stream, indent=2)
        stream.write("\n")
    print(json.dumps({"boundaries": len(results), "last_instructions": results[-1]["instructions"],
                      "last_transition": transitions[-1] if transitions else None,
                      "boot_completed": results[-1]["original_boot_completed_verified"]}))


if __name__ == "__main__":
    main()
