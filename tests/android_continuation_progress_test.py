"""Reject misleading startup progress and unbound or modified native evidence."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("audit", Path(__file__).parents[1] / "scripts/audit-android-continuation.py")
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)


def task(pid=5629, pointer="0xffff1000", utime=8, stime=3):
    return {"pid": pid, "task": pointer, "state": 0, "exit_state": 0,
            "accounting": {"utime": str(utime), "stime": str(stime)},
            "zygote_startup": {"argv_verified": True, "exe_path": "/system/bin/app_process64",
                               "parent_pid": 1, "comm": "main", "argv": ["zygote64"]}}


class ProgressTests(unittest.TestCase):
    def summary(self, value):
        return {"zygote": audit.summarize_task(value)}

    def test_global_instruction_count_is_not_task_progress(self):
        old = self.summary(task()); new = copy.deepcopy(old)
        old["instructions"] = 1; new["instructions"] = 1000000001
        result = audit.transition(old, new)
        self.assertEqual(result["kind"], "no_zygote_cpu_progress_observed")
        self.assertFalse(result["permanent_deadlock_proven"])

    def test_actual_user_and_kernel_counters_measure_progress(self):
        result = audit.transition(self.summary(task()), self.summary(task(utime=10, stime=4)))
        self.assertEqual(result["kind"], "zygote_cpu_progress")
        self.assertEqual(result["delta_ns"], {"utime": 2, "stime": 1})

    def test_restart_does_not_compare_unrelated_counters(self):
        result = audit.transition(self.summary(task()), self.summary(task(pid=6000, utime=0, stime=0)))
        self.assertEqual(result["kind"], "zygote_identity_changed")

    def test_recycled_pid_is_also_a_new_identity(self):
        result = audit.transition(self.summary(task()), self.summary(task(pointer="0xffff2000", utime=0)))
        self.assertEqual(result["kind"], "zygote_identity_changed")

    def test_same_task_counter_regression_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "Counter regressed"):
            audit.transition(self.summary(task()), self.summary(task(utime=0)))

    def test_name_alone_cannot_verify_system_server(self):
        value = task(); value["name"] = "system_server"
        self.assertEqual(audit.task_role(value), "zygote")
        value["zygote_startup"]["argv"] = ["system_server"]
        self.assertIsNone(audit.task_role(value))

    def test_system_server_requires_original_executable_and_verified_arguments(self):
        value = task(); identity = value["zygote_startup"]
        identity.update(comm="system_server", argv=["system_server"])
        self.assertEqual(audit.task_role(value), "system_server")
        identity["argv_verified"] = False
        self.assertIsNone(audit.task_role(value))

    def test_terminal_task_cannot_become_live_system_server(self):
        value = task(); value["zygote_startup"].update(comm="system_server", argv=["system_server"])
        value["exit_state"] = 16
        self.assertIsNone(audit.task_role(value))


class BindingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.root = Path(self.temp.name)
        self.config = {"expected_epoch_sha256": "b"*64, "expected_native_sha256": "c"*64,
                       "expected_events": [], "proof_file": "c87-proof.json"}
        self.data = b"SYNTHETIC TEST SNAPSHOT - NOT AN ANDROID RESULT"
        self.sha = audit.hashlib.sha256(self.data).hexdigest()
        self.pin = {"directory": str(self.root), "checkpoint_sha256": self.sha, "instructions": 100}
        self.save("current.json", {"checkpoint_sha256": self.sha, "guest": {"instructions": 100,
                  "logical_ram_bytes":2147483648,"counter_instructions_per_tick":64},
                  "error": "", "stop_reason": "instruction_budget", "host_elapsed_ms": 1,
                  "provenance": {"new_uart_input_bytes": 0, "input_checkpoint_sha256": "a"*64}})
        self.save("result.json", {"checkpoint_sha256": self.sha, "instructions": 100})
        self.save("process-outcome.json", {"status": "process_completed", "exit_code": 0, "forced_kill": False, "trigger": None})
        self.contract = {"checkpoint_sha256": self.sha, "instructions": 100, "epoch_sha256": "b"*64, "report_file": "current.json"}
        for name, key in [("current.json","report_sha256"),("result.json","result_sha256"),("process-outcome.json","process_sha256")]:
            self.contract[key] = audit.file_sha(self.root/name)
        self.save("output-contract.json",self.contract)
        self.gate = {"checkpoint_sha256": self.sha, "instructions":100,"source_boundary_verified":True,
                     "system_server_alive_verified":False,"android_boot_completed_verified":False}
        self.save("c46-progress.json",self.gate)
        self.save("kernel-observation.json", {"checkpoint_sha256":self.sha,"instructions":100,"input_unchanged":True,
                  "live_kernel_security":{"selinux_enforcing":True},"tasks":[task()]})
        self.save("runtime-evidence.json", {"checkpoint_sha256":self.sha,"instructions":100,
                  "sys_boot_completed_live":False,"framework_runtime_verified":False})
        self.save("host-policy.json",{"native_binary_sha256":"c"*64})
        self.save("c87-proof.json",{"last_admitted_sha256":self.sha,"last_admitted_instructions":100,
                  "events":[],"phase":0,"waiting":False,"failed":False,"live_peripherals_verified":True})
        (self.root/"current.rvg").write_bytes(self.data)

    def save(self, name, value):
        (self.root/name).write_text(json.dumps(value))

    def tearDown(self):
        self.temp.cleanup()

    def test_valid_budget_boundary_still_does_not_claim_boot_or_ui(self):
        value = audit.audit(self.pin,self.config,True)
        self.assertTrue(value["whole_snapshot_sha256_verified_locally"])
        self.assertFalse(value["original_boot_completed_verified"])
        self.assertFalse(value["usable_UI_verified_by_this_tool"])

    def test_changed_native_report_is_rejected(self):
        self.save("current.json",{})
        with self.assertRaisesRegex(ValueError,"hash mismatch"):
            audit.audit(self.pin,self.config)

    def test_archived_parent_metadata_does_not_claim_a_local_snapshot_hash(self):
        (self.root/"current.rvg").unlink()
        result = audit.audit(self.pin,self.config)
        self.assertFalse(result["whole_snapshot_sha256_verified_locally"])
        self.assertFalse(result["whole_snapshot_file_present"])
        with self.assertRaisesRegex(ValueError,"Complete snapshot"):
            audit.audit(self.pin,self.config,True)

    def test_different_checkpoint_is_rejected(self):
        bad = dict(self.pin,checkpoint_sha256="f"*64)
        with self.assertRaisesRegex(ValueError,"Checkpoint pin"):
            audit.audit(bad,self.config)

    def test_changed_whole_snapshot_is_rejected(self):
        (self.root/"current.rvg").write_bytes(b"modified")
        with self.assertRaisesRegex(ValueError,"Whole snapshot"):
            audit.audit(self.pin,self.config,True)

    def test_forced_native_stop_is_rejected_even_when_report_hashes_match(self):
        value = json.loads((self.root/"process-outcome.json").read_text())
        value.update(forced_kill=True,exit_code=-9)
        self.save("process-outcome.json",value)
        self.contract["process_sha256"]=audit.file_sha(self.root/"process-outcome.json")
        self.save("output-contract.json",self.contract)
        with self.assertRaisesRegex(ValueError,"Unclean native"):
            audit.audit(self.pin,self.config)

    def test_unexpected_new_input_is_rejected_with_consistent_report_hash(self):
        value = json.loads((self.root/"current.json").read_text())
        value["provenance"]["new_uart_input_bytes"]=1
        self.save("current.json",value)
        self.contract["report_sha256"]=audit.file_sha(self.root/"current.json")
        self.save("output-contract.json",self.contract)
        with self.assertRaisesRegex(ValueError,"Unexpected new guest input"):
            audit.audit(self.pin,self.config)

    def test_different_native_is_rejected(self):
        self.save("host-policy.json",{"native_binary_sha256":"d"*64})
        with self.assertRaisesRegex(ValueError,"native binary"):
            audit.audit(self.pin,self.config)

    def test_fabricated_boot_without_live_process_is_rejected(self):
        self.gate.update(android_boot_completed_verified=True,system_server_alive_verified=True)
        self.save("c46-progress.json",self.gate)
        with self.assertRaisesRegex(ValueError,"SystemServer gate"):
            audit.audit(self.pin,self.config)

    def test_lost_or_changed_inherited_event_is_rejected(self):
        self.config["expected_events"]=[{"actual_native_event":{"sha256":"e"*64}}]
        with self.assertRaisesRegex(ValueError,"Event ledger"):
            audit.audit(self.pin,self.config)


if __name__ == "__main__":
    unittest.main()
