#!/usr/bin/env python3
"""Resume the admitted 67B experiment; never resend its completed ELF transfer."""
import hashlib
import json
import pathlib
import shlex
import subprocess
import sys


def root(body, marker):
    return ('su 0 sh -c ' + shlex.quote(body) +
            "; r=$?; printf '\\nREVERSE_%s RC_%s\\n' " + marker + ' "$r"\n')


def main():
    repo = pathlib.Path(__file__).resolve().parent.parent
    original = json.loads((repo / 'state/experiments/peripheral-privatepts-control-plan.json').read_text())
    commands = [(repo / original['phases'][i]['input_file']).read_text() for i in [2, 3]]
    for command, i in zip(commands, [2, 3]):
        assert hashlib.sha256(command.encode()).hexdigest() == original['phases'][i]['input_sha256']
    tokens = shlex.split(commands[0])
    assert tokens[:4] == ['su', '0', 'sh', '-c']
    body = tokens[4]
    old = 'IFS= read -r e < /sys/fs/selinux/enforce;'
    assert body.count(old) == 2
    body = body.replace(old, 'e=$(cat /sys/fs/selinux/enforce);')
    # Observe the original guest read return and value, with its failure caught.
    # The enforcing gate itself still requires the complete actual value 1.
    diagnostic = ("read_rc=0; IFS= read -r e < /sys/fs/selinux/enforce || read_rc=$?; "
                  "printf '\\nREVERSE_%s READ_%s VALUE_%s\\n' PTS_ENFORCE_EOF \"$read_rc\" \"$e\"; "
                  'test "$e" = 1; ')
    body = body.replace('e=$(cat /sys/fs/selinux/enforce);', diagnostic + 'e=$(cat /sys/fs/selinux/enforce);', 1)
    elf = original['guest_ELF_path']
    body = body.replace('chmod 700 ' + elf, 'sha256sum ' + elf + '; chmod 700 ' + elf, 1)
    commands = [root(body, 'PTS_EOF_ACTIVATION'),
                root(shlex.split(commands[1])[4], 'PTS_EOF_INSPECT')]
    output = pathlib.Path(sys.argv[1])
    output.mkdir(parents=True, exist_ok=False)
    phases = []
    for i, (command, marker) in enumerate(zip(commands, ['REVERSE_PTS_EOF_ACTIVATION', 'REVERSE_PTS_EOF_INSPECT'])):
        raw = command.encode('ascii')
        assert 0 < len(raw) <= 4096
        subprocess.run(['bash', '-n'], input=raw, check=True)
        filename = f'peripheral-privatepts-eof-control-input-{i}.txt'
        (output / filename).write_bytes(raw)
        phases.append({'input_file': 'state/experiments/' + filename, 'input_bytes': len(raw),
                       'input_sha256': hashlib.sha256(raw).hexdigest(), 'response_marker': marker,
                       'failure_markers': [], 'instruction_budget': 1000000000,
                       'effect': 'Observe actual EOF return; read the complete enforcing value; provision the separate private PTY instance and require unchanged loaded policy and authentic ELF before execution.' if i == 0 else
                                 'Observe the stock OEM Lock and sensors Binder services and real device labels after actual peer readiness.'})
    plan = {**original, 'schema': 'reverseandroid-isolated-peripheral-private-PTY-EOF-control-v1',
            'source_checkpoint_sha256': '7313c8f2fab0d0bd53393355fbecbbd16f1fad62a6be26a5acb5ec33d799866b',
            'source_instructions': 67000000000,
            'source_artifact_lock': 'state/experiments/peripheral-privatepts-eof-control-artifacts.json',
            'source_run_id': 42177363469, 'source_commit': 'ab3d319e1dd185c5a1a408492c785225612e8768',
            'source_proof_file': 'peripheral-privatepts-control-proof.json',
            'source_original_failed_control_not_primary': True,
            'source_previous_event_count': 3, 'phases': phases,
            'completed_guest_ELF_transfer_repeated': False,
            'sysfs_no_newline_handled_without_changing_enforcement': True,
            'previous_transfer_history': 'docs/evidence/c80/privatepts-control-stage6.json',
            'full_original_source_admission_required': True}
    (output / 'peripheral-privatepts-eof-control-plan.json').write_text(json.dumps(plan, indent=2) + '\n')
    print(json.dumps({'source_instructions': plan['source_instructions'], 'phase_sizes': [p['input_bytes'] for p in phases],
                      'completed_transfer_resubmitted': False, 'guest_activation_passed': False}))


if __name__ == '__main__':
    main()
