#!/usr/bin/env python3
"""Start an explicit correction, retaining the authenticated failed C83 attempt."""
import copy
import hashlib
import json
import pathlib
import sys

from console_command_syntax import validate_nested_console_command

ROOT = pathlib.Path(__file__).resolve().parents[1]


def restore_corrective(source, console_file, reports, plan):
    source, console_file, reports = map(pathlib.Path, (source, console_file, reports))
    read = lambda name: json.loads((source / name).read_text())
    digest = lambda b: hashlib.sha256(b).hexdigest()
    contract, gate, host, kernel = map(read, ['output-contract.json', 'c46-progress.json', 'process-outcome.json', 'kernel-observation.json'])
    proof_bytes = (source / 'c83-proof.json').read_bytes()
    previous = json.loads(proof_bytes)
    assert digest(proof_bytes) == plan['source_failed_proof_SHA256']
    assert contract['checkpoint_sha256'] == plan['source_checkpoint_sha256']
    assert contract['instructions'] == plan['source_instructions']
    with (source / 'current.rvg').open('rb') as f:
        assert hashlib.file_digest(f, 'sha256').hexdigest() == contract['checkpoint_sha256']
    assert gate['source_boundary_verified'] is True
    assert gate['checkpoint_sha256'] == previous['last_admitted_sha256'] == contract['checkpoint_sha256']
    assert gate['instructions'] == previous['last_admitted_instructions'] == contract['instructions']
    assert host['status'] == 'process_completed' and host['exit_code'] == 0 and not host['forced_kill']
    assert kernel['live_kernel_security']['selinux_enforcing'] is True
    assert previous['failed'] is True and previous['complete'] is False
    assert previous['phase'] == 3 and previous['waiting'] is True
    assert previous['events'] == plan['inherited_events']
    assert [event['phase'] for event in previous['events']] == [0, 1, 2, 3]
    assert previous['events'][-1]['actual_native_event']['sha256'] == plan['known_failed_input_SHA256']
    raw = console_file.read_bytes()
    assert len(raw) <= 64 * 1024 * 1024
    assert digest(raw) == previous['combined_original_console_bytes_sha256'] == plan['inherited_console_SHA256']
    assert b"sh: syntax error: unexpected ';;'" in raw
    observation = read('console-observation.json')
    assert observation['raw_console_SHA256'] == digest(raw)
    for name in ['PTS_TRANSFER_RX_0', 'PTS_TRANSFER_DATA_0', 'PTS_TRANSFER_RX_1', 'PTS_TRANSFER_DATA_1', 'PTS_TRANSFER_CAPTURE']:
        assert observation['actual_response_codes'][name] == [0]
    assert observation['actual_response_codes']['PTS_TRANSFER_ACTIVATION'] == [1]
    legacy = (ROOT / plan['known_failed_input_file']).read_bytes()
    assert digest(legacy) == plan['known_failed_input_SHA256']
    try:
        validate_nested_console_command(legacy)
    except ValueError as error:
        assert 'actual guest sh -c argument' in str(error)
    else:
        raise AssertionError('Known failed input no longer reproduces its actual nested syntax error')
    assert len(plan['phases']) == 6
    for phase in plan['phases'][4:]:
        data = (ROOT / phase['input_file']).read_bytes()
        assert 0 < len(data) == phase['input_bytes'] <= 4096
        assert digest(data) == phase['input_sha256'] != plan['known_failed_input_SHA256']
        validate_nested_console_command(data)
    assert not (reports / 'control-state.json').exists() and not (reports / 'console-all.bin').exists()
    state = copy.deepcopy(previous)
    state.update(source=str(source), phase=4, waiting=False, failed=False, complete=False,
                 hardware_commands_completed=False, corrective_campaign='C84',
                 prior_C83_campaign_failed=True, prior_failed_phase=3,
                 prior_failed_control_proof_SHA256=digest(proof_bytes),
                 prior_failed_control_proof_preserved=True,
                 known_failed_input_not_replayed_unchanged=True,
                 receiver_and_base64_data_not_retransferred=True,
                 actual_new_corrective_events=0, ui_response_verified=False)
    # Only campaign metadata is new. The original failed proof, native files,
    # snapshot and full console remain unchanged and separately authenticated.
    reports.mkdir(exist_ok=True, parents=True)
    with (reports / 'prior-C83-failed-control-proof.json').open('xb') as f:
        f.write(proof_bytes)
    with (reports / 'console-all.bin').open('xb') as f:
        f.write(raw)
    with (reports / 'control-state.json').open('x') as f:
        json.dump(state, f, indent=2)
        f.write('\n')
    assert (source / 'c83-proof.json').read_bytes() == proof_bytes
    return state


if __name__ == '__main__':
    assert len(sys.argv) == 5
    plan = json.loads(pathlib.Path(sys.argv[4]).read_text())
    state = restore_corrective(*sys.argv[1:4], plan)
    print(json.dumps({'new_campaign': 'C84', 'prior_failed_phase_retained': 3,
                      'next_phase': state['phase'], 'retained_events': len(state['events']),
                      'new_events_at_setup': 0, 'snapshot_bytes_changed': False}))
