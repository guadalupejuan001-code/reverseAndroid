#!/usr/bin/env python3
"""Resume a pinned admitted snapshot and its actual console, never replay inputs."""
import copy
import hashlib
import json
import pathlib
import sys


def restore_campaign(source, proof_name, console_file, reports, plan):
    source, console_file, reports = map(pathlib.Path, (source, console_file, reports))
    read = lambda p: json.loads(p.read_text())
    contract = read(source / 'output-contract.json')
    gate = read(source / 'c46-progress.json')
    state = read(source / proof_name)
    assert contract['checkpoint_sha256'] == plan['source_checkpoint_sha256']
    assert contract['instructions'] == plan['source_instructions']
    assert gate['source_boundary_verified'] is True
    assert state['last_admitted_sha256'] == gate['checkpoint_sha256'] == contract['checkpoint_sha256']
    assert state['last_admitted_instructions'] == gate['instructions'] == contract['instructions']
    assert not state['failed'] and not state['complete']
    assert state['SELinux_enforcing'] is True
    assert state['phase'] == plan['inherited_phase']
    assert state['waiting'] is plan['inherited_waiting']
    assert state['events'] == plan['inherited_events']
    phases = [event['phase'] for event in state['events']]
    assert len(phases) == len(set(phases))
    assert phases == list(range(state['phase'] + int(state['waiting'])))
    assert console_file.stat().st_size <= 64 * 1024 * 1024
    raw = console_file.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    assert digest == state['combined_original_console_bytes_sha256'] == plan['inherited_console_SHA256']
    assert not (reports / 'control-state.json').exists() and not (reports / 'console-all.bin').exists()
    resumed = copy.deepcopy(state)
    resumed['source'] = str(source)
    resumed['resumed_from_original_proof_file'] = str(source / proof_name)
    resumed['inherited_console_SHA256'] = digest
    resumed['inherited_native_events_replayed'] = False
    reports.mkdir(exist_ok=True, parents=True)
    with (reports / 'console-all.bin').open('xb') as f:
        f.write(raw)
    with (reports / 'control-state.json').open('x') as f:
        json.dump(resumed, f, indent=2)
        f.write('\n')
    return resumed


if __name__ == '__main__':
    assert len(sys.argv) == 6
    plan = json.loads(pathlib.Path(sys.argv[5]).read_text())
    state = restore_campaign(*sys.argv[1:5], plan)
    print(json.dumps({'restored_phase': state['phase'], 'waiting': state['waiting'],
                      'inherited_events': len(state['events']), 'new_input_events': 0}))
