#!/usr/bin/env python3
"""C90R: explicitly select a validated host policy, preserving original guest gates.

The larger validation output is evidence only. This campaign always recovers the
original C90 stage 6 and changes only the authenticated host executable/profile.
The old zero-input controllers and their native pins remain untouched.
"""
import argparse
import base64
import copy
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from c88_jit_priority_control import read, save, digest, parse_reply
from c85_peripheral_completion import verify_live_peripherals, boot_complete


ROOT = Path(__file__).resolve().parents[1]
CAP = 3840 * 1024**2
OLD_NATIVE = '957f22e2c8c8d3591029529855c733a05fc5c468e9a6e881ff5238bea6ff102f'
SOURCE_SHA = 'c8a2aa15bca412139973343813570a07d8f11d9fb7318a884cd64d45025f5714'
SOURCE_PROOF = '3751c589aa84129202905ee3a0f2584da802ab5d5106d00361b6450bafab66d2'
SOURCE_COMMIT = 'bea171f38356e681ac08d31669b5c81c6cbcd2cc'
SOURCE_INSTRUCTIONS = 161147727998
SOURCE_RUN = 42317967033
SOURCE_LOCK = 'state/experiments/c90-stage-6-artifacts.json'
PLAN = 'state/experiments/c90r-plan.json'
REFERENCE = 'state/experiments/host-transport-plan.json'
REPORTS = Path('evidence/c90r-reports')
SOURCE = Path('build/source-c90r')
RUNTIME = Path('build/host-transport-runtime')
NATIVE = Path('build/host-native/epoch-clz-ic-host')
FRAME = Path('build/host-native/frame')
MANIFEST = Path('build/host-native/host-transport-manifest.json')


def require_sha(value, length=64):
    assert isinstance(value, str) and re.fullmatch(r'[0-9a-f]{' + str(length) + r'}', value)


def state_path(value):
    path = Path(value)
    assert not path.is_absolute() and path.parts[:2] == ('state', 'experiments')
    assert '..' not in path.parts
    return path


def validate_plan(p, reference):
    """Reject a different source, replayed input or an implicit host selection."""
    assert p['schema'] == 'reverseandroid-validated-host-zero-input-continuation-plan-v1'
    assert p['cycle'] == 'c90r' and p['source_cycle'] == 'C90' and p['source_stage'] == 6
    assert p['source_checkpoint_sha256'] == SOURCE_SHA
    assert p['source_instructions'] == SOURCE_INSTRUCTIONS and p['source_execution_commit'] == SOURCE_COMMIT
    assert p['source_run_id'] == SOURCE_RUN and p['source_native_SHA256'] == OLD_NATIVE
    assert p['source_proof_file'] == 'zero-input-proof.json' and p['source_proof_SHA256'] == SOURCE_PROOF
    assert p['source_original_artifact_lock'] == SOURCE_LOCK
    assert p['source_artifact_lock'] == 'state/experiments/c90r-artifacts.json'
    assert p['host_snapshot_capacity_bytes'] == CAP
    assert type(p['new_UART_bytes']) is int and p['new_UART_bytes'] == 0
    assert 'input_file' not in p and 'input_plan_file' not in p
    assert p['host_wall_ms'] == 900000 and p['max_stages'] == 16
    assert p['initial_instruction_budget'] == reference['large_instruction_budget'] == 300000000
    assert p['continuation_instruction_budget'] == 1000000000
    for key in ['fresh_hardware_power_cycle', 'guest_snapshot_patching', 'guest_property_injection']:
        assert p[key] is False
    assert p['original_CPU_codecs_RAM_disk_seed_clock_cadence_IRQ_preserved'] is True
    assert p['original_BOOT_and_UI_not_yet_claimed'] is True
    assert reference['canonical_source']['checkpoint_sha256'] == SOURCE_SHA
    assert reference['canonical_source']['instructions'] == SOURCE_INSTRUCTIONS
    assert reference['canonical_source']['proof_SHA256'] == SOURCE_PROOF
    assert reference['baseline_native_SHA256'] == OLD_NATIVE and reference['host_snapshot_capacity_bytes'] == CAP
    assert p['inherited_source_events'] == reference['inherited_source_events']
    assert len(p['inherited_source_events']) == 8
    assert [event['phase'] for event in p['inherited_source_events']] == list(range(8))
    for key in ['source_console_base64_SHA256', 'source_console_gzip_SHA256',
                'source_console_bytes', 'source_console_SHA256', 'ELF_SHA256', 'guest_ELF_path']:
        assert p[key] == reference[key], key
    state_path(p['source_console_file'])
    assert 0 < p['source_console_bytes'] <= 64 * 1024**2
    require_sha(p['expected_native_SHA256'])
    require_sha(p['expected_frame_SHA256'])
    assert p['expected_native_SHA256'] != OLD_NATIVE
    h = p['host_validation']
    assert type(h['run_id']) is int and h['run_id'] > 0
    require_sha(h['execution_commit'], 40)
    for key in ['report_SHA256', 'manifest_SHA256', 'large_checkpoint_sha256']:
        require_sha(h[key])
    assert h['large_transport_alias'] == 'validated-large'
    state_path(h['large_transport_manifest'])


