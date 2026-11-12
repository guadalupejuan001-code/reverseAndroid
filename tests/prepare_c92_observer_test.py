import importlib.util
from pathlib import Path
import unittest
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("c92",root/"scripts/prepare-c92-observer.py")
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class ObserverTest(unittest.TestCase):
 def test_exact_bound_replacement(self):
  s="before "+module.NEEDLE+" after"
  r=module.patch(s)
  self.assertIn("task-list cycle",r)
  self.assertIn("tasks.length<4096",r)
  self.assertNotIn("tasks.length<512",r)
 def test_refuse_unrecognized_and_double_patch(self):
  for s in ("unknown",module.REPLACEMENT,module.NEEDLE+module.NEEDLE):
   with self.assertRaises(AssertionError):module.patch(s)
 def test_imported_observer_unchanged(self):
  s=(root/"tools/android_kernel_observer.mjs").read_text()
  self.assertEqual(s.count(module.NEEDLE),1)
if __name__=="__main__":unittest.main()
