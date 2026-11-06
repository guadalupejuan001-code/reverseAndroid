"""The priority control must reject command echo, wrong owners and stale targets."""
import copy
import importlib.util
from pathlib import Path
import subprocess
import sys
import unittest

ROOT=Path(__file__).parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
spec=importlib.util.spec_from_file_location('control',ROOT/'scripts/run-jit5922-control.py')
control=importlib.util.module_from_spec(spec);spec.loader.exec_module(control)


class GuardTests(unittest.TestCase):
    def setUp(self):
        self.plan={'source_checkpoint_sha256':'a'*64,'zygote_PID':5629,'JIT_TID':5922}
        self.worker={'pid':5922,'tgid':5629,'name':'Jit thread pool','priority':139,'normal_prio':139,
                     'state':0,'scheduling':{'credentials':{'uid':'0'}}}
        self.observation={'input_unchanged':True,'checkpoint_sha256':'a'*64,
                          'groups':[{'pid':5629,'thread_list_verified':True,'threads':[self.worker]}]}
        self.kernel={'checkpoint_sha256':'a'*64,'zygote_launch':{'processes':[
            {'pid':5629,'exe_path':'/system/bin/app_process64','argv_verified':True,'argv':['zygote64'],'exit_state':0}]}}

    def test_exact_actual_source_target_passes(self):
        self.assertEqual(control.verify_target(self.observation,self.kernel,self.plan)['pid'],5922)

    def test_foreign_uid_is_rejected(self):
        self.worker['scheduling']['credentials']['uid']='1000'
        with self.assertRaises(AssertionError):control.verify_target(self.observation,self.kernel,self.plan)

    def test_wrong_owner_is_rejected(self):
        self.worker['tgid']=1
        with self.assertRaises(AssertionError):control.verify_target(self.observation,self.kernel,self.plan)

    def test_renamed_or_already_adjusted_target_is_rejected(self):
        self.worker['name']='main'
        with self.assertRaises(AssertionError):control.verify_target(self.observation,self.kernel,self.plan)
        self.worker.update(name='Jit thread pool',priority=120,normal_prio=120)
        with self.assertRaises(AssertionError):control.verify_target(self.observation,self.kernel,self.plan)

    def test_unbound_observation_is_rejected(self):
        self.observation['checkpoint_sha256']='b'*64
        with self.assertRaises(AssertionError):control.verify_target(self.observation,self.kernel,self.plan)

    def test_actual_input_has_valid_shell_syntax_and_its_echo_is_not_a_reply(self):
        command=ROOT/'state/experiments/jit5922-control-input.txt'
        subprocess.run(['/bin/sh','-n',str(command)],check=True)
        self.assertFalse(control.guard_outcome(command.read_bytes())['guard_result_seen'])

    def test_done_and_successful_real_result_are_both_required(self):
        self.assertFalse(control.guard_outcome(b'REVERSE_JIT5922_CONTROL_RESULT RC_0\n')['guard_succeeded'])
        result=control.guard_outcome(b'REVERSE_JIT5922_CONTROL_DONE NICE_0\nREVERSE_JIT5922_CONTROL_RESULT RC_0\n')
        self.assertTrue(result['guard_succeeded']);self.assertFalse(result['failed'])

    def test_failure_cannot_be_masked_by_a_prior_success(self):
        result=control.guard_outcome(b'REVERSE_JIT5922_CONTROL_DONE NICE_0\nREVERSE_JIT5922_CONTROL_RESULT RC_0\nREVERSE_JIT5922_CONTROL_RESULT RC_42\n')
        self.assertTrue(result['failed']);self.assertFalse(result['guard_succeeded'])


if __name__=='__main__':unittest.main()
