import hashlib
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from console_command_syntax import extract_su_shell_body, validate_nested_console_command
from prepare_peripheral_privatepts_buffered_inputs_fixture import root_command


class ConsoleSyntaxTest(unittest.TestCase):
    def test_both_actual_C82_failures_passed_outer_only_but_fail_real_inner_syntax(self):
        fixtures = [(3, 'b0d5c3ec2ab21122066e4fdca6d0224b9d7501b8440862a145f1b9103d0d2a17'),
                    (4, '46667b569856ab07a01b2f8c6e612705363e27432074c5abe759ea20eeef2191')]
        for phase, digest in fixtures:
            with self.subTest(phase=phase):
                raw = (ROOT / f'state/experiments/c82-input-{phase}.txt').read_bytes()
                self.assertEqual(hashlib.sha256(raw).hexdigest(), digest)
                self.assertEqual(subprocess.run(['sh', '-n'], input=raw, capture_output=True).returncode, 0)
                with self.assertRaisesRegex(ValueError, 'actual guest sh -c argument'):
                    validate_nested_console_command(raw)

    def test_extraction_matches_arguments_received_from_real_shells(self):
        body = "printf '%s\\n' \"a; b 'quoted'\"; test \"${x:-one}\" = one; true;"
        command = root_command(body, 'PTS_TEST')
        with tempfile.TemporaryDirectory() as temp:
            path = pathlib.Path(temp)
            fake = path / 'su'
            fake.write_text('#!/usr/bin/env python3\nimport json,os,pathlib,sys\npathlib.Path(os.environ["TASK_ARGUMENT_RECORD"]).write_text(json.dumps(sys.argv[1:]))\n')
            fake.chmod(0o755)
            for shell in ['sh', 'bash']:
                out = path / f'{shell}.json'
                env = {**os.environ, 'PATH': str(path) + os.pathsep + os.environ['PATH'], 'TASK_ARGUMENT_RECORD': str(out)}
                subprocess.run([shell, '-c', command], env=env, check=True, capture_output=True)
                actual = json.loads(out.read_text())
                self.assertEqual(actual[:3], ['0', 'sh', '-c'])
                self.assertEqual(actual[3], body)
                self.assertEqual(extract_su_shell_body(command), actual[3])
                self.assertNotEqual(actual[3], __import__('shlex').split(command)[4])

    def test_validator_checks_without_executing_any_body_or_substitution(self):
        with tempfile.TemporaryDirectory() as temp:
            marker = pathlib.Path(temp) / 'must-not-exist'
            command = root_command(f'touch {marker}; printf "%s" "$(touch {marker})";', 'PTS_TEST')
            validate_nested_console_command(command)
            self.assertFalse(marker.exists())

    def test_previously_passed_immutable_control_arguments_still_parse(self):
        plan = json.loads((ROOT / 'state/experiments/peripheral-privatepts-eof-control-plan.json').read_text())
        for phase in plan['phases']:
            raw = (ROOT / phase['input_file']).read_bytes()
            self.assertEqual(hashlib.sha256(raw).hexdigest(), phase['input_sha256'])
            validate_nested_console_command(raw)


if __name__ == '__main__':
    unittest.main()
