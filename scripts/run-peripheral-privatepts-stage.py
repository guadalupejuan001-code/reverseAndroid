#!/usr/bin/env python3
"""One fully admitted original CPU stage; at most one pinned UART event.

State is advanced only after the original controller admits the complete result.
The workflow preserves each transport before releasing earlier host copies.
"""
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys


def read(path):
    return json.loads(pathlib.Path(path).read_text())


def save(path, value):
    pathlib.Path(path).write_text(json.dumps(value, indent=2)+"\n")


def main():
    n = int(sys.argv[1])
    assert 1 <= n <= 16
    plan = read("state/experiments/peripheral-privatepts-control-plan.json")
    reports = pathlib.Path("evidence/peripheral-privatepts-control-reports")
    reports.mkdir(exist_ok=True, parents=True)
    state_path = reports / "control-state.json"
    if state_path.exists():
        state = read(state_path)
    else:
        state = {"source": "build/source-privatepts", "phase": 0, "waiting": False,
                 "events": [], "complete": False, "failed": False}
    assert not state["complete"] and not state["failed"]
    source = pathlib.Path(state["source"])
    contract = read(source / "output-contract.json")
    if n == 1:
        assert contract["checkpoint_sha256"] == plan["source_checkpoint_sha256"]
        assert contract["instructions"] == plan["source_instructions"]
    phase = plan["phases"][state["phase"]]
    uart = "-"
    if not state["waiting"]:
        input_file = pathlib.Path(phase["input_file"])
        data = input_file.read_bytes()
        assert len(data) == phase["input_bytes"] <= 4096
        assert hashlib.sha256(data).hexdigest() == phase["input_sha256"]
        uart = str(reports / f"stage-{n}-input-plan.json")
        event = {"schema": "reverseandroid-authenticated-console-input-plan-v1",
                 "purpose": "diagnostic_console", "input_file": str(input_file.resolve()),
                 "input_sha256": phase["input_sha256"], "input_bytes": len(data),
                 "source_checkpoint_sha256": contract["checkpoint_sha256"],
                 "source_instructions": contract["instructions"],
                 "effect": phase["effect"], "isolated_control_not_primary": True}
        save(uart, event)
    output = pathlib.Path(f"evidence/peripheral-privatepts-control-{n}")
    command = ["node", "--expose-gc", "--max-old-space-size=5000", "tools/reverseandroid-epoch.mjs",
               "run", str(source), str(output), "build/reverseandroid/epoch",
               "build/media/android-c27.raw", "build/reverseandroid/frame", os.environ["GITHUB_SHA"],
               str(phase["instruction_budget"]), uart]
    with (reports / f"stage-{n}.json").open("x") as stdout:
        subprocess.run(command, stdout=stdout, check=True)
    gate = read(output / "c46-progress.json")
    assert gate["source_boundary_verified"]
    assert gate["checkpoint_sha256"] == read(output / "output-contract.json")["checkpoint_sha256"]
    host = read(output / "process-outcome.json")
    assert host["status"] == "process_completed" and host["exit_code"] == 0 and not host["forced_kill"]
    kernel = read(output / "kernel-observation.json")
    assert kernel["live_kernel_security"]["selinux_enforcing"]
    if uart != "-":
        result = read(output / "result.json")
        actual = result["external_uart_input"]
        assert actual["bytes"] == phase["input_bytes"] and actual["sha256"] == phase["input_sha256"]
        assert actual["instructions"] == contract["instructions"]
        state["events"].append({"phase": state["phase"], "input_plan": uart, "actual_native_event": actual})
        state["waiting"] = True
    transcript = reports / "console-all.bin"
    with transcript.open("ab") as f:
        f.write((output / "console-new.txt").read_bytes())
    raw = transcript.read_bytes()
    lines = raw.decode("utf-8", errors="replace").splitlines()
    markers = phase.get("response_markers", [phase["response_marker"]])
    codes=[]
    for name in markers:
        pattern=re.compile(re.escape(name)+r" RC_([0-9]+)")
        codes += [int(match[1]) for line in lines if (match := pattern.fullmatch(line.strip()))]
    failures=[]
    for name in phase.get("failure_markers", []):
        pattern=re.compile(re.escape(name)+r" RC_([0-9]+)")
        failures += [int(match[1]) for line in lines if (match := pattern.fullmatch(line.strip())) and int(match[1]) != 0]
    if failures:
        state["failed"]=True
    if codes and not state["failed"]:
        if all(code == 0 for code in codes):
            state["phase"] += 1
            state["waiting"] = False
            state["complete"] = state["phase"] == len(plan["phases"])
        else:
            state["failed"] = True
    state["source"] = str(output)
    state["last_admitted_sha256"] = gate["checkpoint_sha256"]
    state["last_admitted_instructions"] = gate["instructions"]
    state["combined_original_console_bytes_sha256"] = hashlib.sha256(raw).hexdigest()
    state["original_HAL_registration_seen_in_actual_console"] = {
        name: any(line.strip() == f"Service {name}: found" for line in lines)
        for name in ["android.hardware.oemlock.IOemLock/default", "android.hardware.sensors.ISensors/default"]
    }
    state["actual_AArch64_peer_ready_line_seen"] = any(line.strip() == "REVERSE_PERIPHERAL_PEER_READY" for line in lines)
    state["actual_guest_ELF_hash_line_seen"] = any(line.strip() == plan["ELF_SHA256"]+"  "+plan["guest_ELF_path"] for line in lines)
    state["actual_private_device_filesystem_mount_seen"] = any(
        len(parts := line.strip().split()) >= 4 and parts[1:3] == ["/dev/ra-pts", "devpts"]
        and "fscontext=" in parts[3] and "u:object_r:tmpfs:s0" in parts[3]
        for line in lines)
    state["actual_loaded_policy_unchanged_hashes"] = [match[1] for line in lines
        if (match := re.fullmatch(r"REVERSE_PTS_POLICY_UNCHANGED SHA_([0-9a-f]{64})", line.strip()))]
    state["control_is_promotion_authority"] = False
    state["ELF_SHA256_expected"] = plan["ELF_SHA256"]
    state["all_event_phases_unique"] = len(state["events"]) == len({event["phase"] for event in state["events"]})
    assert state["all_event_phases_unique"]
    state["main_C81_modified"] = False
    save(state_path, state)
    save(output / "peripheral-privatepts-control-proof.json", state)
    # Hardlinks and current.rvg remain. Remove only redundant journal names
    # after original admission; transport packing/preservation follows.
    shutil.rmtree(output / "native/journal")
    with open(os.environ["GITHUB_OUTPUT"], "a") as f:
        f.write("complete="+str(state["complete"] or state["failed"]).lower()+"\n")
    print(json.dumps({"stage": n, "phase": state["phase"], "waiting": state["waiting"],
                      "complete": state["complete"], "failed": state["failed"], "gate": gate}))


if __name__ == "__main__":
    main()