def validate_report(p, reference, report):
    """Bind the actual old/new controls and larger admission to the selected profile."""
    h = p['host_validation']
    assert report['schema'] == 'reverseandroid-validated-host-only-transport-extension-v1'
    assert report['validated'] is True and report['candidate_is_not_yet_primary'] is True
    assert report['CI_run_id'] == h['run_id'] and report['execution_commit'] == h['execution_commit']
    assert report['baseline_native_SHA256'] == OLD_NATIVE
    assert report['candidate_native_SHA256'] == p['expected_native_SHA256']
    assert report['candidate_frame_SHA256'] == p['expected_frame_SHA256']
    assert report['host_extension_manifest_SHA256'] == h['manifest_SHA256']
    assert report['host_snapshot_capacity_bytes'] == CAP and report['new_UART_bytes'] == 0
    for key in ['original_870_source_files_unchanged',
                'guest_RAM_disk_seed_clock_cadence_IRQ_and_snapshot_codecs_unchanged',
                'original_BOOT_and_UI_gates_unchanged']:
        assert report[key] is True, key
    control = report['full_original_control']
    assert control['schema'] == 'reverseandroid-host-transport-byte-equality-v1'
    assert control['control_source'] == reference['control_source']
    assert control['instructions'] == reference['control_instruction_budget'] == 50000000
    assert control['baseline_native_SHA256'] == OLD_NATIVE
    assert control['candidate_native_SHA256'] == p['expected_native_SHA256']
    assert control['new_UART_bytes'] == 0
    for key in ['actual_cmp_every_RVG_byte_equal', 'entire_console_bytes_equal',
                'original_C46_gate_equal', 'entire_original_kernel_observation_equal',
                'host_transport_policy_not_serialized', 'control_is_not_primary_authority']:
        assert control[key] is True, key
    large = report['full_larger_original_admission']
    assert large['schema'] == 'reverseandroid-host-transport-large-original-admission-v1'
    assert large['source'] == reference['canonical_source']
    assert large['host_snapshot_capacity_bytes'] == CAP
    assert large['native_SHA256'] == p['expected_native_SHA256']
    assert large['frame_native_SHA256'] == p['expected_frame_SHA256']
    assert large['checkpoint_SHA256'] == h['large_checkpoint_sha256']
    assert large['instructions'] == SOURCE_INSTRUCTIONS + reference['large_instruction_budget']
    assert reference['large_instruction_budget'] == 300000000
    assert 2147483648 < large['checkpoint_bytes'] <= CAP
    assert large['inherited_source_events'] == p['inherited_source_events']
    assert large['new_UART_bytes'] == 0 and large['usable_UI_or_response_claimed'] is False
    for key in ['actual_checkpoint_exceeds_previous_2GiB_host_bound',
                'full_original_native_roundtrip_and_original_C46_boundary_verified',
                'fresh_live_peripherals_verified', 'SELinux_enforcing',
                'experimental_validation_is_not_yet_primary']:
        assert large[key] is True, key


