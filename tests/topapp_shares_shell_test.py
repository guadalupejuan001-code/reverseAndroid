#!/usr/bin/env python3
"""Run the exact inner guest command against temporary proc/cgroup fixtures."""
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import sys

REPO = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('nested_syntax', REPO/'scripts/console_command_syntax.py')
syntax = importlib.util.module_from_spec(spec); spec.loader.exec_module(syntax)
command = (REPO/'state/experiments/topapp-shares-boost-input.txt').read_text()
body = syntax.validate_nested_console_command(command)
sys.path.insert(0,str(REPO/'scripts'))
spec2 = importlib.util.spec_from_file_location('shares_control', REPO/'scripts/run-topapp-shares-control.py')
control = importlib.util.module_from_spec(spec2); spec2.loader.exec_module(control)


class GuestSharesGuard(unittest.TestCase):
    def run_body(self, shell, **changes):
        with tempfile.TemporaryDirectory(prefix='android-shares-guard-') as temporary:
            root = Path(temporary); task = root/'proc/5629/task/5922'; task.mkdir(parents=True)
            controller = root/'cpu/top-app'; controller.mkdir(parents=True)
            name = changes.get('name', 'Jit thread pool')
            (task/'comm').write_text(name+'\n')
            (task/'status').write_text(f"Tgid:\t{changes.get('tgid',5629)}\nUid:\t{changes.get('uid',0)}\t0\t0\t0\n")
            fields = ['S']+['0']*16; fields[16] = str(changes.get('nice',0))
            (task/'stat').write_text('5922 ('+name+') '+' '.join(fields)+'\n')
            (root/'proc/5629/cgroup').write_text(changes.get('cgroup','2:cpu:/top-app\n0::/uid_0/pid_5629\n'))
            shares = controller/'cpu.shares'; shares.write_text(str(changes.get('shares',1024))+'\n')
            exact = body.replace('/proc/',str(root/'proc')+'/').replace('/dev/cpuctl/',str(root/'cpu')+'/')
            result = subprocess.run([shell,'-c',exact],capture_output=True,text=True)
            return result, shares.read_text().strip()

    def test_exact_nested_command_and_plan_binding(self):
        import hashlib
        plan = json.loads((REPO/'state/experiments/topapp-shares-boost-input-plan.json').read_text())
        raw = command.encode('ascii')
        self.assertEqual(hashlib.sha256(raw).hexdigest(),plan['input_sha256'])
        self.assertEqual(len(raw),plan['input_bytes'])
        self.assertNotIn('REVERSE_CFS_TOPAPP_BOOST_DONE SHARES_16384',command)

    def test_real_shells_change_only_verified_controller(self):
        for shell in ['sh','bash']:
            for controllers in ['cpu','cpu,cpuacct','cpuacct,cpu']:
                with self.subTest(shell=shell,controllers=controllers):
                    result, shares = self.run_body(shell,cgroup=f'2:{controllers}:/top-app\n')
                    self.assertEqual(result.returncode,0,result.stderr)
                    self.assertEqual(shares,'16384')
                    self.assertEqual(result.stdout.strip(),'REVERSE_CFS_TOPAPP_BOOST_DONE SHARES_16384')

    def test_identity_priority_membership_and_old_value_fail_before_write(self):
        cases = [({'name':'another thread'},51),({'tgid':55},52),({'uid':1000},52),
                 ({'nice':19},53),({'cgroup':'2:cpu:/background\n'},54),
                 ({'cgroup':'0::/top-app\n'},55),({'shares':2048},56)]
        for shell in ['sh','bash']:
            for change, code in cases:
                with self.subTest(shell=shell,change=change):
                    result, shares = self.run_body(shell,**change)
                    self.assertEqual(result.returncode,code,result.stderr)
                    self.assertEqual(shares,str(change.get('shares',1024)))
                    self.assertNotIn('BOOST_DONE',result.stdout)

    def test_real_shells_restore_only_trial_value(self):
        exact = syntax.validate_nested_console_command(control.restore_command())
        for shell in ['sh','bash']:
            for initial, expected, status in [(16384,1024,0),(1024,1024,61),(2048,2048,61)]:
                with self.subTest(shell=shell,initial=initial):
                    with tempfile.TemporaryDirectory(prefix='android-shares-restore-') as temporary:
                        f = Path(temporary)/'cpu.shares'; f.write_text(str(initial)+'\n')
                        source = exact.replace('/dev/cpuctl/top-app/cpu.shares',str(f))
                        result = subprocess.run([shell,'-c',source],capture_output=True,text=True)
                        self.assertEqual(result.returncode,status,result.stderr)
                        self.assertEqual(f.read_text().strip(),str(expected))

    def test_echo_is_not_a_reply_and_failure_overrides_success(self):
        self.assertFalse(control.replies(command.encode(),'BOOST')['success'])
        self.assertFalse(control.replies(control.restore_command().encode(),'RESTORE')['success'])
        raw = b'REVERSE_CFS_TOPAPP_BOOST_DONE SHARES_16384\nREVERSE_CFS_TOPAPP_BOOST_RESULT RC_0\n'
        self.assertTrue(control.replies(raw,'BOOST')['success'])
        self.assertTrue(control.replies(raw+b'REVERSE_CFS_TOPAPP_BOOST_RESULT RC_56\n','BOOST')['failed'])
        self.assertFalse(control.replies(raw+b'REVERSE_CFS_TOPAPP_BOOST_RESULT RC_56\n','BOOST')['success'])


if __name__ == '__main__':
    unittest.main()
