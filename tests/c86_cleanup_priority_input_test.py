import os
import pathlib
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/"scripts"))
from c86_cleanup_priority_input import cleanup_body,command
from console_command_syntax import validate_nested_console_command


class OrdinaryPriorityGuards(unittest.TestCase):
    def run_body(self, *, name="Jit thread pool",uid=0,tgid=562,nice=19,flags=4,absent=False,exit_after=False,already=False):
        with tempfile.TemporaryDirectory() as temp:
            root=pathlib.Path(temp);proc=root/"proc";d=proc/"3387";d.mkdir(parents=True);bin=root/"bin";bin.mkdir()
            (d/"comm").write_text(name+"\n");(d/"status").write_text(f"Name:\t{name}\nTgid:\t{tgid}\nUid:\t{uid}\t{uid}\t{uid}\t{uid}\n")
            tail=["R","1","0","0","0","0",str(flags)]+["0"]*8+["139",str(nice)]
            (d/"stat").write_text("3387 (Jit thread pool) "+" ".join(tail)+"\n")
            if absent:(d/"status").unlink()
            log=root/"renice-args.json"
            # PATH interception is mandatory: this test never renices a host task.
            script='''#!/usr/bin/env python3
import json,pathlib,sys
root=pathlib.Path(__file__).resolve().parents[1]
(root/'renice-args.json').write_text(json.dumps(sys.argv[1:]))
assert sys.argv[1:]==['-n','-19','-p','3387']
p=root/'proc/3387/stat'
'''
            if exit_after:script+='p.unlink()\n'
            else:script+='s=p.read_text();prefix,tail=s.split(") ",1);v=tail.split();v[16]=str(int(v[16])-19);p.write_text(prefix+") "+" ".join(v)+"\\n")\n'
            (bin/"renice").write_text(script);(bin/"renice").chmod(0o700)
            env=dict(os.environ,PATH=str(bin)+os.pathsep+os.environ["PATH"])
            result=subprocess.run(["sh","-c",cleanup_body(proc_root=str(proc))],env=env,capture_output=True,text=True)
            return result,log.read_text() if log.exists() else None

    def test_exact_nested_syntax_and_relative_minus19(self):
        validate_nested_console_command(command())
        r,call=self.run_body();self.assertEqual(r.returncode,0,r.stderr)
        self.assertEqual(call,'["-n", "-19", "-p", "3387"]')
        self.assertIn("PRIORITY_APPLIED RC_0 NICE_0",r.stdout)

    def test_wrong_identity_nice_or_non_exiting_task_never_changes_priority(self):
        for args,code in [({"name":"another thread"},41),({"uid":1000},42),({"tgid":3386},42),({"flags":0},44),({"nice":10},45)]:
            with self.subTest(args=args):
                r,call=self.run_body(**args);self.assertEqual(r.returncode,code,r.stderr);self.assertIsNone(call)

    def test_completed_cleanup_or_already_normal_is_reported_without_renice(self):
        for args,marker in [({"absent":True},"ALREADY_ABSENT"),({"nice":0},"ALREADY_NICE0")]:
            r,call=self.run_body(**args);self.assertEqual(r.returncode,0,r.stderr);self.assertIsNone(call);self.assertIn(marker,r.stdout)

    def test_successful_priority_change_may_naturally_finish_before_post_read(self):
        r,call=self.run_body(exit_after=True);self.assertEqual(r.returncode,0,r.stderr)
        self.assertIsNotNone(call);self.assertIn("RENICED_THEN_ABSENT",r.stdout)


if __name__=="__main__":unittest.main()
