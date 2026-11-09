import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("c91d0", ROOT / "scripts/c91d0_boot_blocker.py")
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class C91D0Tests(unittest.TestCase):
    def test_pointer_guard(self):
        self.assertFalse(mod.pointer_is_newer_than_stage5({"cycle":"C91","stage":5}))
        self.assertTrue(mod.pointer_is_newer_than_stage5({"cycle":"C91","stage":6}))
        self.assertFalse(mod.pointer_is_newer_than_stage5({"cycle":"C90R","stage":5}))
        self.assertTrue(mod.pointer_is_newer_than_stage5({"cycle":"C92","stage":1}))

    def test_classifier_requires_all_blocker_evidence(self):
        ss={"on_rq":1,"on_cpu":0}
        delta={"sum_exec_runtime":4_010_000}
        console=("BUG: workqueue lockup\n"
                 "in-flight: 5672:rust_binder 7506:drain_vmap_area_work\n")
        tasks=[{"name":"mv","on_rq":1} for _ in range(10)]
        verdict=mod.classify(
            boot_completed=False,ss_task=ss,delta=delta,console=console,
            bootanimation_alive=True,binder={},tasks=tasks)
        self.assertTrue(verdict["H1_supported"])
        verdict=mod.classify(
            boot_completed=False,ss_task=ss,delta=delta,
            console="no lockup",bootanimation_alive=True,binder={},tasks=tasks)
        self.assertFalse(verdict["H1_supported"])


if __name__ == "__main__":
    unittest.main()
