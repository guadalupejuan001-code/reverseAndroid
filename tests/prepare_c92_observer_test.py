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
 def test_original_imported_observer_unchanged(self):
  s=(root/"tools/android_kernel_observer.mjs").read_text()
  self.assertEqual(s.count(module.NEEDLE),1)
 def test_original_readonly_probe_unchanged(self):
  s=(root/"extensions/host-thread-probe/reverseandroid-thread-probe-4g.mjs").read_text()
  self.assertEqual(s.count(module.PROBE_NEEDLE),1)
  r=module.patch_probe(s)
  self.assertIn("derived_observer_verified:true",r)
  self.assertIn("derived_observer_SHA256",r)
  self.assertIn("task_limit,4096",r)
  with self.assertRaises(AssertionError):module.patch_probe(r)
 def test_git_blob_matches_pinned_original(self):
  import json
  raw=(root/"tools/android_kernel_observer.mjs").read_bytes()
  snap=json.loads((root/"state/upstream-snapshot.json").read_text())
  expected=next(x for x in snap["files"] if x["path"]=="tools/android_kernel_observer.mjs")["git_blob_sha1"]
  self.assertEqual(module.git_blob(raw),expected)
if __name__=="__main__":
 unittest.main()
