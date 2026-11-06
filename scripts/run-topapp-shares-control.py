#!/usr/bin/env python3
"""Isolated, source-bound CPU shares trial through the original Android engine."""
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
PLANS = REPO / 'state/experiments'


def read(path):
    return json.loads(Path(path).read_text())


def save(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + '\n')


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def replies(raw, phase):
    # The generated input contains printf format strings, never these exact replies.
    lines = [line.strip() for line in raw.decode('ascii', errors='replace').splitlines()]
    codes = [int(m[1]) for line in lines if (m := re.fullmatch(
        rf'REVERSE_CFS_TOPAPP_{phase}_RESULT RC_([0-9]+)', line))]
    shares = 16384 if phase == 'BOOST' else 1024
    return {'seen': bool(codes), 'failed': any(c != 0 for c in codes),
            'success': bool(codes) and all(c == 0 for c in codes)
            and f'REVERSE_CFS_TOPAPP_{phase}_DONE SHARES_{shares}' in lines}


def restore_command():
    return ('\nsu 0 sh -c \'f=/dev/cpuctl/top-app/cpu.shares; IFS= read -r b < "$f"; '
            '[ "$b" = 16384 ] || exit 61; printf "%s\\n" 1024 > "$f" || exit 62; '
            'IFS= read -r a < "$f"; [ "$a" = 1024 ] || exit 63; '
            'printf "\\nREVERSE_%s SHARES_%s\\n" CFS_TOPAPP_RESTORE_DONE "$a"\'; '
            'r=$?; printf \'\\nREVERSE_%s RC_%s\\n\' CFS_TOPAPP_RESTORE_RESULT "$r"\n')


def group_observation(probe, plan):
    assert probe['input_unchanged'] and probe['original_CPU_and_codecs_unchanged']
    group = next(g for g in probe['groups'] if g['pid'] == plan['zygote_PID'])
    assert group['thread_list_verified']
    leader = next(t for t in group['threads'] if t['pid'] == plan['zygote_PID'])
    scheduling = leader['scheduling']
    assert scheduling['task_and_runqueue_group_match'] and scheduling['ancestry_verified']
    selected = next(g for g in scheduling['groups'] if g['path'] == plan['group_path'])
    assert selected['task_group'] == plan['group_task_pointer']
    assert selected['quota_raw'] == '18446744073709551615'
    assert selected['period_raw'] == '100000000'
    assert scheduling['groups'][-1]['path'] == '/'
    assert scheduling['groups'][-1]['shares_raw'] == '1048576'
    return selected


