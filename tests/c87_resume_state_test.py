import copy
import hashlib
import importlib.util
import json
import pathlib
import tempfile
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('c87_resume',ROOT/'scripts/prepare-c87-resume-state.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
sha=lambda b:hashlib.sha256(b).hexdigest()


class BoundedResumeTest(unittest.TestCase):
    def fixture(self,root):
        source=root/'source';source.mkdir();metadata=root/'metadata';metadata.mkdir()
        save=lambda p,d:p.write_text(json.dumps(d,indent=2)+'\n')
        native='957f22e2c8c8d3591029529855c733a05fc5c468e9a6e881ff5238bea6ff102f'
        data=b'explicit unit fixture, not a real RVG'
        (source/'current.rvg').write_bytes(data);checkpoint=sha(data);epoch='test-original-epoch'
        events=[dict(phase=n,actual_native_event=dict(bytes=n+1,sha256=str(n)*64)) for n in range(7)]
        raw=b'unit-test authenticated base console\n';base=root/'base.bin';base.write_bytes(raw)
        items=[]
        for cycle,count in [('c84',4),('c85',3),('c86',8)]:
            for n in range(1,count+1):
                alias=f'{cycle}-console-{n}';b=metadata/alias/'evidence'/f'{cycle}-{n}';b.mkdir(parents=True)
                part=f'unit-{cycle}-{n}\n'.encode();raw+=part
                proof=dict(combined_original_console_bytes_sha256=sha(raw));save(b/f'{cycle}-proof.json',proof)
                (b/'console-new.txt').write_bytes(part);(b/'source-commit.txt').write_text(cycle+'-test-commit\n')
                save(b/'c46-progress.json',dict(source_boundary_verified=True))
                items.append(dict(cycle=cycle,stage=n,artifact_alias=alias,execution_commit=cycle+'-test-commit',
                                  control_proof_SHA256=sha((b/f'{cycle}-proof.json').read_bytes()),
                                  new_console_SHA256=sha(part),combined_console_SHA256=sha(raw)))
        state=dict(last_admitted_sha256=checkpoint,last_admitted_instructions=109147727998,
                   phase=7,waiting=False,events=events,hardware_commands_completed=True,live_peripherals_verified=True,
                   failed=False,complete=False,native_binary_SHA256=native,original_admission_and_codecs_unchanged=True,
                   explicit_IC_IVAU_correction=True,IC_IVAU_validation_run=42221035217,fresh_power_cycle=False,
                   combined_original_console_bytes_sha256=sha(raw),prior_actual_failed_input_preserved=True)
        save(source/'c86-proof.json',state)
        save(source/'output-contract.json',dict(checkpoint_sha256=checkpoint,instructions=109147727998,epoch_sha256=epoch))
        save(source/'c46-progress.json',dict(checkpoint_sha256=checkpoint,instructions=109147727998,source_boundary_verified=True))
        save(source/'process-outcome.json',dict(status='process_completed',exit_code=0,forced_kill=False))
        save(source/'kernel-observation.json',dict(live_kernel_security=dict(selinux_enforcing=True)))
        save(source/'host-policy.json',dict(native_binary_sha256=native))
        plan=dict(source_checkpoint_sha256=checkpoint,source_instructions=109147727998,source_epoch_SHA256=epoch,
                  source_control_proof_SHA256=sha((source/'c86-proof.json').read_bytes()),source_stage=8,
                  expected_native_SHA256=native,inherited_phases=[{} for _ in range(7)],inherited_source_events=events,
                  source_base_C83_console_SHA256=sha(base.read_bytes()),source_console_stages=items,
                  source_combined_console_bytes=len(raw),source_combined_console_SHA256=sha(raw),
                  initial_instruction_budget=1000000000,continuation_instruction_budget=1000000000,new_UART_input_planned=False)
        return source,base,metadata,root/'reports',plan

    def test_exact_parent_and_all_seven_events_survive_without_input_or_snapshot_changes(self):
        with tempfile.TemporaryDirectory() as tmp:
            args=self.fixture(pathlib.Path(tmp));source,base,metadata,reports,plan=args
            original=(source/'c86-proof.json').read_bytes();snapshot=(source/'current.rvg').read_bytes()
            result=module.prepare(*args);state=json.loads((reports/'control-state.json').read_text())
            self.assertEqual((reports/'source-c86-proof.json').read_bytes(),original)
            self.assertEqual((source/'c86-proof.json').read_bytes(),original)
            self.assertEqual((source/'current.rvg').read_bytes(),snapshot)
            self.assertEqual(state['events'],plan['inherited_source_events'])
            self.assertEqual(state['phase'],7);self.assertIs(state['waiting'],False)
            self.assertEqual(result['new_UART_input_bytes'],0)
            self.assertTrue(state['pending_C87_new_admission'])

    def test_refuses_the_observed_unsafe_2B_budget_or_any_new_UART(self):
        for key,value in [('continuation_instruction_budget',2000000000),('initial_instruction_budget',2000000000),('new_UART_input_planned',True)]:
            with self.subTest(key=key),tempfile.TemporaryDirectory() as tmp:
                args=list(self.fixture(pathlib.Path(tmp)));args[-1][key]=value
                with self.assertRaises(AssertionError):module.prepare(*args)
                self.assertFalse((args[3]/'control-state.json').exists())

    def test_rejects_waiting_incomplete_foreign_or_unadmitted_sources(self):
        cases=[('c86-proof.json','waiting',True),('c86-proof.json','phase',6),
               ('c86-proof.json','hardware_commands_completed',False),('c86-proof.json','live_peripherals_verified',False),
               ('c86-proof.json','events',[]),('c86-proof.json','failed',True),('c86-proof.json','complete',True),
               ('process-outcome.json','forced_kill',True),('process-outcome.json','exit_code',137),
               ('c46-progress.json','source_boundary_verified',False),('host-policy.json','native_binary_sha256','wrong')]
        for file,key,value in cases:
            with self.subTest(file=file,key=key),tempfile.TemporaryDirectory() as tmp:
                args=list(self.fixture(pathlib.Path(tmp)));p=args[0]/file;d=json.loads(p.read_text());d[key]=value;p.write_text(json.dumps(d)+'\n')
                if file=='c86-proof.json':args[-1]['source_control_proof_SHA256']=sha(p.read_bytes())
                with self.assertRaises(AssertionError):module.prepare(*args)

    def test_corrupt_snapshot_console_or_security_cannot_be_adopted(self):
        for kind in ['snapshot','base_console','part','policy','old_proof']:
            with self.subTest(kind=kind),tempfile.TemporaryDirectory() as tmp:
                args=list(self.fixture(pathlib.Path(tmp)));source,base,metadata,reports,plan=args
                if kind=='snapshot':(source/'current.rvg').write_bytes(b'changed')
                if kind=='base_console':base.write_bytes(b'changed')
                if kind=='part':next(metadata.rglob('console-new.txt')).write_bytes(b'changed')
                if kind=='policy':(source/'kernel-observation.json').write_text(json.dumps(dict(live_kernel_security=dict(selinux_enforcing=False))))
                if kind=='old_proof':(source/'c86-proof.json').write_bytes(b'{}')
                with self.assertRaises(AssertionError):module.prepare(*args)


if __name__=='__main__':unittest.main()
