"""Model GitHub's empty outputs on skipped steps, without executing a guest."""
import pathlib
import re
import unittest
import yaml

ROOT = pathlib.Path(__file__).resolve().parent.parent


def enabled(expression, outcomes, outputs, successful=True):
    value = expression.replace('success()', repr(successful))
    value = re.sub(r"steps\.(r[0-9]+)\.outcome", lambda m: repr(outcomes.get(m[1], 'skipped')), value)
    value = re.sub(r"steps\.(r[0-9]+)\.outputs\.complete", lambda m: repr(outputs.get(m[1], '')), value)
    assert 'steps.' not in value
    return bool(eval(value.replace('&&', 'and').replace('||', 'or'), {'__builtins__': {}}, {}))


def run_graph(runs, terminal, failure=False):
    outcomes, outputs, executed = {}, {}, []
    successful = True
    for step in runs:
        if enabled(step['if'], outcomes, outputs, successful):
            executed.append(step['id'])
            is_terminal = len(executed) == terminal
            outcomes[step['id']] = 'failure' if failure and is_terminal else 'success'
            outputs[step['id']] = 'true' if is_terminal else 'false'
            successful = not (failure and is_terminal)
        else:
            outcomes[step['id']] = 'skipped'
    return executed


class WorkflowPhaseGuardTest(unittest.TestCase):
    def workflows(self):
        paths = [ROOT / '.github/workflows/android-c81-systemserver-diagnostic.yml']
        for name in ['android-c82-validated-peripherals.yml', 'android-c83-clz-resumed-boot.yml', 'android-c84-corrected-guest-shell.yml']:
            candidate = ROOT / '.github/workflows' / name
            if candidate.exists():
                paths.append(candidate)
        for path in paths:
            w = yaml.safe_load(path.read_text())
            steps = next(iter(w['jobs'].values()))['steps']
            yield path.name, [s for s in steps if re.fullmatch(r'r[0-9]+', s.get('id', ''))]

    def test_no_later_native_run_after_any_completed_or_failed_stage(self):
        for name, runs in self.workflows():
            for terminal in range(1, len(runs) + 1):
                for failure in [False, True]:
                    with self.subTest(workflow=name, terminal=terminal, failure=failure):
                        self.assertEqual(run_graph(runs, terminal, failure), [s['id'] for s in runs[:terminal]])

    def test_the_observed_old_condition_reproduces_a_run_after_a_skipped_step(self):
        broken = [{'id': f'r{n}', 'if': 'success()' + (f" && steps.r{n-1}.outputs.complete != 'true'" if n > 1 else '')} for n in range(1, 9)]
        self.assertIn('r5', run_graph(broken, 3))


if __name__ == '__main__':
    unittest.main()