def fresh_probe(runtime, source, sha, destination):
    subprocess.run(['node', '--max-old-space-size=5000',
                    str(runtime/'tools/reverseandroid-thread-probe.mjs'),
                    str(source/'current.rvg'), sha, str(destination)], check=True)
    result = read(destination)
    assert result['checkpoint_sha256'] == sha and result['input_unchanged']
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage', type=int)
    parser.add_argument('--runtime-root', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, required=True)
    parser.add_argument('--commit', required=True)
    args = parser.parse_args()
    assert re.fullmatch(r'[0-9a-f]{40}', args.commit)
    plan = read(PLANS/'topapp-shares-control-plan.json')
    assert 1 <= args.stage <= plan['max_stages']
    runtime, root = args.runtime_root.resolve(), args.output_root.resolve()
    reports = root/'reports'; reports.mkdir(parents=True, exist_ok=True)
    state_file = reports/'control-state.json'
    if args.stage == 1:
        assert not state_file.exists(), 'Never replay the source-bound shares input'
        source = args.source.resolve()
        contract = read(source/'output-contract.json')
        assert contract['checkpoint_sha256'] == plan['source_checkpoint_sha256']
        assert contract['instructions'] == plan['source_instructions']
        assert contract['epoch_sha256'] == plan['source_epoch_SHA256']
        assert digest(source/'current.rvg') == plan['source_checkpoint_sha256']
        assert digest(source/'jit5922-control-proof.json') == plan['source_control_proof_SHA256']
        old = read(source/'jit5922-control-proof.json')
        assert old['guard_succeeded'] and not old['failed'] and old['live_peripherals_verified']
        assert old['checkpoint_sha256'] == plan['source_checkpoint_sha256']
        assert old['last_stage'] == 3 and len(old['new_events']) == 1
        assert old['new_events'][0]['sha256'] == '70523e6019776b099cb44df08db7042b9cb46fcd154b7b6ac7af4c342369a1e5'
        assert len(old['inherited_events']) == 7
        packed = read(PLANS/'topapp-shares-source-console.json')
        assert packed['uncompressed_bytes'] <= 4*1024*1024
        with gzip.GzipFile(fileobj=io.BytesIO(base64.b64decode(packed['gzip_base64'],validate=True))) as zipped:
            raw = zipped.read(4*1024*1024+1)
        assert len(raw) == packed['uncompressed_bytes']
        assert hashlib.sha256(raw).hexdigest() == packed['uncompressed_SHA256'] == old['combined_console_SHA256'] == plan['source_console_SHA256']
        if (source/'console-all.bin').exists():
            assert digest(source/'console-all.bin') == plan['source_console_SHA256']
        (reports/'console-all.bin').write_bytes(raw)
        shutil.copy2(source/'jit5922-control-proof.json', reports/'source-JIT-proof.json')
        before = fresh_probe(runtime, source, plan['source_checkpoint_sha256'], reports/'source-threads.json')
        selected = group_observation(before, plan)
        assert selected['shares_raw'] == plan['kernel_shares_raw_before']
        owner = next(g for g in before['groups'] if g['pid'] == plan['zygote_PID'])
        worker = next(t for t in owner['threads'] if t['pid'] == plan['JIT_TID'])
        assert worker['name'] == 'Jit thread pool' and worker['tgid'] == plan['zygote_PID']
        assert worker['priority'] == worker['normal_prio'] == 120 and worker['exit_state'] == 0
        assert worker['scheduling']['credentials']['uid'] == '0'
        state = {'source': str(source), 'last_stage': 0, 'new_events': [],
                 'inherited_C87_events': old['inherited_events'], 'inherited_JIT_events': old['new_events'],
                 'source_JIT_proof_SHA256': plan['source_control_proof_SHA256'],
                 'failed': False, 'complete': False, 'restore_issued': False,
                 'original_boot_completed': False, 'live_peripherals_verified': True}
    else:
        state = read(state_file); source = Path(state['source'])
        assert state['last_stage'] == args.stage-1 and not state['failed'] and not state['complete']
        assert 1 <= len(state['new_events']) <= 2
    native = runtime/'build/ic-native/epoch-clz-ic'
    assert digest(native) == plan['native_SHA256']
    output = root/f'stage-{args.stage}'
    uart, purpose = '-', None
    if args.stage == 1:
        uart, purpose = str(PLANS/'topapp-shares-boost-input-plan.json'), 'boost'
    elif state['original_boot_completed'] and state['live_peripherals_verified'] and not state['restore_issued']:
        # Restore the normal Linux share through an additional explicit modeled input.
        assert state['boost_reply']['success'] and state['actual_boosted_group_verified']
        restore_file = reports/f'stage-{args.stage}-restore-input.txt'
        command = restore_command()
        payload = command.encode('ascii'); restore_file.write_bytes(payload)
        uart = str(reports/f'stage-{args.stage}-restore-input-plan.json'); purpose = 'restore'
        contract = read(source/'output-contract.json')
        save(uart, {'schema': 'reverseandroid-authenticated-console-input-plan-v1',
                    'purpose': 'diagnostic_console', 'source_checkpoint_sha256': contract['checkpoint_sha256'],
                    'source_instructions': contract['instructions'], 'input_file': restore_file.name,
                    'input_sha256': hashlib.sha256(payload).hexdigest(), 'input_bytes': len(payload),
                    'scope': 'Restore the trial top-app CPU shares16384 to original1024 after original completed boot; live values are guarded by the guest.'})
        state['restore_issued'] = True
    budget = plan['matched_first_budgets'][args.stage-1] if args.stage <= 2 else plan['continuation_budget']
    with (reports/f'stage-{args.stage}.json').open('x') as log:
        subprocess.run(['node','--expose-gc','--max-old-space-size=5000',str(runtime/'tools/reverseandroid-epoch.mjs'),
                        'run',str(source),str(output),str(native),str(runtime/'build/media/android-c27.raw'),
                        str(runtime/'build/reverseandroid/frame'),args.commit,str(budget),uart],
                       cwd=runtime,stdout=log,check=True)
    gate, host = read(output/'c46-progress.json'), read(output/'process-outcome.json')
    assert gate['source_boundary_verified'] and digest(output/'current.rvg') == gate['checkpoint_sha256']
    assert host['status'] == 'process_completed' and host['exit_code'] == 0 and not host['forced_kill']
    provenance = read(output/'current.json')['provenance']
    if purpose:
        event = read(output/'result.json')['external_uart_input']; expected = read(uart)
        assert event['sha256'] == expected['input_sha256'] and event['bytes'] == expected['input_bytes']
        assert event['instructions'] == read(source/'output-contract.json')['instructions']
        state['new_events'].append({'purpose':purpose, 'actual_native_event':event})
    else:
        assert provenance['new_uart_input_bytes'] == 0 and 'external_uart_input' not in provenance
    with (reports/'console-all.bin').open('ab') as stream:
        stream.write((output/'console-new.txt').read_bytes())
    raw = (reports/'console-all.bin').read_bytes()
    state['boost_reply'], state['restore_reply'] = replies(raw,'BOOST'), replies(raw,'RESTORE')
    state['failed'] = state['boost_reply']['failed'] or state['restore_reply']['failed']
    probe = fresh_probe(runtime,output,gate['checkpoint_sha256'],output/'threads-readonly.json')
    observed = group_observation(probe,plan)
    state['actual_boosted_group_verified'] = observed['shares_raw'] == plan['kernel_shares_raw_trial']
    state['actual_original_shares_restored'] = observed['shares_raw'] == plan['kernel_shares_raw_before']
    assert state['actual_boosted_group_verified'] or state['actual_original_shares_restored']
    subprocess.run(['node',str(runtime/'tools/reverseandroid-console-proof.mjs'),str(reports/'console-all.bin'),
                    '548714c27bfb7efe8b6300b99b2dcd77054b30d58b31c3c44a529e056a0bf42b',
                    '/data/local/tmp/reverseandroid-peer-abi3.elf',str(output/'console-observation.json')],check=True)
    subprocess.run(['node','--max-old-space-size=5000',str(runtime/'tools/reverseandroid-peripheral-probe.mjs'),
                    str(output/'current.rvg'),gate['checkpoint_sha256'],str(output/'peripherals-live-required.json'),
                    '/data/local/tmp/reverseandroid-peer-abi3.elf'],check=True)
    live = verify_live_peripherals(read(output/'peripherals-live-required.json'),read(output/'console-observation.json'),gate['checkpoint_sha256'])
    state.update(source=str(output), last_stage=args.stage, checkpoint_sha256=gate['checkpoint_sha256'],
                 instructions=gate['instructions'],live_peripherals_verified=live,
                 original_boot_completed=gate['android_boot_completed_verified'],
                 original_system_server_alive=gate['system_server_alive_verified'],
                 combined_console_SHA256=hashlib.sha256(raw).hexdigest(),native_SHA256=plan['native_SHA256'],
                 original_engine_and_codecs_preserved=True, usable_UI_response_verified=False,
                 fresh_hardware_power_cycle=False)
    state['complete'] = (not state['failed'] and state['restore_reply']['success']
                         and state['actual_original_shares_restored'] and boot_complete(True,live,gate))
    save(output/'topapp-control-proof.json',state);save(state_file,state)
    shutil.copy2(reports/'console-all.bin',output/'console-all.bin')
    shutil.rmtree(output/'native/journal')
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'],'a') as stream:
            stream.write('stop='+str(state['failed'] or state['complete']).lower()+'\n')
    print(json.dumps({k:state[k] for k in ['last_stage','checkpoint_sha256','instructions','boost_reply',
          'restore_reply','actual_boosted_group_verified','original_system_server_alive','original_boot_completed','complete','failed']}))


if __name__ == '__main__':
    main()