def validate_preserved_transport(p, report, manifest, pin):
    """Require pinned preservation of the validated trial without selecting its state."""
    h, large = p['host_validation'], report['full_larger_original_admission']
    assert pin['producer_run_id'] == h['run_id']
    assert pin['producer_execution_commit'] == h['execution_commit']
    assert pin['transport_manifest'] == h['large_transport_manifest']
    assert manifest['schema'] == 'reverseandroid-checkpoint-transport-v1'
    assert manifest['compression'] == 'tar+gzip'
    c, g = manifest['source_contract'], manifest['progress']
    assert c['checkpoint_sha256'] == g['checkpoint_sha256'] == large['checkpoint_SHA256']
    assert c['instructions'] == g['instructions'] == large['instructions']
    assert c['source_commit'] == h['execution_commit'] and c['epoch_original_init_verified'] is True
    assert g['source_boundary_verified'] is True
    assert g['android_boot_completed_verified'] == large['original_BOOT']
    parts = manifest['parts']
    assert 1 <= len(parts) <= 16 and len(pin['pieces']) == len(parts)
    assert [part['file'] for part in parts] == [f'checkpoint.part-{i:03d}' for i in range(len(parts))]
    for part, artifact in zip(parts, pin['pieces']):
        assert 0 < part['bytes'] <= 200 * 1024**2
        require_sha(part['sha256'])
        assert type(artifact['id']) is int and artifact['id'] > 0
        assert artifact['repository'] == 'guadalupejuan001-code/reverseAndroid'
        require_sha(artifact['sha256'])


def measured_profile_matches(p, native_sha, frame_sha, manifest_sha, producer_commit):
    assert native_sha == p['expected_native_SHA256']
    assert frame_sha == p['expected_frame_SHA256']
    assert manifest_sha == p['host_validation']['manifest_SHA256']
    assert producer_commit == p['host_validation']['execution_commit']


def validate_first_checkpoint(report, checkpoint_sha, instructions, checkpoint_bytes):
    """The first replay must reproduce the entire authenticated larger-trial state."""
    large = report['full_larger_original_admission']
    assert checkpoint_sha == large['checkpoint_SHA256']
    assert instructions == large['instructions']
    assert checkpoint_bytes == large['checkpoint_bytes']


def validate_generated_profile(validated, generated, tools_directory):
    """Ignore JSON key order only; every manifest value and program byte must match.

    The downloaded validated manifest retains its separately pinned raw SHA-256.
    Local filesystem iteration may order derived_tool_files differently when the
    same tool copies are regenerated, without changing the authenticated data.
    """
    assert generated == validated
    for name, entry in validated['derived_tool_files'].items():
        assert Path(name).name == name
        assert digest(Path(tools_directory) / name) == entry['build_SHA256']


