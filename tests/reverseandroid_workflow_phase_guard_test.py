"""Model GitHub's empty outputs on skipped steps, without executing a guest."""
import pathlib
import re
import unittest
import yaml

ROOT = pathlib.Path(__file__).resolve().parent.parent


def enabled(expression, outcomes, outputs, successful=True):
    value = expression.replace('success()', repr(successful))
    value = re.sub(r"steps\.([rp][0-9]+)\.outcome", lambda m: repr(outcomes.get(m[1], 'skipped')), value)
    value = re.sub(r"steps\.(r[0-9]+)\.outputs\.complete", lambda m: repr(outputs.get(m[1], '')), value)
    assert 'steps.' not in value
    return bool(eval(value.replace('&&', 'and').replace('||', 'or'), {'__builtins__': {}}, {}))


def run_graph(runs, terminal, failure=False, skipped_preservation=None):
    outcomes, outputs, executed = {}, {}, []
    successful = True
    for step in runs:
        if enabled(step['if'], outcomes, outputs, successful):
            executed.append(step['id'])
            is_terminal = len(executed) == terminal
            outcomes[step['id']] = 'failure' if failure and is_terminal else 'success'
            stage = int(step['id'][1:])
            outcomes[f'p{stage}'] = 'skipped' if stage == skipped_preservation else 'success'
            outputs[step['id']] = 'true' if is_terminal else 'false'
            successful = not (failure and is_terminal)
        else:
            outcomes[step['id']] = 'skipped'
    return executed


class WorkflowPhaseGuardTest(unittest.TestCase):
    def test_generated_workflows_keep_the_GitHub_on_trigger(self):
        # PyYAML's default YAML 1.1 loader treats an unquoted on key as True.
        # BaseLoader checks the actual emitted key, including valid unquoted on.
        for name in ['android-c88-jit-priority.yml','android-c87-bounded-continuation.yml','retire-replaced-android-reference-runs.yml']:
            path=ROOT/'.github/workflows'/name
            if not path.exists():continue
            with self.subTest(workflow=name):
                value=yaml.load(path.read_text(),Loader=yaml.BaseLoader)
                self.assertIn('on',value)
                self.assertNotIn('true',value)

    def workflows(self):
        paths = [ROOT / '.github/workflows/android-c81-systemserver-diagnostic.yml']
        for name in ['android-c82-validated-peripherals.yml', 'android-c83-clz-resumed-boot.yml', 'android-c84-corrected-guest-shell.yml', 'android-c85-ic-ivau-continuation.yml', 'android-c86-cleanup-priority.yml', 'android-c87-bounded-continuation.yml', 'android-c88-jit-priority.yml']:
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

    def test_preserves_the_actual_stage_before_advancing_or_releasing(self):
        for cycle,name in [(86,'android-c86-cleanup-priority.yml'),(87,'android-c87-bounded-continuation.yml'),(88,'android-c88-jit-priority.yml')]:
            path=ROOT/'.github/workflows'/name
            if not path.exists():continue
            steps=next(iter(yaml.safe_load(path.read_text())['jobs'].values()))['steps']
            preserves=[s for s in steps if s.get('uses')=='./.github/actions/preserve-android-checkpoint']
            runs=[s for s in steps if re.fullmatch(r'r[0-9]+',s.get('id',''))]
            self.assertEqual(len(preserves),len(runs))
            for n,step in enumerate(preserves,1):
                with self.subTest(cycle=cycle,stage=n):
                    self.assertEqual(step['id'],f'p{n}')
                    source=step['with']['checkpoint_path']
                    self.assertEqual(source,f'evidence/c{cycle}-{n}')
                    self.assertIn(f"hashFiles('{source}/c46-progress.json')",step['if'])
                    release=next(s for s in steps if s.get('name','').startswith(f'C{cycle} ') and f' stage {n}: release ' in s.get('name',''))
                    self.assertIn(f"steps.p{n}.outcome == 'success'",release['if'])
                    self.assertEqual(run_graph(runs,len(runs)+1,skipped_preservation=n),[f'r{i}' for i in range(1,n+1)])


if __name__ == '__main__':
    unittest.main()
