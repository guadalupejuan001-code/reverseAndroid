import importlib.util
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("c91f0",ROOT/"scripts/c91f0_priority_control.py")
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class C91F0Test(unittest.TestCase):
    def test_reply_parser(self):
        raw=b"REVERSE_C91F0_DONE CHANGED_87 SEEN_90 BAD_0\nREVERSE_C91F0_RESULT RC_0\nREVERSE_C91F0_ACTIVITY_REGISTERED AFTER_3\nREVERSE_C91F0_BOOTPROP_absent\n"
        r=m.parse_reply(raw)
        self.assertTrue(r["guard_succeeded"]);self.assertEqual(r["changed"],87)
        self.assertTrue(r["activity_registered"]);self.assertEqual(r["activity_after_seconds"],3)
        self.assertEqual(r["boot_property"],"absent")

    def test_cfs_ratio_detects_normalization(self):
        before={"tasks":[{"pid":1,"name":"mv","accounting":{"sum_exec_runtime":"1000","vruntime":"100"}}]}
        after={"tasks":[{"pid":1,"name":"mv","accounting":{"sum_exec_runtime":"2000","vruntime":"1100"}}]}
        rows,med=m.cfs_delta_ratios(before,after)
        self.assertEqual(len(rows),1);self.assertEqual(med,1.0)

    def test_plan_is_minimal(self):
        p=m.read(m.PLAN);m.validate_plan(p)
        self.assertFalse(p["guest_property_injection"]);self.assertFalse(p["guest_snapshot_patching"])
        self.assertFalse(p["GPU_or_QSSD_modified"]);self.assertFalse(p["promotion_authority"])

if __name__=="__main__":unittest.main()