def authenticate_profile(p, reference):
    validate_plan(p, reference)
    candidates = list(Path('build/host-validation').rglob('validated-host-transport.json'))
    assert len(candidates) == 1, 'Expected exactly one authenticated validation report'
    report_path = candidates[0]
    assert digest(report_path) == p['host_validation']['report_SHA256']
    report = read(report_path)
    validate_report(p, reference, report)
    measured_profile_matches(p, digest(NATIVE), digest(FRAME), digest(MANIFEST),
                             Path('build/host-native/execution-commit.txt').read_text().strip())
    manifest = read(MANIFEST)
    assert manifest['host_RVG_file_capacity_bytes'] == CAP
    assert manifest['original_870_files_unchanged'] is True
    assert manifest['CPU_instruction_decoder_RAM_disk_clock_seed_cadence_IRQ_RVG_RVI_MXIM_and_original_milestones_unchanged'] is True
    assert manifest['retained_IC_CLZ_extension'] == read('build/ic-native/extension-manifest.json')
    assert digest('build/ic-native/epoch-clz-ic') == OLD_NATIVE
    generated = RUNTIME / 'host-transport-manifest.json'
    validate_generated_profile(manifest, read(generated), RUNTIME / 'tools')
    lock = read(p['source_artifact_lock'])
    original = read(SOURCE_LOCK)
    assert lock['repository'] == original['repository'] == 'guadalupejuan001-code/reverseAndroid'
    for name in ['seed', 'native-cache', 'ic-native', 'media-manifest', 'media-aa', 'media-ab', 'media-ac']:
        assert lock['artifacts'][name] == original['artifacts'][name], name
    for name in ['host-native', 'host-validation']:
        pin = lock['artifacts'][name]
        assert type(pin['id']) is int and pin['id'] > 0
        require_sha(pin['sha256'])
    h = p['host_validation']
    validate_preserved_transport(p, report, read(h['large_transport_manifest']),
                                 lock['artifacts'][h['large_transport_alias']])
    return report


def validate_original_source(p, contract, gate, kernel, proof, original_preflight,
                             host_preflight, host_policy):
    assert original_preflight == host_preflight
    assert contract == original_preflight['contract']
    assert contract['checkpoint_sha256'] == gate['checkpoint_sha256'] == SOURCE_SHA
    assert contract['instructions'] == gate['instructions'] == SOURCE_INSTRUCTIONS
    assert contract['source_commit'] == SOURCE_COMMIT
    assert contract['epoch_sha256'] == 'bc5e6f91cc5befb13ba8f8e96ffb74cad22edda25586324a0f469e1b26fd5e75'
    assert gate['source_boundary_verified'] is True
    assert kernel['kernel_sha256'] == 'f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8'
    assert kernel['live_kernel_security']['selinux_enforcing'] is True and kernel['input_unchanged'] is True
    assert proof['events'] == p['inherited_source_events'] and proof['phase'] == 8
    assert proof['waiting'] is False and proof['failed'] is False
    assert proof['hardware_commands_completed'] is True
    assert proof['C88_guard_succeeded'] is True and proof['C88_priority_postprobe_checked'] is True
    assert proof['native_binary_SHA256'] == host_policy['native_binary_sha256'] == OLD_NATIVE
    assert proof['combined_original_console_bytes_sha256'] == p['source_console_SHA256']


def source_console(p):
    encoded = Path(p['source_console_file']).read_bytes()
    assert hashlib.sha256(encoded).hexdigest() == p['source_console_base64_SHA256']
    compressed = base64.b64decode(encoded.strip(), validate=True)
    assert hashlib.sha256(compressed).hexdigest() == p['source_console_gzip_SHA256']
    with gzip.GzipFile(fileobj=io.BytesIO(compressed)) as stream:
        raw = stream.read(p['source_console_bytes'] + 1)
        assert not stream.read(1)
    assert len(raw) == p['source_console_bytes']
    assert hashlib.sha256(raw).hexdigest() == p['source_console_SHA256']
    assert parse_reply(raw)['guard_succeeded']
    return raw


