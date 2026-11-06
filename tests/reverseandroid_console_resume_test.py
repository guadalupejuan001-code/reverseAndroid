import copy
import importlib.util
import json
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('resume', ROOT / 'scripts/restore-console-campaign.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ResumeTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.tmp.name)
        self.source = self.root / 'source'
        self.source.mkdir()
        proof = json.loads((ROOT / 'docs/evidence/c82/stage3-producer-boundary.json').read_text())['actual_experiment_state']
        self.state = copy.deepcopy(proof)
        self.console = self.root / 'console.bin'
        self.console.write_bytes(b'original prior console bytes\r\n')
        import hashlib
        digest = hashlib.sha256(self.console.read_bytes()).hexdigest()
        self.state['combined_original_console_bytes_sha256'] = digest
        self.plan = {'source_checkpoint_sha256': proof['last_admitted_sha256'],
                     'source_instructions': proof['last_admitted_instructions'],
                     'inherited_phase': proof['phase'], 'inherited_waiting': proof['waiting'],
                     'inherited_events': copy.deepcopy(proof['events']), 'inherited_console_SHA256': digest}
        self.contract = {'checkpoint_sha256': proof['last_admitted_sha256'], 'instructions': proof['last_admitted_instructions']}
        self.gate = {**self.contract, 'source_boundary_verified': True}
        self.reports = self.root / 'reports'

    def tearDown(self):
        self.tmp.cleanup()

    def run_restore(self):
        for name, value in [('output-contract.json', self.contract), ('c46-progress.json', self.gate), ('c82-proof.json', self.state)]:
            (self.source / name).write_text(json.dumps(value))
        return module.restore_campaign(self.source, 'c82-proof.json', self.console, self.reports, self.plan)

    def rejected_without_reports(self):
        with self.assertRaises(AssertionError):
            self.run_restore()
        self.assertFalse(self.reports.exists())

    def test_exact_ledger_wait_and_entire_console_are_preserved_without_input(self):
        result = self.run_restore()
        self.assertEqual(result['events'], self.state['events'])
        self.assertEqual((self.reports / 'console-all.bin').read_bytes(), self.console.read_bytes())
        self.assertTrue(result['waiting'])
        self.assertFalse(result['inherited_native_events_replayed'])

    def test_one_corrupt_console_byte_is_rejected_before_writes(self):
        self.console.write_bytes(self.console.read_bytes() + b'!')
        self.rejected_without_reports()

    def test_crossed_source_or_count_is_rejected(self):
        self.contract['instructions'] += 1
        self.rejected_without_reports()

    def test_duplicate_ledger_event_is_rejected_even_if_plan_copies_it(self):
        self.state['events'].append(copy.deepcopy(self.state['events'][0]))
        self.plan['inherited_events'] = copy.deepcopy(self.state['events'])
        self.rejected_without_reports()

    def test_completed_campaign_is_never_restarted(self):
        self.state['complete'] = True
        self.rejected_without_reports()


if __name__ == '__main__':
    unittest.main()
