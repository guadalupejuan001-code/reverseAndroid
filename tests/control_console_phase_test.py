import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / 'scripts'))
from control_console_phase import evaluate_console_phase


class PhaseTest(unittest.TestCase):
    phase = {'response_marker': 'REVERSE_PTS_TRANSFER_DATA_0',
             'response_markers': ['REVERSE_PTS_TRANSFER_DATA_0', 'REVERSE_PTS_TRANSFER_RX_1'],
             'failure_markers': ['REVERSE_PTS_TRANSFER_CAPTURE']}

    def check_codes(self, codes):
        return evaluate_console_phase({'actual_response_codes': codes}, self.phase)

    def test_cannot_advance_before_receiver_read_acknowledgment(self):
        self.assertFalse(self.check_codes({})['succeeded'])
        self.assertFalse(self.check_codes({'PTS_TRANSFER_DATA_0': [0]})['succeeded'])
        self.assertFalse(self.check_codes({'PTS_TRANSFER_RX_1': [0]})['succeeded'])
        self.assertTrue(self.check_codes({'PTS_TRANSFER_DATA_0': [0], 'PTS_TRANSFER_RX_1': [0]})['succeeded'])

    def test_capture_failure_cannot_be_hidden_by_successful_partial_acknowledgments(self):
        result = self.check_codes({'PTS_TRANSFER_DATA_0': [0], 'PTS_TRANSFER_RX_1': [0], 'PTS_TRANSFER_CAPTURE': [21]})
        self.assertTrue(result['failed'])
        self.assertFalse(result['succeeded'])
        self.assertTrue(self.check_codes({'PTS_TRANSFER_DATA_0': [0, 1]})['failed'])

    def test_unrelated_earlier_responses_cannot_complete_this_phase(self):
        self.assertFalse(self.check_codes({'PTS_ENFORCE': [0], 'PTS_TRANSFER_RX_0': [0]})['succeeded'])

    def test_empty_requirements_and_non_integer_codes_are_invalid(self):
        with self.assertRaises(AssertionError):
            evaluate_console_phase({'actual_response_codes': {}}, {**self.phase, 'response_markers': []})
        for value in ['0', True, -1, 256]:
            with self.assertRaises(AssertionError):
                self.check_codes({'PTS_TRANSFER_DATA_0': [value]})


if __name__ == '__main__':
    unittest.main()