def prepare(p, reference):
    authenticate_profile(p, reference)
    assert not (REPORTS / 'control-state.json').exists()
    assert not (REPORTS / 'console-all.bin').exists()
    c, g = read(SOURCE / 'output-contract.json'), read(SOURCE / 'c46-progress.json')
    st = read(SOURCE / p['source_proof_file'])
    validate_original_source(p, c, g, read(SOURCE / 'kernel-observation.json'), st,
                             read(REPORTS / 'source-original-admission.json'),
                             read(REPORTS / 'source-host-profile-admission.json'),
                             read(SOURCE / 'host-policy.json'))
    assert digest(SOURCE / 'current.rvg') == SOURCE_SHA
    assert digest(SOURCE / p['source_proof_file']) == SOURCE_PROOF
    assert (SOURCE / 'source-commit.txt').read_text().strip() == SOURCE_COMMIT
    raw = source_console(p)
    console = read(SOURCE / 'console-observation.json')
    assert console['raw_console_SHA256'] == p['source_console_SHA256']
    assert verify_live_peripherals(read(SOURCE / 'peripherals-live-required.json'), console, SOURCE_SHA) is True
    assert st['live_peripherals_verified'] is True
    REPORTS.mkdir(parents=True, exist_ok=True)
    (REPORTS / 'console-all.bin').write_bytes(raw)
    (REPORTS / 'source-original-campaign-proof.json').write_bytes((SOURCE / p['source_proof_file']).read_bytes())
    state = copy.deepcopy(st)
    state.update(source=str(SOURCE), complete=False, failed=False, zero_input_continuation_campaign='c90r',
                 actual_campaign_new_events=0, current_campaign_stage=0,
                 selected_native_unchanged_from_parent=False, original_parent_proof_SHA256=SOURCE_PROOF,
                 current_native_SHA256=OLD_NATIVE, next_native_binary_SHA256=p['expected_native_SHA256'],
                 first_checkpoint_reproduces_validated_trial=False,
                 selected_host_validation_report_SHA256=p['host_validation']['report_SHA256'],
                 selected_host_extension_manifest_SHA256=p['host_validation']['manifest_SHA256'],
                 host_snapshot_capacity_bytes=CAP, original_source_checkpoint_SHA256=SOURCE_SHA)
    save(REPORTS / 'control-state.json', state)
    save(REPORTS / 'resume-proof.json', dict(
        schema='reverseandroid-c90r-explicit-host-policy-selection-v1',
        source_cycle='C90', source_stage=6, source_checkpoint_sha256=SOURCE_SHA,
        source_instructions=SOURCE_INSTRUCTIONS, original_source_native_SHA256=OLD_NATIVE,
        selected_native_SHA256=p['expected_native_SHA256'], selected_frame_SHA256=p['expected_frame_SHA256'],
        selected_native_unchanged_from_parent=False, host_snapshot_capacity_bytes=CAP,
        validation=p['host_validation'], original_epoch_preflight_passed=True,
        derived_host_epoch_preflight_equal=True, whole_RVG_SHA256_verified=True,
        all_prior_events_preserved=True, prior_console_bytes_preserved=True, new_UART_bytes=0,
        experimental_large_checkpoint_used_as_source=False, new_guest_execution_started=False,
        actual_UI_response_verified=False))
    print('C90 stage 6 recovered and admitted; validated host profile explicitly selected, zero new UART')


