#!/usr/bin/env python3
"""Execute the exact guest control bodies against process/cgroup fixtures."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

REPO=Path(__file__).resolve().parents[1];sys.path.insert(0,str(REPO/'scripts'))
spec=importlib.util.spec_from_file_location('foreground',REPO/'scripts/run-foreground-shares-control.py')
control=importlib.util.module_from_spec(spec);spec.loader.exec_module(control)
body=control.validate_nested_console_command(control.boost_command(6512))


class ForegroundGuestControl(unittest.TestCase):
    def run_boost(self,shell,**changes):
        with tempfile.TemporaryDirectory(prefix='foreground-shares-') as temporary:
            root=Path(temporary);proc=root/'proc/6512';proc.mkdir(parents=True)
            (proc/'comm').write_text(changes.get('name','system_server')+'\n')
            (proc/'status').write_text(f"Tgid:\t{changes.get('tgid',6512)}\nUid:\t{changes.get('uid',1000)}\t1000\t1000\t1000\n")
            (proc/'cgroup').write_text(changes.get('cgroup','2:cpu,cpuacct:/foreground\n'))
            fore=root/'cpu/foreground';top=root/'cpu/top-app';fore.mkdir(parents=True);top.mkdir(parents=True)
            (fore/'cpu.shares').write_text(str(changes.get('shares',1024))+'\n')
            (top/'cpu.shares').write_text(str(changes.get('top_shares',16384))+'\n')
            source=body.replace('/proc/',str(root/'proc')+'/').replace('/dev/cpuctl/',str(root/'cpu')+'/')
            result=subprocess.run([shell,'-c',source],capture_output=True,text=True)
            return result,(fore/'cpu.shares').read_text().strip(),(top/'cpu.shares').read_text().strip()

    def test_real_shells_request_only_guarded_foreground_weight(self):
        for shell in ['sh','bash']:
            for controllers in ['cpu','cpu,cpuacct','cpuacct,cpu']:
                with self.subTest(shell=shell,controllers=controllers):
                    result,fore,top=self.run_boost(shell,cgroup=f'2:{controllers}:/foreground\n')
                    self.assertEqual(result.returncode,0,result.stderr)
                    self.assertEqual((fore,top),('16384','16384'))
                    self.assertEqual(result.stdout.strip(),'REVERSE_CFS_FOREGROUND_BOOST_DONE SHARES_16384')

    def test_identity_membership_and_existing_weights_fail_before_write(self):
        cases=[({'name':'main'},81),({'tgid':7},82),({'uid':0},82),({'cgroup':'2:cpu:/top-app\n'},83),
               ({'cgroup':'0::/foreground\n'},84),({'shares':2048},85),({'top_shares':1024},86)]
        for shell in ['sh','bash']:
            for changes,status in cases:
                with self.subTest(shell=shell,changes=changes):
                    result,fore,top=self.run_boost(shell,**changes)
                    self.assertEqual(result.returncode,status,result.stderr)
                    self.assertEqual(fore,str(changes.get('shares',1024)))
                    self.assertEqual(top,str(changes.get('top_shares',16384)))
                    self.assertNotIn('BOOST_DONE',result.stdout)

    def test_real_shells_restore_both_groups_only_from_verified_trial(self):
        exact=control.validate_nested_console_command(control.restore_command())
        for shell in ['sh','bash']:
            for a,b,status in [(16384,16384,0),(1024,16384,91),(16384,1024,91),(2048,2048,91)]:
                with self.subTest(shell=shell,a=a,b=b):
                    with tempfile.TemporaryDirectory(prefix='foreground-restore-') as temporary:
                        root=Path(temporary)
                        for name,value in [('foreground',a),('top-app',b)]:
                            (root/name).mkdir();(root/name/'cpu.shares').write_text(str(value)+'\n')
                        result=subprocess.run([shell,'-c',exact.replace('/dev/cpuctl/',str(root)+'/')],capture_output=True,text=True)
                        self.assertEqual(result.returncode,status,result.stderr)
                        self.assertEqual((root/'foreground/cpu.shares').read_text().strip(),str(1024 if status==0 else a))
                        self.assertEqual((root/'top-app/cpu.shares').read_text().strip(),str(1024 if status==0 else b))

    def test_input_echo_and_failed_reply_cannot_authorize_restore(self):
        self.assertFalse(control.reply(control.boost_command(6512),'BOOST')['success'])
        self.assertFalse(control.reply(control.restore_command(),'RESTORE')['success'])
        good='REVERSE_CFS_FOREGROUND_BOOST_DONE SHARES_16384\nREVERSE_CFS_FOREGROUND_BOOST_RESULT RC_0\n'
        self.assertTrue(control.reply(good,'BOOST')['success'])
        bad=control.reply(good+'REVERSE_CFS_FOREGROUND_BOOST_RESULT RC_85\n','BOOST')
        self.assertTrue(bad['failed']);self.assertFalse(bad['success'])
        with self.assertRaises(AssertionError):control.boost_command('6512; injected')


if __name__=='__main__':unittest.main()
