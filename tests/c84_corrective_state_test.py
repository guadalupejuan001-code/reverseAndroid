"""Control-file guards; the original native preflight is required separately."""
import copy
import hashlib
import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
spec = importlib.util.spec_from_file_location('corrective', ROOT / 'scripts/prepare-c84-corrective-state.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class CorrectiveStateTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.tmp.name)
        self.source = self.root / 'source'
        self.source.mkdir()
        self.reports = self.root / 'reports'
        self.plan = json.loads((ROOT / 'state/experiments/c84-plan.json').read_text())
        self.previous = copy.deepcopy(self.plan['inherited_events'])
        self.state = json.loads((ROOT / 'docs/evidence/c83/stage7-producer-boundary.json').read_text())['actual_experiment_state']
        # A small file exercises the control restorer's byte binding. It is
        # deliberately not an executable RVG and never reaches native admission.
        image = b'control-file-unit-test-only'
        (self.source / 'current.rvg').write_bytes(image)
        sha = hashlib.sha256(image).hexdigest()
        self.plan['source_checkpoint_sha256'] = sha
        self.state['last_admitted_sha256'] = sha
        self.console = self.root / 'console.bin'
        self.codes = {name: [0] for name in ['PTS_TRANSFER_RX_0', 'PTS_TRANSFER_DATA_0', 'PTS_TRANSFER_RX_1', 'PTS_TRANSFER_DATA_1', 'PTS_TRANSFER_CAPTURE']}
        self.codes['PTS_TRANSFER_ACTIVATION'] = [1]
        raw = b"sh: syntax error: unexpected ';;'\r\n" + b''.join(f'REVERSE_{key} RC_{values[0]}\r\n'.encode() for key, values in self.codes.items())
        self.console.write_bytes(raw)
        digest = hashlib.sha256(raw).hexdigest()
        self.plan['inherited_console_SHA256'] = digest
        self.state['combined_original_console_bytes_sha256'] = digest
        contract = {'checkpoint_sha256': sha, 'instructions': self.plan['source_instructions']}
        self.write('output-contract.json', contract)
        self.write('c46-progress.json', {**contract, 'source_boundary_verified': True})
        self.write('process-outcome.json', {'status': 'process_completed', 'exit_code': 0, 'forced_kill': False})
        self.write('kernel-observation.json', {'live_kernel_security': {'selinux_enforcing': True}})
        self.write('console-observation.json', {'raw_console_SHA256': digest, 'actual_response_codes': self.codes})
        self.save_state()

    def write(self, name, value):
        (self.source / name).write_text(json.dumps(value, indent=2) + '\n')

    def save_state(self):
        self.write('c83-proof.json', self.state)
        self.original_proof = (self.source / 'c83-proof.json').read_bytes()
        self.plan['source_failed_proof_SHA256'] = hashlib.sha256(self.original_proof).hexdigest()

    def tearDown(self):
        self.tmp.cleanup()

    def restore(self):
        return module.restore_corrective(self.source, self.console, self.reports, self.plan)

    def reject_before_writes(self):
        with self.assertRaises(AssertionError):
            self.restore()
        self.assertFalse(self.reports.exists())

    def test_previous_failure_and_all_events_remain_immutable(self):
        result = self.restore()
        self.assertEqual(result['events'], self.previous)
        self.assertEqual(result['phase'], 4)
        self.assertFalse(result['failed'])
        self.assertTrue(result['prior_C83_campaign_failed'])
        self.assertEqual(result['actual_new_corrective_events'], 0)
        self.assertEqual((self.source / 'c83-proof.json').read_bytes(), self.original_proof)
        self.assertTrue(json.loads(self.original_proof)['failed'])
        self.assertEqual((self.reports / 'prior-C83-failed-control-proof.json').read_bytes(), self.original_proof)
        self.assertEqual((self.reports / 'console-all.bin').read_bytes(), self.console.read_bytes())

    def test_corrupt_snapshot_rejected(self):
        with (self.source / 'current.rvg').open('ab') as f:
            f.write(b'!')
        self.reject_before_writes()

    def test_corrupt_console_rejected(self):
        self.console.write_bytes(self.console.read_bytes() + b'!')
        self.reject_before_writes()

    def test_unknown_failure_code_is_never_silently_cleared(self):
        self.codes['PTS_TRANSFER_ACTIVATION'] = [2]
        observation = json.loads((self.source / 'console-observation.json').read_text())
        observation['actual_response_codes'] = self.codes
        self.write('console-observation.json', observation)
        self.reject_before_writes()

    def test_successful_campaign_is_not_a_known_failed_source(self):
        self.state['failed'] = False
        self.save_state()
        self.reject_before_writes()

    def test_unchanged_bad_input_cannot_be_replayed_as_the_correction(self):
        phase = self.plan['phases'][4]
        bad = ROOT / 'state/experiments/c82-input-3.txt'
        phase.update(input_file=str(bad), input_bytes=bad.stat().st_size, input_sha256=hashlib.sha256(bad.read_bytes()).hexdigest())
        self.reject_before_writes()

    def test_new_phase_requires_its_own_ack_without_erasing_old_failure(self):
        from control_console_phase import evaluate_console_phase
        observed = {'actual_response_codes': copy.deepcopy(self.codes)}
        phase = self.plan['phases'][4]
        pending = evaluate_console_phase(observed, phase)
        self.assertFalse(pending['failed'])
        self.assertFalse(pending['succeeded'])
        observed['actual_response_codes']['PTS_C84_ACTIVATION'] = [0]
        done = evaluate_console_phase(observed, phase)
        self.assertTrue(done['succeeded'])
        self.assertEqual(observed['actual_response_codes']['PTS_TRANSFER_ACTIVATION'], [1])


if __name__ == '__main__':
    unittest.main()