def run_stage(p, reference, stage):
    report = authenticate_profile(p, reference)
    assert 1 <= stage <= p['max_stages']
    state_path_ = REPORTS / 'control-state.json'
    st = read(state_path_)
    assert st['zero_input_continuation_campaign'] == 'c90r' and not st['failed'] and not st['complete']
    assert st['current_campaign_stage'] == stage - 1
    assert st['events'] == p['inherited_source_events'] and st['phase'] == 8 and st['waiting'] is False
    assert st['hardware_commands_completed'] is True and st['actual_campaign_new_events'] == 0
    assert st['selected_host_validation_report_SHA256'] == p['host_validation']['report_SHA256']
    assert st['selected_host_extension_manifest_SHA256'] == p['host_validation']['manifest_SHA256']
    source = Path(st['source'])
    c = read(source / 'output-contract.json')
    parent_native = read(source / 'host-policy.json')['native_binary_sha256']
    assert parent_native == st['native_binary_SHA256'] == (OLD_NATIVE if stage == 1 else p['expected_native_SHA256'])
    if stage == 1:
        assert source == SOURCE and c['checkpoint_sha256'] == SOURCE_SHA and c['instructions'] == SOURCE_INSTRUCTIONS
    else:
        assert st['first_checkpoint_reproduces_validated_trial'] is True
        assert source == Path(f'evidence/c90r-{stage - 1}')
        assert c['checkpoint_sha256'] == st['last_admitted_sha256']
        assert c['instructions'] == st['last_admitted_instructions']
        assert read(source / 'zero-input-proof.json') == st
    output = Path(f'evidence/c90r-{stage}')
    budget = p['initial_instruction_budget'] if stage == 1 else p['continuation_instruction_budget']
    command = ['node', '--expose-gc', '--max-old-space-size=5000',
               str(RUNTIME / 'tools/reverseandroid-epoch.mjs'), 'run', str(source), str(output),
               str(NATIVE), 'build/media/android-c27.raw', str(FRAME), os.environ['GITHUB_SHA'], str(budget), '-']
    with (REPORTS / f'stage-{stage}.json').open('x') as log:
        subprocess.run(command, stdout=log, check=True)
    g, host = read(output / 'c46-progress.json'), read(output / 'process-outcome.json')
    k, result = read(output / 'kernel-observation.json'), read(output / 'current.json')
    assert g['source_boundary_verified'] is True
    checkpoint_sha = digest(output / 'current.rvg')
    assert checkpoint_sha == g['checkpoint_sha256'] == read(output / 'output-contract.json')['checkpoint_sha256']
    if stage == 1:
        validate_first_checkpoint(report, checkpoint_sha, g['instructions'], (output / 'current.rvg').stat().st_size)
        st['first_checkpoint_reproduces_validated_trial'] = True
    assert c['instructions'] < g['instructions'] <= c['instructions'] + budget
    assert host['status'] == 'process_completed' and host['exit_code'] == 0 and host['forced_kill'] is False
    policy = read(output / 'host-policy.json')
    assert policy['native_binary_sha256'] == p['expected_native_SHA256'] and policy['host_wall_ms'] == p['host_wall_ms']
    assert k['live_kernel_security']['selinux_enforcing'] is True
    assert result['guest']['logical_ram_bytes'] == 2147483648 and result['guest']['counter_instructions_per_tick'] == 64
    provenance = result['provenance']
    assert provenance['input_checkpoint_sha256'] == c['checkpoint_sha256']
    assert provenance['host_snapshot_transport_bytes'] == CAP
    assert provenance['new_uart_input_bytes'] == 0 and 'external_uart_input' not in provenance
    transcript = REPORTS / 'console-all.bin'
    assert digest(transcript) == st['combined_original_console_bytes_sha256']
    with transcript.open('ab') as stream:
        stream.write((output / 'console-new.txt').read_bytes())
    raw = transcript.read_bytes()
    assert parse_reply(raw)['guard_succeeded']
    with (REPORTS / f'stage-{stage}-console-observer.log').open('x') as log:
        subprocess.run(['node', 'tools/reverseandroid-console-proof.mjs', str(transcript), p['ELF_SHA256'],
                        p['guest_ELF_path'], str(output / 'console-observation.json')], stdout=log, check=True)
    console = read(output / 'console-observation.json')
    assert console['original_input_bytes_unchanged'] is True
    assert console['raw_console_SHA256'] == hashlib.sha256(raw).hexdigest()
    assert console['original_C46_or_boot_or_UI_gates_modified'] is False
    probe = output / 'peripherals-live-required.json'
    with (REPORTS / f'stage-{stage}-required-peripherals.log').open('x') as log:
        checked = subprocess.run(['node', '--max-old-space-size=5000',
                                  str(RUNTIME / 'tools/reverseandroid-peripheral-probe.mjs'),
                                  str(output / 'current.rvg'), g['checkpoint_sha256'], str(probe), p['guest_ELF_path']], stdout=log)
    live = read(probe) if checked.returncode == 0 and probe.exists() else None
    hardware = verify_live_peripherals(live, console, g['checkpoint_sha256'])
    # A transient peripheral restart does not invalidate an admitted native state;
    # genuine BOOT completion still requires every fresh live peripheral check.
    st.update(source=str(output), last_admitted_sha256=g['checkpoint_sha256'],
              last_admitted_instructions=g['instructions'], current_campaign_stage=stage,
              actual_campaign_new_events=0, current_stage_new_UART_bytes=0,
              live_peripherals_verified=hardware, live_peripheral_probe_exit_code=checked.returncode,
              boot_completed=g['android_boot_completed_verified'], system_server_alive=g['system_server_alive_verified'],
              combined_original_console_bytes_sha256=hashlib.sha256(raw).hexdigest(),
              console_observation=str(output / 'console-observation.json'),
              native_binary_SHA256=p['expected_native_SHA256'], parent_native_binary_SHA256=parent_native,
              current_native_SHA256=p['expected_native_SHA256'], next_native_binary_SHA256=p['expected_native_SHA256'],
              selected_native_unchanged_from_parent=(parent_native == p['expected_native_SHA256']),
              SELinux_enforcing=True)
    st['complete'] = boot_complete(True, hardware, g)
    assert st['events'] == p['inherited_source_events'] and st['phase'] == 8 and st['waiting'] is False
    save(output / 'zero-input-proof.json', st)
    save(state_path_, st)
    shutil.rmtree(output / 'native/journal')
    with open(os.environ['GITHUB_OUTPUT'], 'a') as stream:
        stream.write('complete=' + str(st['complete']).lower() + '\n')
    print(json.dumps(dict(cycle='c90r', stage=stage, new_UART_bytes=0, original_gate=g,
                          live_peripherals_verified=hardware, complete_original_BOOT_and_peripherals=st['complete'],
                          actual_UI_response_verified=False)))


