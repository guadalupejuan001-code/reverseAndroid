#!/usr/bin/env python3
"""One fully admitted original CPU stage; at most one pinned UART event.

State is advanced only after the original controller admits the complete result.
The workflow preserves each transport before releasing earlier host copies.
"""
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys


def read(path):
    return json.loads(pathlib.Path(path).read_text())


def save(path, value):
    pathlib.Path(path).write_text(json.dumps(value, indent=2)+"\n")


def main():
    n = int(sys.argv[1])
    assert 1 <= n <= 8
    plan = read("state/experiments/c81-systemserver-diagnostic-plan.json")
    reports = pathlib.Path("evidence/c81-systemserver-diagnostic-reports")
    reports.mkdir(exist_ok=True, parents=True)
    state_path = reports / "control-state.json"
    if state_path.exists():
        state = read(state_path)
    else:
        state = {"source": "build/source-ssdiag", "phase": 0, "waiting": False,
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
    output = pathlib.Path(f"evidence/c81-systemserver-diagnostic-{n}")
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
    assert hashlib.file_digest((output / "current.rvg").open("rb"), "sha256").hexdigest() == gate["checkpoint_sha256"]
    assert host["status"] == "process_completed" and host["exit_code"] == 0 and not host["forced_kill"]
    kernel = read(output / "kernel-observation.json")
    assert kernel["live_kernel_security"]["selinux_enforcing"]
    if uart == "-":
        provenance = read(output / "current.json")["provenance"]
        assert provenance["new_uart_input_bytes"] == 0 and "external_uart_input" not in provenance
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
    observation = output / "console-observation.json"
    with (reports / f"stage-{n}-console-observer.log").open("x") as observer_log:
        subprocess.run(["node", "tools/reverseandroid-diagnostic-console.mjs", str(transcript),
                        str(observation)],
                       stdout=observer_log, check=True)
    observed = read(observation)
    assert observed["original_console_bytes_unchanged"]
    assert observed["raw_console_SHA256"] == hashlib.sha256(raw).hexdigest()
    assert not observed["original_C46_or_boot_or_UI_gates_modified"]
    if observed["diagnostic_failed"]:
        state["failed"] = True
    if observed["diagnostic_completed"] and not state["failed"]:
        state["complete"] = True
        state["phase"] = 1
        state["waiting"] = False
    state["source"] = str(output)
    state["last_admitted_sha256"] = gate["checkpoint_sha256"]
    state["last_admitted_instructions"] = gate["instructions"]
    state["combined_original_console_bytes_sha256"] = hashlib.sha256(raw).hexdigest()
    state["actual_diagnostic_observation"] = observed
    state["actual_SystemServer_alive_gate"] = gate["system_server_alive_verified"]
    state["original_boot_completed_gate"] = gate["android_boot_completed_verified"]
    state["control_is_promotion_authority"] = False
    state["main_C81_modified"] = False
    state["one_readonly_input_once_then_zero_further_UART"] = True
    assert len(state["events"]) == 1
    save(state_path, state)
    save(output / "c81-systemserver-diagnostic-proof.json", state)
    # Hardlinks and current.rvg remain. Remove only redundant journal names
    # after original admission; transport packing/preservation follows.
    shutil.rmtree(output / "native/journal")
    with open(os.environ["GITHUB_OUTPUT"], "a") as f:
        f.write("complete="+str(state["complete"] or state["failed"]).lower()+"\n")
    print(json.dumps({"stage": n, "phase": state["phase"], "waiting": state["waiting"],
                      "complete": state["complete"], "failed": state["failed"], "gate": gate}))


if __name__ == "__main__":
    main()
