#!/usr/bin/env python3
"""One admitted explicit CLZ-extension stage; inherit all actual console inputs.

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

from control_console_phase import evaluate_console_phase


def read(path):
    return json.loads(pathlib.Path(path).read_text())


def save(path, value):
    pathlib.Path(path).write_text(json.dumps(value, indent=2)+"\n")


def main():
    n = int(sys.argv[1])
    assert 1 <= n <= 16
    plan = read("state/experiments/c83-plan.json")
    reports = pathlib.Path("evidence/c83-reports")
    reports.mkdir(exist_ok=True, parents=True)
    state_path = reports / "control-state.json"
    assert state_path.exists(), "Authentic prior console and event ledger must be restored first"
    state = read(state_path)
    assert not state["complete"] and not state["failed"]
    source = pathlib.Path(state["source"])
    contract = read(source / "output-contract.json")
    if n == 1:
        assert contract["checkpoint_sha256"] == plan["source_checkpoint_sha256"]
        assert contract["instructions"] == plan["source_instructions"]
    phase = plan["phases"][state["phase"]] if state["phase"] < len(plan["phases"]) else None
    uart = "-"
    if n >= 3 and phase is not None and not state["waiting"]:
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
                 "effect": phase["effect"], "isolated_control_not_primary": False, "measured_peripheral_adoption": True}
        save(uart, event)
    budget = (plan["prefix_instruction_budget"] if n == 1 else 1 if n == 2 else
              plan["waiting_instruction_budget"] if state["waiting"] else
              phase["instruction_budget"] if phase is not None else plan["continuation_instruction_budget"])
    assert hashlib.file_digest(pathlib.Path("build/clz-native/epoch-clz").open("rb"), "sha256").hexdigest() == plan["expected_extended_native_SHA256"]
    output = pathlib.Path(f"evidence/c83-{n}")
    command = ["node", "--expose-gc", "--max-old-space-size=5000", "tools/reverseandroid-epoch.mjs",
               "run", str(source), str(output), "build/clz-native/epoch-clz",
               "build/media/android-c27.raw", "build/reverseandroid/frame", os.environ["GITHUB_SHA"],
               str(budget), uart]
    with (reports / f"stage-{n}.json").open("x") as stdout:
        subprocess.run(command, stdout=stdout, check=True)
    gate = read(output / "c46-progress.json")
    assert gate["source_boundary_verified"]
    assert gate["checkpoint_sha256"] == read(output / "output-contract.json")["checkpoint_sha256"]
    host = read(output / "process-outcome.json")
    assert hashlib.file_digest((output / "current.rvg").open("rb"), "sha256").hexdigest() == gate["checkpoint_sha256"]
    assert host["status"] == "process_completed" and host["exit_code"] == 0 and not host["forced_kill"]
    if n == 1:
        assert uart == "-" and gate["instructions"] == 85047727997
        assert gate["checkpoint_sha256"] == plan["prefix_SHA256"]
    if n == 2:
        assert uart == "-"
        assert gate["checkpoint_sha256"] == plan["after_CLZ_SHA256"]
        subprocess.run([sys.executable, "scripts/verify-clz-retirement.py", str(source), str(output), str(output / "actual-CLZ-retirement.json")], check=True)
    kernel = read(output / "kernel-observation.json")
    assert kernel["live_kernel_security"]["selinux_enforcing"]
    provenance = read(output / "current.json")["provenance"]
    if uart == "-":
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
        subprocess.run(["node", "tools/reverseandroid-console-proof.mjs", str(transcript),
                        plan["ELF_SHA256"], plan["guest_ELF_path"], str(observation)],
                       stdout=observer_log, check=True)
    observed = read(observation)
    assert observed["original_input_bytes_unchanged"]
    assert observed["raw_console_SHA256"] == hashlib.sha256(raw).hexdigest()
    assert not observed["original_C46_or_boot_or_UI_gates_modified"]
    decision = evaluate_console_phase(observed, phase) if phase is not None else {"failed": False, "succeeded": False}
    if decision["failed"]:
        state["failed"] = True
    if decision["succeeded"] and not state["failed"]:
        state["phase"] += 1
        state["waiting"] = False
    state["hardware_commands_completed"] = state["phase"] == len(plan["phases"]) and not state["failed"]
    if state["hardware_commands_completed"]:
        probe = output / "peripherals-live-required.json"
        with (reports / f"stage-{n}-required-peripherals.log").open("x") as log:
            subprocess.run(["node", "--max-old-space-size=5000", "tools/reverseandroid-peripheral-probe.mjs",
                            str(output / "current.rvg"), gate["checkpoint_sha256"], str(probe), plan["guest_ELF_path"]], stdout=log, check=True)
        actual = read(probe)
        assert all(actual[key] for key in ["real_AArch64_peer_alive", "original_oemlock_executable_alive", "original_sensors_executable_alive", "OEM_channel_connected_to_actual_peer", "two_separate_sensor_channels_connected_to_actual_peer"])
        assert len({x["PTY_index"] for x in actual["paired_FDs"]}) == 3
        assert len({x["peer_fd"] for x in actual["paired_FDs"]}) == 3
        assert observed["actual_peer_READY_observed"] and observed["actual_exact_guest_hash_and_path_observed"]
        assert all(observed["original_HAL_Binder_found"].values()) and observed["actual_private_device_filesystem_mounts"]
        assert observed["actual_loaded_policy_unchanged_hashes"] == ["e7dc888c8a4c5ae6f48a175ace91e63ab262e64d4d6f6799a015fed5b4c32f97"]
    state["boot_completed"] = gate["android_boot_completed_verified"]
    state["system_server_alive"] = gate["system_server_alive_verified"]
    state["complete"] = state["hardware_commands_completed"] and state["boot_completed"] and state["system_server_alive"]
    state["source"] = str(output)
    state["last_admitted_sha256"] = gate["checkpoint_sha256"]
    state["last_admitted_instructions"] = gate["instructions"]
    state["combined_original_console_bytes_sha256"] = hashlib.sha256(raw).hexdigest()
    state["original_HAL_registration_seen_in_actual_console"] = observed["original_HAL_Binder_found"]
    state["actual_AArch64_peer_ready_line_seen"] = observed["actual_peer_READY_observed"]
    state["actual_guest_ELF_hash_line_seen"] = observed["actual_exact_guest_hash_and_path_observed"]
    state["actual_private_device_filesystem_mount_seen"] = bool(observed["actual_private_device_filesystem_mounts"])
    state["actual_loaded_policy_unchanged_hashes"] = observed["actual_loaded_policy_unchanged_hashes"]
    state["console_observation"] = str(observation)
    state["prior_canonical_phase_input_repeated"] = False
    state["admitted_parent_primary"] = plan["source_artifact_lock"]
    state["continuation_is_primary"] = True
    state["validated_original_HAL_control"] = plan["validated_original_HAL_control"]
    state["original_admission_and_codecs_unchanged"] = True
    state["original_engine_binary_unchanged"] = False
    state["explicit_ISA_extension"] = plan["ISA_extension"]
    state["native_binary_SHA256"] = plan["expected_extended_native_SHA256"]
    state["prefix_byte_identical_to_original_stop_verified"] = True
    state["actual_CLZ_retirement_verified"] = n >= 2
    state["fresh_power_cycle"] = False
    state["SELinux_enforcing"] = True
    state["ui_response_verified"] = False
    state["ELF_SHA256_expected"] = plan["ELF_SHA256"]
    state["all_event_phases_unique"] = len(state["events"]) == len({event["phase"] for event in state["events"]})
    assert state["all_event_phases_unique"]
    state["original_C81_branch_state_rewritten"] = False
    state["C81_is_reference_after_explicit_C82_fork"] = True
    save(state_path, state)
    save(output / "c83-proof.json", state)
    # Hardlinks and current.rvg remain. Remove only redundant journal names
    # after original admission; transport packing/preservation follows.
    shutil.rmtree(output / "native/journal")
    with open(os.environ["GITHUB_OUTPUT"], "a") as f:
        f.write("complete="+str(state["complete"] or state["failed"]).lower()+"\n")
    print(json.dumps({"stage": n, "phase": state["phase"], "waiting": state["waiting"],
                      "complete": state["complete"], "failed": state["failed"], "gate": gate}))


if __name__ == "__main__":
    main()
