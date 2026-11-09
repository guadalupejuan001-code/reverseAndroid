import importlib.util
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("f1",ROOT/"scripts/c91f1_priority_control.py")
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class C91F1Test(unittest.TestCase):
  def test_reply(self):
    r=m.parse_reply(b"REVERSE_C91F1_DONE N_87 BAD_0\nREVERSE_C91F1_RESULT RC_0\n")
    self.assertTrue(r["guard_succeeded"]);self.assertEqual(r["changed"],87);self.assertEqual(r["bad"],0)
    self.assertFalse(m.parse_reply(b"REVERSE_C91F1_RESULT RC_61\n")["guard_succeeded"])
  def test_scheduling_stats(self):
    data={"all_thread_scheduling":[
      {"pid":7354,"name":"system_server","threads":[{"pid":7354,"static_prio":120,"prio":120,"normal_prio":120,"policy":0,"on_rq":1,"on_cpu":0,"accounting":{"sum_exec_runtime":"100","vruntime":"200"}}]},
      {"pid":1,"name":"fsync","threads":[{"pid":1,"static_prio":100,"on_rq":1,"on_cpu":0,"accounting":{"sum_exec_runtime":"1","vruntime":"1"}}]},
      {"pid":2,"name":"cppreopts.sh","threads":[{"pid":2,"static_prio":120,"on_rq":0,"on_cpu":0,"accounting":{"sum_exec_runtime":"1","vruntime":"1"}}]}]}
    s=m.scheduling_stats(data);self.assertEqual(s["nice_minus20"],1);self.assertEqual(s["nice0"],1)
    self.assertEqual(s["system_server"]["sum_exec_runtime"],100)
  def test_plan(self):
    p=m.read(m.PLAN);m.validate_plan(p);self.assertFalse(p["promotion_authority"])
    self.assertEqual(p["input_bytes"],456)

if __name__=="__main__":unittest.main()
