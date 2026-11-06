#!/usr/bin/env python3
"""One explicit source-bound JIT priority input, then bounded zero-input runs.

The original epoch controller, snapshots, admission and peripheral observers
remain authoritative. C87 remains the unchanged-input reference.
"""
import argparse
import base64
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from c85_peripheral_completion import verify_live_peripherals, boot_complete


REPO = Path(__file__).resolve().parents[1]
STATE = REPO / 'state/experiments'


def read(path):
    return json.loads(Path(path).read_text())


def save(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + '\n')


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def guard_outcome(raw):
    lines = [line.strip() for line in raw.decode('utf-8', errors='replace').splitlines()]
    codes = [int(m[1]) for line in lines
             if (m := re.fullmatch(r'REVERSE_JIT5922_CONTROL_RESULT RC_([0-9]+)', line))]
    return {'guard_result_seen': bool(codes), 'guard_succeeded': bool(codes)
            and all(code == 0 for code in codes) and 'REVERSE_JIT5922_CONTROL_DONE NICE_0' in lines,
            'failed': any(code != 0 for code in codes)}


def verify_target(observed, kernel, plan):
    assert observed['input_unchanged'] and observed['checkpoint_sha256'] == plan['source_checkpoint_sha256']
    assert kernel['checkpoint_sha256'] == plan['source_checkpoint_sha256']
    group = next(g for g in observed['groups'] if g['pid'] == plan['zygote_PID'])
    assert group['thread_list_verified']
    worker = next(t for t in group['threads'] if t['pid'] == plan['JIT_TID'])
    assert worker['tgid'] == plan['zygote_PID'] and worker['name'] == 'Jit thread pool'
    assert worker['priority'] == worker['normal_prio'] == 139 and worker['state'] == 0
    assert worker['scheduling']['credentials']['uid'] == '0'
    owner = next(p for p in kernel['zygote_launch']['processes'] if p['pid'] == plan['zygote_PID'])
    assert owner['exe_path'] == '/system/bin/app_process64' and owner['argv_verified']
    assert owner['argv'] == ['zygote64'] and owner['exit_state'] == 0
    return worker


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage', type=int)
    parser.add_argument('--runtime-root', type=Path, default=REPO)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, required=True)
    parser.add_argument('--commit', required=True)
    args = parser.parse_args()
    assert re.fullmatch(r'[0-9a-f]{40}', args.commit)
    plan = read(STATE / 'jit5922-control-plan.json')
    assert 1 <= args.stage <= plan['max_stages']
    runtime, root = args.runtime_root.resolve(), args.output_root.resolve()
    reports = root / 'reports'; reports.mkdir(parents=True, exist_ok=True)
    state_file = reports / 'control-state.json'
    if args.stage == 1:
        assert not state_file.exists(), 'Priority input must never be replayed'
        source = args.source.resolve(); contract = read(source / 'output-contract.json')
        assert contract['checkpoint_sha256'] == plan['source_checkpoint_sha256']
        assert contract['instructions'] == plan['source_instructions']
        assert contract['epoch_sha256'] == plan['source_epoch_SHA256']
        assert digest(source / 'current.rvg') == contract['checkpoint_sha256']
        previous = source / 'c87-proof.json'; inherited = read(previous)
        assert digest(previous) == plan['source_control_proof_SHA256']
        assert inherited['events'] == plan['inherited_events'] and inherited['phase'] == 7
        assert not inherited['waiting'] and not inherited['failed'] and inherited['live_peripherals_verified']
        assert read(source / 'c46-progress.json')['source_boundary_verified']
        packed = read(STATE / 'jit5922-control-source-console.json')
        assert packed['uncompressed_bytes'] <= 4 * 1024 * 1024
        with gzip.GzipFile(fileobj=io.BytesIO(base64.b64decode(packed['gzip_base64'], validate=True))) as zipped:
            raw = zipped.read(4 * 1024 * 1024 + 1)
        assert len(raw) == packed['uncompressed_bytes']
        assert hashlib.sha256(raw).hexdigest() == packed['uncompressed_SHA256'] == plan['source_console_SHA256']
        assert inherited['combined_original_console_bytes_sha256'] == plan['source_console_SHA256']
        (reports / 'console-all.bin').write_bytes(raw)
        shutil.copy2(previous, reports / 'source-c87-proof.json')
        before = reports / 'source-threads-readonly.json'
        subprocess.run(['node', '--max-old-space-size=5000', str(runtime/'tools/reverseandroid-thread-probe.mjs'),
                        str(source/'current.rvg'), contract['checkpoint_sha256'], str(before)], check=True)
        verify_target(read(before), read(source/'kernel-observation.json'), plan)
        state = {'source': str(source), 'new_events': [], 'inherited_events': plan['inherited_events'],
                 'inherited_C87_proof_SHA256': plan['source_control_proof_SHA256'],
                 'C87_continues_as_reference': True, 'failed': False, 'complete': False}
    else:
        state = read(state_file); source = Path(state['source'])
        assert not state['failed'] and not state['complete'] and len(state['new_events']) == 1
        assert state['last_stage'] == args.stage-1
    native = runtime / 'build/ic-native/epoch-clz-ic'
    assert digest(native) == plan['native_SHA256']
    uart = str(STATE/'jit5922-control-input-plan.json') if args.stage == 1 else '-'
    budget = plan['first_instruction_budget'] if args.stage == 1 else plan['continuation_instruction_budget']
    assert 0 < budget <= 1000000000
    output = root / f'stage-{args.stage}'
    command = ['node', '--expose-gc', '--max-old-space-size=5000', str(runtime/'tools/reverseandroid-epoch.mjs'),
               'run', str(source), str(output), str(native), str(runtime/'build/media/android-c27.raw'),
               str(runtime/'build/reverseandroid/frame'), args.commit, str(budget), uart]
    with (reports/f'stage-{args.stage}.json').open('x') as log:
        subprocess.run(command, cwd=runtime, stdout=log, check=True)
    gate, host = read(output/'c46-progress.json'), read(output/'process-outcome.json')
    assert gate['source_boundary_verified'] and digest(output/'current.rvg') == gate['checkpoint_sha256']
    assert host['status'] == 'process_completed' and host['exit_code'] == 0 and not host['forced_kill']
    provenance = read(output/'current.json')['provenance']
    if args.stage == 1:
        event = read(output/'result.json')['external_uart_input']; expected = read(uart)
        assert event['instructions'] == plan['source_instructions'] and event['sha256'] == expected['input_sha256']
        assert event['bytes'] == expected['input_bytes']
        state['new_events'].append(event)
    else:
        assert provenance['new_uart_input_bytes'] == 0 and 'external_uart_input' not in provenance
    assert len(state['new_events']) == 1 and state['inherited_events'] == plan['inherited_events']
    with (reports/'console-all.bin').open('ab') as stream:
        stream.write((output/'console-new.txt').read_bytes())
    raw = (reports/'console-all.bin').read_bytes(); state.update(guard_outcome(raw))
    observation = output/'console-observation.json'
    subprocess.run(['node', str(runtime/'tools/reverseandroid-console-proof.mjs'), str(reports/'console-all.bin'),
                    '548714c27bfb7efe8b6300b99b2dcd77054b30d58b31c3c44a529e056a0bf42b',
                    '/data/local/tmp/reverseandroid-peer-abi3.elf', str(observation)], check=True)
    probe = output/'peripherals-live-required.json'
    result = subprocess.run(['node','--max-old-space-size=5000',str(runtime/'tools/reverseandroid-peripheral-probe.mjs'),
                             str(output/'current.rvg'),gate['checkpoint_sha256'],str(probe),
                             '/data/local/tmp/reverseandroid-peer-abi3.elf'])
    live = verify_live_peripherals(read(probe) if result.returncode == 0 else None,
                                   read(observation),gate['checkpoint_sha256'])
    state.update(source=str(output), last_stage=args.stage, checkpoint_sha256=gate['checkpoint_sha256'],
                 instructions=gate['instructions'], live_peripherals_verified=live,
                 original_boot_completed=gate['android_boot_completed_verified'],
                 original_system_server_alive=gate['system_server_alive_verified'],
                 combined_console_SHA256=hashlib.sha256(raw).hexdigest(),
                 native_SHA256=plan['native_SHA256'], original_admission_and_codecs_preserved=True,
                 usable_UI_response_verified=False, fresh_hardware_power_cycle=False)
    state['complete'] = state['guard_succeeded'] and boot_complete(True, live, gate)
    save(output/'jit5922-control-proof.json',state); save(state_file,state)
    shutil.copy2(reports/'console-all.bin',output/'console-all.bin')
    shutil.rmtree(output/'native/journal')
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'],'a') as stream:
            stream.write('stop='+str(state['failed'] or state['complete']).lower()+'\n')
    print(json.dumps({'stage':args.stage,'checkpoint_sha256':state['checkpoint_sha256'],
                      'instructions':state['instructions'], 'guard_succeeded':state['guard_succeeded'],
                      'failed':state['failed'],'original_boot_completed':state['original_boot_completed']}))


if __name__ == '__main__':
    main()
