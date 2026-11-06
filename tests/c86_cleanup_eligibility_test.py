import copy
import json
import pathlib
import sys
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from c86_cleanup_eligibility import verify


class CleanupEligibilityTest(unittest.TestCase):
    def inputs(self):
        read=lambda p:json.loads((ROOT/p).read_text())
        plan=read('state/experiments/c86-plan.json')
        transport=read('state/experiments/c85-stage-3-transport.json')
        contract=transport['source_contract']
        gate=transport['progress']
        host=dict(status='process_completed',exit_code=0,forced_kill=False)
        kernel=dict(live_kernel_security=dict(selinux_enforcing=True))
        state=dict(last_admitted_sha256=contract['checkpoint_sha256'],last_admitted_instructions=contract['instructions'],
                   phase=5,waiting=False,events=copy.deepcopy(plan['inherited_source_events']),failed=False,complete=False,
                   explicit_IC_IVAU_correction=True,IC_IVAU_validation_run=42221035217,
                   native_binary_SHA256=plan['expected_extended_native_SHA256'])
        threads=read(plan['source_thread_proof_file'])
        return [contract,gate,host,kernel,state,threads,plan]

    def test_exact_original_thread_evidence_authorizes_only_relative_minus19(self):
        proof=verify(*self.inputs())
        self.assertEqual(proof['guarded_relative_renice'],-19)
        self.assertEqual(proof['source_events_preserved'],5)

    def test_rejects_stale_unadmitted_waiting_completed_or_foreign_worker(self):
        cases=[(0,['checkpoint_sha256'],'0'*64),(1,['source_boundary_verified'],False),
               (1,['system_server_alive_verified'],True),(1,['android_boot_completed_verified'],True),
               (2,['exit_code'],137),(2,['forced_kill'],True),(3,['live_kernel_security','selinux_enforcing'],False),
               (4,['waiting'],True),(4,['phase'],4),(4,['failed'],True),(4,['complete'],True),
               (4,['native_binary_SHA256'],'0'*64),(5,['input_unchanged'],False),
               (5,['groups',0,'thread_list_verified'],False),(5,['groups',0,'thread_count'],3),
               (5,['groups',0,'threads',0,'exit_state'],0),(5,['groups',0,'threads',0,'signals','exit_code_raw'],'0'),
               (5,['groups',0,'threads',1,'tgid'],999),(5,['groups',0,'threads',1,'name'],'other'),
               (5,['groups',0,'threads',1,'priority'],120),(5,['groups',0,'threads',1,'state'],1),
               (5,['groups',0,'threads',1,'scheduling','credentials','uid'],'1000'),
               (5,['groups',0,'threads',1,'signals','ptrace_raw'],'1'),
               (5,['groups',0,'threads',1,'signals','group_exit_code_raw'],'0'),
               (5,['groups',0,'threads',1,'kernel_stack'],[])]
        for index,path,value in cases:
            with self.subTest(path=path):
                args=self.inputs();target=args[index]
                for key in path[:-1]:target=target[key]
                target[path[-1]]=value
                with self.assertRaises(AssertionError):verify(*args)


if __name__=='__main__':unittest.main()
