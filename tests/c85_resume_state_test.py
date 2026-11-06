import copy
import hashlib
import importlib.util
import json
import pathlib
import tempfile
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("c85_resume",ROOT/"scripts/prepare-c85-resume-state.py")
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
sha=lambda b:hashlib.sha256(b).hexdigest()


class ResumeGuards(unittest.TestCase):
    def fixture(self, root, phase=4, waiting=True):
        source=root/"source";source.mkdir();metadata=root/"metadata";reports=root/"reports"
        snapshot=b"original synthetic checkpoint bytes";digest=sha(snapshot);count=99547727998
        plan=json.loads((ROOT/"state/experiments/c85-plan.json").read_text())
        events=copy.deepcopy(plan["inherited_source_events"])
        if phase==5 and waiting or phase==6:
            events.append({"phase":5,"actual_native_event":{"sha256":plan["phases"][5]["input_sha256"],"bytes":932}})
        state={"corrective_campaign":"C84","prior_C83_campaign_failed":True,"prior_failed_control_proof_preserved":True,
            "receiver_and_base64_data_not_retransferred":True,"original_admission_and_codecs_unchanged":True,
            "failed":False,"complete":False,"events":events,"phase":phase,"waiting":waiting,
            "last_admitted_sha256":digest,"last_admitted_instructions":count}
        raw=b"authentic fixture base console with old failed command retained\x00";base=root/"base.bin";base.write_bytes(raw)
        plan.update(source_checkpoint_sha256=digest,source_instructions=count,inherited_source_events=events,
            inherited_source_phase=phase,inherited_source_waiting=waiting,source_base_C83_console_SHA256=sha(raw))
        stages=[]
        for n in range(1,5):
            alias=f"c84-console-{n}";b=metadata/alias/"evidence"/f"c84-{n}";b.mkdir(parents=True)
            part=f"unchanged original console stage {n}\n".encode();raw+=part
            proof=json.dumps({"combined_original_console_bytes_sha256":sha(raw)}).encode()
            (b/"c84-proof.json").write_bytes(proof);(b/"console-new.txt").write_bytes(part)
            (b/"source-commit.txt").write_text(plan["source_commit"])
            (b/"c46-progress.json").write_text(json.dumps({"source_boundary_verified":True}))
            stages.append({"stage":n,"artifact_alias":alias,"control_proof_SHA256":sha(proof),
                "new_console_SHA256":sha(part),"combined_console_SHA256":sha(raw)})
        state["combined_original_console_bytes_sha256"]=sha(raw)
        original=json.dumps(state).encode()
        plan.update(source_control_proof_SHA256=sha(original),source_combined_console_SHA256=sha(raw),
            source_combined_console_bytes=len(raw),source_console_stages=stages)
        (source/"current.rvg").write_bytes(snapshot);(source/plan["source_proof_file"]).write_bytes(original)
        for name,value in {"output-contract.json":{"checkpoint_sha256":digest,"instructions":count},
            "c46-progress.json":{"checkpoint_sha256":digest,"instructions":count,"source_boundary_verified":True},
            "process-outcome.json":{"status":"process_completed","exit_code":0,"forced_kill":False},
            "kernel-observation.json":{"live_kernel_security":{"selinux_enforcing":True}},
            "host-policy.json":{"native_binary_sha256":plan["source_native_SHA256"]}}.items():
            (source/name).write_text(json.dumps(value))
        return source,base,metadata,reports,plan,state,snapshot

    def test_pending_activation_inspection_and_completed_hardware_keep_exact_ledger(self):
        for phase,waiting in [(4,True),(5,False),(5,True),(6,False)]:
            with self.subTest(phase=phase,waiting=waiting),tempfile.TemporaryDirectory() as d:
                source,base,meta,reports,plan,state,snapshot=self.fixture(pathlib.Path(d),phase,waiting)
                result=module.prepare(source,base,meta,reports,plan);restored=module.read(reports/"control-state.json")
                self.assertEqual(restored["events"],state["events"]);self.assertEqual(restored["phase"],phase)
                self.assertIs(restored["waiting"],waiting);self.assertEqual(result["new_UART_input_bytes"],0)
                self.assertEqual((source/"current.rvg").read_bytes(),snapshot)
                self.assertEqual((reports/"source-c84-proof.json").read_bytes(),(source/plan["source_proof_file"]).read_bytes())
                self.assertEqual(restored["source"],str(source.resolve()))

    def test_corrupt_snapshot_or_console_rejected_before_any_state_is_written(self):
        for kind in ["snapshot","base","increment"]:
            with self.subTest(kind=kind),tempfile.TemporaryDirectory() as d:
                source,base,meta,reports,plan,_,_=self.fixture(pathlib.Path(d))
                path=source/"current.rvg" if kind=="snapshot" else base if kind=="base" else meta/"c84-console-2/evidence/c84-2/console-new.txt"
                path.write_bytes(path.read_bytes()+b"corruption")
                with self.assertRaises(AssertionError):module.prepare(source,base,meta,reports,plan)
                self.assertFalse(reports.exists())

    def test_no_ledger_edits_or_waiting_reset_allowed(self):
        for field,value in [("events",[]),("waiting",False),("failed",True)]:
            with self.subTest(field=field),tempfile.TemporaryDirectory() as d:
                source,base,meta,reports,plan,state,_=self.fixture(pathlib.Path(d));state[field]=value
                data=json.dumps(state).encode();(source/plan["source_proof_file"]).write_bytes(data)
                plan["source_control_proof_SHA256"]=sha(data)
                with self.assertRaises(AssertionError):module.prepare(source,base,meta,reports,plan)
                self.assertFalse(reports.exists())

    def test_incorrect_proof_native_or_enforcement_rejected(self):
        for kind in ["proof","native","enforcement","host"]:
            with self.subTest(kind=kind),tempfile.TemporaryDirectory() as d:
                source,base,meta,reports,plan,_,_=self.fixture(pathlib.Path(d))
                if kind=="proof":plan["source_control_proof_SHA256"]="0"*64
                else:
                    path=source/({"native":"host-policy.json","enforcement":"kernel-observation.json","host":"process-outcome.json"}[kind])
                    j=module.read(path)
                    if kind=="native":j["native_binary_sha256"]="0"*64
                    elif kind=="enforcement":j["live_kernel_security"]["selinux_enforcing"]=False
                    else:j["forced_kill"]=True
                    path.write_text(json.dumps(j))
                with self.assertRaises(AssertionError):module.prepare(source,base,meta,reports,plan)
                self.assertFalse(reports.exists())


if __name__=="__main__":unittest.main()