def finish(p, reference):
    authenticate_profile(p, reference)
    st = read(REPORTS / 'control-state.json')
    assert st['zero_input_continuation_campaign'] == 'c90r' and not st['failed']
    assert st['first_checkpoint_reproduces_validated_trial'] is True
    assert st['events'] == p['inherited_source_events'] and st['actual_campaign_new_events'] == 0
    assert st['phase'] == 8 and st['waiting'] is False
    source = Path(st['source'])
    g, k = read(source / 'c46-progress.json'), read(source / 'kernel-observation.json')
    assert parse_reply((REPORTS / 'console-all.bin').read_bytes())['guard_succeeded']
    hardware = verify_live_peripherals(read(source / 'peripherals-live-required.json'),
                                       read(source / 'console-observation.json'), g['checkpoint_sha256'])
    assert k['live_kernel_security']['selinux_enforcing'] is True
    assert read(source / 'host-policy.json')['native_binary_sha256'] == p['expected_native_SHA256']
    summary = dict(cycle='c90r', last_stage=st['current_campaign_stage'],
                   checkpoint_sha256=g['checkpoint_sha256'], instructions=g['instructions'],
                   original_SystemServer_live=g['system_server_alive_verified'],
                   original_BOOT_completed=g['android_boot_completed_verified'],
                   live_peripherals_verified=hardware, SELinux_enforcing=True, new_UART_bytes=0,
                   complete_original_BOOT_and_peripherals=boot_complete(True, hardware, g),
                   actual_UI_and_gesture_still_require_separate_test=True)
    save(REPORTS / 'final-summary.json', summary)
    print(json.dumps(summary))
    assert st['complete'] is True and boot_complete(True, hardware, g), 'Bounded stages ended before genuine Android BOOT'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['profile', 'prepare', 'run', 'finish'])
    parser.add_argument('--plan', default=PLAN)
    parser.add_argument('--stage', type=int)
    args = parser.parse_args()
    os.chdir(ROOT)
    p, reference = read(args.plan), read(REFERENCE)
    if args.command == 'profile':
        authenticate_profile(p, reference)
        print('Validated host binary, profile, controls, larger admission and preservation pins authenticated')
    elif args.command == 'prepare':
        prepare(p, reference)
    elif args.command == 'run':
        assert args.stage is not None
        run_stage(p, reference, args.stage)
    else:
        finish(p, reference)


if __name__ == '__main__':
    main()
