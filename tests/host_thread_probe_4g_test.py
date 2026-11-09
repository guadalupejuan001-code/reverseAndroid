from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
ORIGINAL=ROOT/"tools/reverseandroid-thread-probe.mjs"
DERIVED=ROOT/"extensions/host-thread-probe/reverseandroid-thread-probe-4g.mjs"

class HostThreadProbe4GTest(unittest.TestCase):
    def test_only_explicit_host_and_readonly_target_selection_changes(self):
        original=ORIGINAL.read_text()
        derived=DERIVED.read_text()
        header="// C91D2 host-only derived probe: only the two snapshot-capacity bounds are extended to the already validated 4GiB host profile.\n"
        preload="import './reverseandroid-host-transport-preload.mjs';\n"
        self.assertTrue(derived.startswith(header+preload))
        body=derived[len(header+preload):]
        expected=original.replace("2147483648","4026531840")
        expected=expected.replace(
            "options.copyProcesses&&(/^(cp|cppreopts\\.sh)$/.test(t.name)||/^preopt2cache/.test(t.name))",
            "options.copyProcesses&&(/^(cp|mv|fsync|cppreopts\\.sh)$/.test(t.name)||/^preopt2cache/.test(t.name))")
        self.assertEqual(body,expected)
        self.assertEqual(original.count("2147483648"),2)
        self.assertEqual(body.count("4026531840"),2)
        self.assertEqual(derived.count("reverseandroid-host-transport-preload.mjs"),1)
        self.assertIn("original_observer_file_unchanged:true",body)
        self.assertIn("read_only:true",body)

if __name__=="__main__":unittest.main()
