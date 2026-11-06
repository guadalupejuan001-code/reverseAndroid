#!/usr/bin/env python3
"""Continue the latest admitted epoch with one measured, guarded OS input.

The original runner and admission gates remain authoritative. Preserve the
complete output transport before releasing any earlier host copy.
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
    pathlib.Path(path).write_text(json.dumps(value, indent=2) + "\n")


def main():
    n = int(sys.argv[1])
    plan = read("state/experiments/c81-plan.json")
    assert 1 <= n <= len(plan["instruction_budgets"])
    reports = pathlib.Path("evidence/c81-reports")
    reports.mkdir(parents=True, exist_ok=True)
    state_path = reports / "continuation-state.json"
    state = read(state_path) if state_path.exists() else {
        "source": "build/source-c81", "input_events": [],
        "guard_result_seen": False, "guard_succeeded": False,
        "failed": False, "boot_completed": False,
    }
    assert not state["failed"] and not state["boot_completed"]
    source = pathlib.Path(state["source"])
    contract = read(source / "output-contract.json")
    uart = "-"
    if n == 1:
        assert contract["checkpoint_sha256"] == plan["source_checkpoint_sha256"]
        assert contract["instructions"] == plan["source_instructions"]
        # Re-observe the actual source, never an old experimental snapshot.
        before = reports / "source-threads-readonly.json"
        subprocess.run(["node", "--max-old-space-size=5000",
                        "tools/reverseandroid-thread-probe.mjs",
                        str(source / "current.rvg"), contract["checkpoint_sha256"],
                        str(before)], check=True)
        observed = read(before)
        assert observed["input_unchanged"]
        assert observed["checkpoint_sha256"] == contract["checkpoint_sha256"]
        group = next(g for g in observed["groups"] if g["pid"] == 562)
        jit = next(t for t in group["threads"] if t["pid"] == 1156)
        assert jit["tgid"] == 562 and jit["name"] == "Jit thread pool"
        assert jit["priority"] == jit["normal_prio"] == 139
        assert jit["scheduling"]["credentials"]["uid"] == "0"
        kernel = read(source / "kernel-observation.json")
        assert kernel["checkpoint_sha256"] == contract["checkpoint_sha256"]
        zygote = next(p for p in kernel["zygote_launch"]["processes"] if p["pid"] == 562)
        assert zygote["exe_path"] == "/system/bin/app_process64"
        assert zygote["argv_verified"] and zygote["argv"] == ["zygote64"]
        uart = "state/experiments/c81-priority-input-plan.json"
        event = read(uart)
        assert event["source_checkpoint_sha256"] == contract["checkpoint_sha256"]
        assert event["source_instructions"] == contract["instructions"]
        data = pathlib.Path("state/experiments/c81-priority-input.txt").read_bytes()
        assert len(data) == event["input_bytes"]
        assert hashlib.sha256(data).hexdigest() == event["input_sha256"]
    output = pathlib.Path(f"evidence/c81-{n}")
    with (reports / f"stage-{n}.json").open("x") as stdout:
        subprocess.run(["node", "--expose-gc", "--max-old-space-size=5000",
                        "tools/reverseandroid-epoch.mjs", "run", str(source), str(output),
                        "build/reverseandroid/epoch", "build/media/android-c27.raw",
                        "build/reverseandroid/frame", os.environ["GITHUB_SHA"],
                        str(plan["instruction_budgets"][n-1]), uart],
                       stdout=stdout, check=True)
    gate = read(output / "c46-progress.json")
    assert gate["source_boundary_verified"]
    assert gate["checkpoint_sha256"] == read(output / "output-contract.json")["checkpoint_sha256"]
    assert hashlib.file_digest((output / "current.rvg").open("rb"), "sha256").hexdigest() == gate["checkpoint_sha256"]
    host = read(output / "process-outcome.json")
    assert host["status"] == "process_completed" and host["exit_code"] == 0 and not host["forced_kill"]
    assert read(output / "kernel-observation.json")["live_kernel_security"]["selinux_enforcing"]
    provenance = read(output / "current.json")["provenance"]
    if n == 1:
        actual = read(output / "result.json")["external_uart_input"]
        event = read(uart)
        assert actual["instructions"] == contract["instructions"]
        assert actual["bytes"] == event["input_bytes"] and actual["sha256"] == event["input_sha256"]
        state["input_events"].append(actual)
    else:
        assert provenance["new_uart_input_bytes"] == 0 and "external_uart_input" not in provenance
    assert len(state["input_events"]) == 1
    transcript = reports / "console-all.bin"
    with transcript.open("ab") as f:
        f.write((output / "console-new.txt").read_bytes())
    raw = transcript.read_bytes()
    lines = [line.strip() for line in raw.decode("utf-8", errors="replace").splitlines()]
    codes = [int(m[1]) for line in lines
             if (m := re.fullmatch(r"REVERSE_JIT_PRIORITY_RESULT RC_([0-9]+)", line))]
    state["guard_result_seen"] = bool(codes)
    state["guard_succeeded"] = bool(codes) and all(code == 0 for code in codes) and "REVERSE_JIT_PRIORITY_DONE NICE_0" in lines
    state["failed"] = any(code != 0 for code in codes)
    state["source"] = str(output)
    state["checkpoint_sha256"] = gate["checkpoint_sha256"]
    state["instructions"] = gate["instructions"]
    state["boot_completed"] = gate["android_boot_completed_verified"]
    state["system_server_alive"] = gate["system_server_alive_verified"]
    state["console_sha256"] = hashlib.sha256(raw).hexdigest()
    state["original_admission_and_codecs_unchanged"] = True
    state["fresh_power_cycle"] = False
    state["SELinux_enforcing"] = True
    state["ui_response_verified"] = False
    save(output / "c81-continuation-proof.json", state)
    save(state_path, state)
    # Optional observers cannot erase a fully admitted state.
    with (output / "threads-readonly.log").open("w") as log:
        probe = subprocess.run(["node", "--max-old-space-size=5000",
                                "tools/reverseandroid-thread-probe.mjs",
                                str(output / "current.rvg"), gate["checkpoint_sha256"],
                                str(output / "threads-readonly.json")], stdout=log, stderr=subprocess.STDOUT)
    save(output / "optional-thread-probe-outcome.json", {"exit_code": probe.returncode})
    shutil.rmtree(output / "native/journal")
    with open(os.environ["GITHUB_OUTPUT"], "a") as f:
        f.write("stop=" + str(state["failed"] or state["boot_completed"]).lower() + "\n")


if __name__ == "__main__":
    main()
