#!/usr/bin/env python3
"""C91: recover admitted C90R-5 with one explicitly selected host reader fix.

The native/frame executables and the validated host transport profile remain
unchanged. Only the derived memory-record reader changes. The failed C90R-6
tail is a reproduction target, never an admitted source or a substitute gate.
"""
import argparse
import base64
import copy
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from urllib.request import Request, urlopen

import host_transport_continuation as transport
from c88_jit_priority_control import read, save, digest, parse_reply
from c85_peripheral_completion import verify_live_peripherals, boot_complete


ROOT = Path(__file__).resolve().parents[1]
PLAN = 'state/experiments/c91-plan.json'
SOURCE_LOCK = 'state/experiments/c90r-stage-5-artifacts.json'
SOURCE = Path('build/source-c91')
REPORTS = Path('evidence/c91-reports')
RUNTIME = Path('build/host-observer-runtime')
MANIFEST = RUNTIME / 'host-observer-manifest.json'
READER = Path('extensions/host-memory-lines/rvg_memory_lines.mjs')
CAP = transport.CAP
SOURCE_SHA = '7c00c86bb9af645fe7a6f416f63c0145351234ec20d749c2bf5b3dd30f5dd802'
SOURCE_PROOF = 'c46a51a9787a26b39afeac6dfbc5cec15bf7b236c4c3ffc0946944c9c55f3654'
SOURCE_COMMIT = 'ceb722e62ebc720fa33133761a188199e91a59a5'
SOURCE_INSTRUCTIONS = 165447727998
SOURCE_RUN = 42587296800
SOURCE_NATIVE = 'a31e3fd923f63071ebadeb6a78ee5aadf77111c88186e428927220f199710318'
SOURCE_FRAME = '540440a1f76baec7a955deab2eeee6431a2b656c56bcca90246c6a3a77f1cd7e'
SOURCE_KERNEL = 'efa9685eb258c21551968e10c3a57d5d02231720a177bced5bc2bd7ec29750e8'
SOURCE_PERIPHERALS = 'dbf2fc6229823657c9e38ce8572d1ec0d0aa8c0e5693533522a6a2bd3ceca930'
TAIL_SHA = '7e0135bad3a7c61f12e48d6f97b4c60a89c1848b9e03c464895a5b861bbd612b'
TAIL_INSTRUCTIONS = 166447727998
EPOCH_SHA = 'bc5e6f91cc5befb13ba8f8e96ffb74cad22edda25586324a0f469e1b26fd5e75'
KERNEL_SHA = 'f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8'
BUDGET = 1000000000
PROFILE_SCHEMA = 'reverseandroid-explicit-host-memory-line-reader-v1'


def validate_plan(p, base, reference, source_lock):
    """Select C90R-5 explicitly while retaining the original v1 validation recipe."""
    transport.validate_plan(base, reference)
    assert p['schema'] == 'reverseandroid-host-observer-zero-input-continuation-plan-v1'
    assert p['cycle'] == 'c91' and p['source_cycle'] == 'C90R' and p['source_stage'] == 5
    assert p['source_checkpoint_sha256'] == SOURCE_SHA
    assert p['source_instructions'] == SOURCE_INSTRUCTIONS
    assert p['source_execution_commit'] == SOURCE_COMMIT and p['source_run_id'] == SOURCE_RUN
    assert p['source_proof_file'] == 'zero-input-proof.json' and p['source_proof_SHA256'] == SOURCE_PROOF
    assert p['source_original_artifact_lock'] == SOURCE_LOCK
    assert p['source_artifact_lock'] == 'state/experiments/c91-artifacts.json'
    transport.require_sha(p['source_artifact_lock_SHA256'])
    assert p['base_host_profile_plan'] == transport.PLAN
    assert p['base_host_transport_reference'] == transport.REFERENCE
    assert p['host_validation'] == base['host_validation']
    assert p['source_native_SHA256'] == p['expected_native_SHA256'] == base['expected_native_SHA256'] == SOURCE_NATIVE
    assert p['expected_frame_SHA256'] == base['expected_frame_SHA256'] == SOURCE_FRAME
    assert p['host_snapshot_capacity_bytes'] == base['host_snapshot_capacity_bytes'] == CAP
    assert p['observer_profile_schema'] == PROFILE_SCHEMA and p['observer_reader_source'] == str(READER)
    for key in ['expected_observer_reader_SHA256', 'expected_observer_manifest_SHA256']:
        transport.require_sha(p[key])
    assert p['max_record_bytes'] == 16384 and p['max_allowed_record_bound_bytes'] == 65536
    assert p['source_kernel_observation_SHA256'] == SOURCE_KERNEL
    assert p['source_peripherals_SHA256'] == SOURCE_PERIPHERALS
    assert type(p['new_UART_bytes']) is int and p['new_UART_bytes'] == 0
    assert 'input_file' not in p and 'input_plan_file' not in p
    assert p['initial_instruction_budget'] == p['continuation_instruction_budget'] == BUDGET
    assert p['host_wall_ms'] == 900000 and p['max_stages'] == 16
    for key in ['fresh_hardware_power_cycle', 'guest_snapshot_patching', 'guest_property_injection']:
        assert p[key] is False
    assert p['original_CPU_codecs_RAM_disk_seed_clock_cadence_IRQ_preserved'] is True
    assert p['original_BOOT_and_UI_not_yet_claimed'] is True
    assert p['inherited_source_events'] == base['inherited_source_events'] == reference['inherited_source_events']
    assert len(p['inherited_source_events']) == 8
    assert [event['phase'] for event in p['inherited_source_events']] == list(range(8))
    source = source_lock['source']
    assert source['cycle'] == 'C90R' and source['stage'] == 5
    assert source['checkpoint_sha256'] == SOURCE_SHA and source['instructions'] == SOURCE_INSTRUCTIONS
    assert source['proof_file'] == p['source_proof_file'] and source['proof_SHA256'] == SOURCE_PROOF
    for field, key in [('file', 'source_console_file'), ('base64_SHA256', 'source_console_base64_SHA256'),
                       ('gzip_SHA256', 'source_console_gzip_SHA256'), ('bytes', 'source_console_bytes'),
                       ('SHA256', 'source_console_SHA256')]:
        assert p[key] == source['entire_console'][field], key
    transport.state_path(p['source_console_file'])
    assert 0 < p['source_console_bytes'] <= 64 * 1024**2
    for key in ['ELF_SHA256', 'guest_ELF_path']:
        assert p[key] == base[key] == reference[key]
    assert p['first_checkpoint_reproduction'] == dict(
        diagnostic_only=True, failed_tail_used_as_source=False, cycle='C90R', stage=6,
        producer_run_id=SOURCE_RUN, producer_execution_commit=SOURCE_COMMIT,
        source_checkpoint_sha256=SOURCE_SHA, checkpoint_sha256=TAIL_SHA,
        instructions=TAIL_INSTRUCTIONS, instruction_budget=BUDGET)


def validate_source_lock(p, lock, original, source_transport):
    """The new lock is an exact immutable copy, including all native/media pins."""
    assert lock == original
    assert lock['repository'] == 'guadalupejuan001-code/reverseAndroid'
    pin = lock['artifacts']['seed']
    assert pin['producer_run_id'] == SOURCE_RUN and pin['producer_execution_commit'] == SOURCE_COMMIT
    assert pin['transport_manifest'] == 'state/experiments/c90r-stage-5-transport.json'
    assert source_transport['schema'] == 'reverseandroid-checkpoint-transport-v1'
    assert source_transport['compression'] == 'tar+gzip'
    c, g = source_transport['source_contract'], source_transport['progress']
    assert c['checkpoint_sha256'] == g['checkpoint_sha256'] == SOURCE_SHA
    assert c['instructions'] == g['instructions'] == SOURCE_INSTRUCTIONS
    assert c['source_commit'] == SOURCE_COMMIT and c['epoch_original_init_verified'] is True
    assert g['source_boundary_verified'] is True and g['system_server_alive_verified'] is True
    assert g['android_boot_completed_verified'] is False
    parts = source_transport['parts']
    assert len(parts) == len(pin['pieces']) == 4
    assert [part['file'] for part in parts] == [f'checkpoint.part-{i:03d}' for i in range(4)]
    for part, artifact in zip(parts, pin['pieces']):
        assert 0 < part['bytes'] <= 200 * 1024**2
        transport.require_sha(part['sha256'])
        assert type(artifact['id']) is int and artifact['id'] > 0
        assert artifact['repository'] == lock['repository']
        transport.require_sha(artifact['sha256'])
    assert lock['epoch_native_binary_SHA256'] == p['expected_native_SHA256']
    assert lock['epoch_frame_SHA256'] == p['expected_frame_SHA256']
    assert lock['host_snapshot_capacity_bytes'] == CAP


def measured_profile_matches(p, native_sha, frame_sha, manifest_sha, reader_sha):
    assert native_sha == p['expected_native_SHA256'] == SOURCE_NATIVE
    assert frame_sha == p['expected_frame_SHA256'] == SOURCE_FRAME
    assert manifest_sha == p['expected_observer_manifest_SHA256']
    assert reader_sha == p['expected_observer_reader_SHA256']


def validate_observer_manifest(p, validated, observer, tools_directory):
    """Require the entire profile and every tool byte; only one file may differ."""
    original = validated['derived_tool_files']['rvg_memory_lines.mjs']
    assert original['source_SHA256'] == original['build_SHA256']
    assert p['expected_observer_reader_SHA256'] != original['build_SHA256']
    files = copy.deepcopy(validated['derived_tool_files'])
    files['rvg_memory_lines.mjs']['build_SHA256'] = p['expected_observer_reader_SHA256']
    h = p['host_validation']
    expected = dict(
        schema=PROFILE_SCHEMA, baseline_validation_run_id=h['run_id'],
        baseline_validation_commit=h['execution_commit'], baseline_manifest_SHA256=h['manifest_SHA256'],
        native_SHA256=p['expected_native_SHA256'], frame_SHA256=p['expected_frame_SHA256'],
        host_snapshot_capacity_bytes=CAP, original_870_files_unchanged=True,
        original_rvg_memory_lines_SHA256=original['build_SHA256'],
        replacement_rvg_memory_lines_SHA256=p['expected_observer_reader_SHA256'],
        max_record_bytes=16384, max_allowed_record_bound_bytes=65536,
        changed_tool_files=['rvg_memory_lines.mjs'], derived_tool_files=files)
    assert observer == expected
    tools_directory = Path(tools_directory)
    assert {path.name for path in tools_directory.iterdir() if path.is_file()} == set(files)
    for name, entry in files.items():
        assert Path(name).name == name
        assert digest(tools_directory / name) == entry['build_SHA256'], name


def authenticate_profile(p, base, reference):
    original = read(SOURCE_LOCK)
    validate_plan(p, base, reference, original)
    # This call deliberately uses the ORIGINAL C90R and transport plans. No v1
    # globals, source pins, validated tools, or historical proof are rewritten.
    transport.authenticate_profile(base, reference)
    assert digest(SOURCE_LOCK) == digest(p['source_artifact_lock']) == p['source_artifact_lock_SHA256']
    lock = read(p['source_artifact_lock'])
    validate_source_lock(p, lock, original, read(lock['artifacts']['seed']['transport_manifest']))
    measured_profile_matches(p, digest(transport.NATIVE), digest(transport.FRAME), digest(MANIFEST), digest(READER))
    validated, manifest = read(transport.MANIFEST), read(MANIFEST)
    assert digest(RUNTIME / 'baseline-host-transport-manifest.json') == p['host_validation']['manifest_SHA256']
    assert digest('tools/rvg_memory_lines.mjs') == validated['derived_tool_files']['rvg_memory_lines.mjs']['build_SHA256']
    validate_observer_manifest(p, validated, manifest, RUNTIME / 'tools')
    for name in ['state', 'tests']:
        assert (RUNTIME / name).resolve() == (ROOT / name).resolve()
    subprocess.run([sys.executable, 'scripts/verify-reverseandroid-import.py'],
                   check=True, stdout=subprocess.DEVNULL)
    return manifest


def validate_source_bytes(p, checkpoint_sha, proof_sha, execution_commit):
    assert checkpoint_sha == p['source_checkpoint_sha256'] == SOURCE_SHA
    assert proof_sha == p['source_proof_SHA256'] == SOURCE_PROOF
    assert execution_commit == p['source_execution_commit'] == SOURCE_COMMIT


def validate_source_lease(p, latest, continuation):
    """Check the selected repository frontier once, before any C91 execution."""
    assert latest['cycle'] == continuation['cycle'] == 'C90R'
    assert latest['stage'] == continuation['current_stage'] == 5
    assert latest['checkpoint_sha256'] == continuation['current_source_checkpoint_sha256'] == SOURCE_SHA
    assert latest['instructions'] == continuation['current_source_instructions'] == SOURCE_INSTRUCTIONS
    assert latest['source_artifact_lock'] == continuation['current_source_artifact_lock'] == SOURCE_LOCK
    assert latest['current_source_proof_SHA256'] == continuation['current_source_proof_SHA256'] == SOURCE_PROOF
    assert latest['execution_commit'] == continuation['current_source_execution_commit'] == SOURCE_COMMIT
    assert latest['source_boundary_verified'] is True and latest['android_boot_completed_verified'] is False
    assert latest['native_binary_SHA256'] == continuation['current_source_native_SHA256'] == SOURCE_NATIVE
    assert latest['total_campaign_ledger_events'] == 8 and continuation['current_input_phase'] == 8
    assert continuation['current_input_waiting'] is False
    for pointer in [latest, continuation]:
        assert pointer['next_explicit_campaign'] == 'C91' and pointer['next_plan'] == PLAN
        assert pointer['pending_C91_first_admission'] is True
        assert pointer['current_source_entire_console']['SHA256'] == p['source_console_SHA256']
        assert pointer['current_source_entire_console']['bytes'] == p['source_console_bytes']
        assert pointer['next_host_observer_profile']['manifest_SHA256'] == p['expected_observer_manifest_SHA256']
        assert pointer['next_host_observer_profile']['reader_SHA256'] == p['expected_observer_reader_SHA256']


def acquire_source_lease(p):
    """Read both live canonical pointers at one remote main commit.

    A checkout-only check would allow rerunning an obsolete workflow commit
    after main has advanced. This read-only lease is acquired just once before
    execution; subsequent checkpoint promotions do not interrupt the campaign.
    """
    repository = 'guadalupejuan001-code/reverseAndroid'
    assert os.environ['GITHUB_REPOSITORY'] == repository
    assert os.environ['GITHUB_REF'] == 'refs/heads/main'
    token = os.environ['GH_TOKEN']

    def api(path):
        request = Request('https://api.github.com/repos/' + repository + '/' + path,
                          headers={'Authorization': 'Bearer ' + token, 'Accept': 'application/vnd.github+json',
                                   'X-GitHub-Api-Version': '2022-11-28'})
        with urlopen(request, timeout=60) as response:
            payload = response.read(1024 * 1024 + 1)
        assert len(payload) <= 1024 * 1024
        return json.loads(payload)

    head = api('git/ref/heads/main')['object']['sha']
    transport.require_sha(head, 40)
    records = {}
    for name in ['latest', 'continuation']:
        content = api(f'contents/state/{name}.json?ref={head}')
        assert content['encoding'] == 'base64' and content['path'] == f'state/{name}.json'
        raw = base64.b64decode(''.join(content['content'].split()), validate=True)
        assert len(raw) == content['size']
        records[name] = raw
    validate_source_lease(p, json.loads(records['latest']), json.loads(records['continuation']))
    REPORTS.mkdir(parents=True, exist_ok=True)
    for name, raw in records.items():
        with (REPORTS / f'source-lease-{name}.json').open('xb') as output:
            output.write(raw)
    lease = dict(schema='reverseandroid-c91-source-lease-v1', repository=repository,
                 observed_main_commit=head, observed_UTC=datetime.now(timezone.utc).isoformat(),
                 workflow_execution_commit=os.environ['GITHUB_SHA'],
                 source_checkpoint_sha256=SOURCE_SHA, source_instructions=SOURCE_INSTRUCTIONS,
                 latest_JSON_SHA256=hashlib.sha256(records['latest']).hexdigest(),
                 continuation_JSON_SHA256=hashlib.sha256(records['continuation']).hexdigest(),
                 both_live_pointers_select_same_admitted_source=True)
    save(REPORTS / 'source-lease.json', lease)
    return lease


def validate_source_observations(p, original_kernel, observed_kernel, original_probe, observed_probe):
    assert hashlib.sha256(original_kernel).hexdigest() == p['source_kernel_observation_SHA256']
    assert hashlib.sha256(original_probe).hexdigest() == p['source_peripherals_SHA256']
    assert original_kernel == observed_kernel
    assert original_probe == observed_probe
    # Equality covers every field, including errors and provenance, not merely
    # success booleans or a subset of process/channel observations.
    assert json.loads(original_kernel) == json.loads(observed_kernel)
    assert json.loads(original_probe) == json.loads(observed_probe)


def validate_original_source(p, contract, gate, kernel, proof, baseline_preflight, observer_preflight, policy):
    assert baseline_preflight == observer_preflight
    assert contract == baseline_preflight['contract']
    assert contract['checkpoint_sha256'] == gate['checkpoint_sha256'] == kernel['checkpoint_sha256'] == SOURCE_SHA
    assert contract['instructions'] == gate['instructions'] == kernel['instructions'] == SOURCE_INSTRUCTIONS
    assert contract['source_commit'] == policy['controller_commit'] == SOURCE_COMMIT
    assert contract['epoch_sha256'] == EPOCH_SHA and contract['epoch_original_init_verified'] is True
    assert gate['source_boundary_verified'] is True and gate['system_server_alive_verified'] is True
    assert gate['android_boot_completed_verified'] is False
    assert kernel['kernel_sha256'] == KERNEL_SHA and kernel['input_unchanged'] is True
    assert kernel['live_kernel_security']['selinux_enforcing'] is True
    assert proof['events'] == p['inherited_source_events'] and proof['phase'] == 8
    for key in ['waiting', 'failed', 'complete', 'boot_completed']:
        assert proof[key] is False, key
    for key in ['hardware_commands_completed', 'C88_guard_succeeded', 'C88_priority_postprobe_checked',
                'live_peripherals_verified', 'system_server_alive', 'SELinux_enforcing',
                'first_checkpoint_reproduces_validated_trial', 'selected_native_unchanged_from_parent']:
        assert proof[key] is True, key
    assert proof['zero_input_continuation_campaign'] == 'c90r' and proof['current_campaign_stage'] == 5
    assert proof['actual_campaign_new_events'] == proof['current_stage_new_UART_bytes'] == 0
    assert proof['last_admitted_sha256'] == SOURCE_SHA and proof['last_admitted_instructions'] == SOURCE_INSTRUCTIONS
    for key in ['native_binary_SHA256', 'current_native_SHA256', 'next_native_binary_SHA256']:
        assert proof[key] == p['source_native_SHA256'] == SOURCE_NATIVE, key
    assert proof['selected_host_validation_report_SHA256'] == p['host_validation']['report_SHA256']
    assert proof['selected_host_extension_manifest_SHA256'] == p['host_validation']['manifest_SHA256']
    assert proof['combined_original_console_bytes_sha256'] == p['source_console_SHA256']
    assert policy['native_binary_sha256'] == SOURCE_NATIVE and policy['host_wall_ms'] == p['host_wall_ms']
    assert policy['logical_RAM_clock_seed_cadence_IRQ_and_codecs_modified'] is False
    assert policy['external_input_plan_present'] is False


def prepare(p, base, reference):
    validate_source_lease(p, read('state/latest.json'), read('state/continuation.json'))
    authenticate_profile(p, base, reference)
    assert not (REPORTS / 'control-state.json').exists() and not (REPORTS / 'console-all.bin').exists()
    lease = acquire_source_lease(p)
    c, g = read(SOURCE / 'output-contract.json'), read(SOURCE / 'c46-progress.json')
    st = read(SOURCE / p['source_proof_file'])
    baseline = read(REPORTS / 'source-baseline-admission.json')
    assert baseline['epoch'] == read(SOURCE / 'epoch.json')
    assert baseline['report'] == read(SOURCE / c['report_file'])
    seed = read(read(p['source_artifact_lock'])['artifacts']['seed']['transport_manifest'])
    assert c == seed['source_contract'] and g == seed['progress']
    validate_original_source(p, c, g, read(SOURCE / 'kernel-observation.json'), st, baseline,
                             read(REPORTS / 'source-observer-admission.json'), read(SOURCE / 'host-policy.json'))
    validate_source_bytes(p, digest(SOURCE / 'current.rvg'), digest(SOURCE / p['source_proof_file']),
                          (SOURCE / 'source-commit.txt').read_text().strip())
    validate_source_observations(p, (SOURCE / 'kernel-observation.json').read_bytes(),
                                  (REPORTS / 'source-observer-kernel.json').read_bytes(),
                                  (SOURCE / 'peripherals-live-required.json').read_bytes(),
                                  (REPORTS / 'source-observer-peripherals.json').read_bytes())
    raw = transport.source_console(p)
    console = read(SOURCE / 'console-observation.json')
    assert console['raw_console_SHA256'] == p['source_console_SHA256']
    assert console['raw_console_bytes'] == p['source_console_bytes']
    assert verify_live_peripherals(read(REPORTS / 'source-observer-peripherals.json'), console, SOURCE_SHA) is True
    REPORTS.mkdir(parents=True, exist_ok=True)
    (REPORTS / 'console-all.bin').write_bytes(raw)
    (REPORTS / 'source-original-campaign-proof.json').write_bytes((SOURCE / p['source_proof_file']).read_bytes())
    historical = {key: copy.deepcopy(st[key]) for key in [
        'zero_input_continuation_campaign', 'current_campaign_stage', 'original_parent_proof_SHA256',
        'original_source_checkpoint_SHA256', 'first_checkpoint_reproduces_validated_trial',
        'selected_host_extension_manifest_SHA256', 'selected_host_validation_report_SHA256']}
    state = copy.deepcopy(st)
    # The C90R first-stage trial remains a historical fact, with its original
    # proof preserved byte for byte. C91 has a different first-stage obligation.
    del state['first_checkpoint_reproduces_validated_trial']
    state.update(source=str(SOURCE), complete=False, failed=False, zero_input_continuation_campaign='c91',
                 actual_campaign_new_events=0, current_campaign_stage=0, current_stage_new_UART_bytes=0,
                 selected_native_unchanged_from_parent=True, original_parent_proof_SHA256=SOURCE_PROOF,
                 native_binary_SHA256=SOURCE_NATIVE, current_native_SHA256=SOURCE_NATIVE,
                 next_native_binary_SHA256=SOURCE_NATIVE, original_source_checkpoint_SHA256=SOURCE_SHA,
                 historical_parent_campaign=historical, first_checkpoint_reproduces_prior_failed_tail=False,
                 failed_tail_used_as_source=False, original_source_observations_reproduced_byte_for_byte=True,
                 selected_host_validation_report_SHA256=p['host_validation']['report_SHA256'],
                 selected_host_transport_manifest_SHA256=p['host_validation']['manifest_SHA256'],
                 selected_host_extension_manifest_SHA256=p['expected_observer_manifest_SHA256'],
                 selected_host_observer_manifest_SHA256=p['expected_observer_manifest_SHA256'],
                 selected_host_observer_reader_SHA256=p['expected_observer_reader_SHA256'],
                 host_observer_profile_schema=PROFILE_SCHEMA, host_snapshot_capacity_bytes=CAP,
                 admitted_parent_primary=SOURCE_LOCK, continuation_is_primary=False,
                 repository_promotion_requires_preserved_checkpoint=True)
    save(REPORTS / 'control-state.json', state)
    save(REPORTS / 'resume-proof.json', dict(
        schema='reverseandroid-c91-explicit-host-observer-selection-v1',
        source_cycle='C90R', source_stage=5, source_checkpoint_sha256=SOURCE_SHA,
        source_instructions=SOURCE_INSTRUCTIONS, original_parent_proof_SHA256=SOURCE_PROOF,
        original_source_native_SHA256=SOURCE_NATIVE, selected_native_SHA256=SOURCE_NATIVE,
        selected_frame_SHA256=SOURCE_FRAME, selected_native_unchanged_from_parent=True,
        host_snapshot_capacity_bytes=CAP, historical_host_validation=p['host_validation'],
        selected_observer_manifest_SHA256=p['expected_observer_manifest_SHA256'],
        selected_observer_reader_SHA256=p['expected_observer_reader_SHA256'],
        baseline_host_epoch_preflight_passed=True, derived_observer_epoch_preflight_equal=True,
        entire_original_kernel_observation_byte_identical=True,
        entire_original_live_peripheral_probe_byte_identical=True,
        canonical_source_lease_checked_before_execution=True,
        canonical_source_lease=lease,
        whole_RVG_SHA256_verified=True, all_prior_events_preserved=True,
        prior_console_bytes_preserved=True, new_UART_bytes=0, failed_tail_used_as_source=False,
        failed_tail_is_only_a_reproduction_target=p['first_checkpoint_reproduction'],
        new_guest_execution_started=False, actual_UI_response_verified=False))
    print('Admitted C90R stage 5 recovered; unchanged native and explicit host reader selected, zero new UART')


def validate_control_state(p, st):
    assert st['zero_input_continuation_campaign'] == 'c91' and st['failed'] is False
    assert st['events'] == p['inherited_source_events'] and st['phase'] == 8 and st['waiting'] is False
    assert st['hardware_commands_completed'] is True and st['actual_campaign_new_events'] == 0
    assert st['current_stage_new_UART_bytes'] == 0
    assert st['original_parent_proof_SHA256'] == SOURCE_PROOF and st['original_source_checkpoint_SHA256'] == SOURCE_SHA
    assert st['failed_tail_used_as_source'] is False and st['original_source_observations_reproduced_byte_for_byte'] is True
    for key in ['native_binary_SHA256', 'current_native_SHA256', 'next_native_binary_SHA256']:
        assert st[key] == p['expected_native_SHA256'] == SOURCE_NATIVE
    assert st['selected_native_unchanged_from_parent'] is True
    assert st['selected_host_validation_report_SHA256'] == p['host_validation']['report_SHA256']
    assert st['selected_host_transport_manifest_SHA256'] == p['host_validation']['manifest_SHA256']
    assert st['selected_host_extension_manifest_SHA256'] == p['expected_observer_manifest_SHA256']
    assert st['selected_host_observer_manifest_SHA256'] == p['expected_observer_manifest_SHA256']
    assert st['selected_host_observer_reader_SHA256'] == p['expected_observer_reader_SHA256']
    assert st['host_observer_profile_schema'] == PROFILE_SCHEMA and st['host_snapshot_capacity_bytes'] == CAP


def validate_first_checkpoint(p, checkpoint_sha, instructions):
    target = p['first_checkpoint_reproduction']
    assert target['failed_tail_used_as_source'] is False and target['diagnostic_only'] is True
    assert checkpoint_sha == target['checkpoint_sha256'] == TAIL_SHA
    assert instructions == target['instructions'] == SOURCE_INSTRUCTIONS + BUDGET == TAIL_INSTRUCTIONS


def validate_stage_result(p, stage, parent, contract, gate, kernel, current, host, policy,
                          checkpoint_sha, checkpoint_bytes, execution_commit):
    assert contract['checkpoint_sha256'] == gate['checkpoint_sha256'] == checkpoint_sha
    assert kernel['checkpoint_sha256'] == current['checkpoint_sha256'] == checkpoint_sha
    assert contract['instructions'] == gate['instructions'] == kernel['instructions'] == current['guest']['instructions']
    assert contract['source_commit'] == policy['controller_commit'] == execution_commit
    assert contract['epoch_sha256'] == parent['epoch_sha256'] == EPOCH_SHA
    assert contract['epoch_original_init_verified'] is True and contract['initialized_hardware_root'] is False
    assert gate['source_boundary_verified'] is True
    assert type(gate['system_server_alive_verified']) is bool and type(gate['android_boot_completed_verified']) is bool
    assert parent['instructions'] < gate['instructions'] <= parent['instructions'] + BUDGET
    assert 0 < checkpoint_bytes <= CAP
    assert current['initial_instructions'] == parent['instructions']
    assert current['advanced_instructions'] == gate['instructions'] - parent['instructions']
    assert current['stop_reason'] in ['instruction_budget', 'host_wall_budget'] and current['error'] == ''
    assert host['status'] == 'process_completed' and host['exit_code'] == 0 and host['forced_kill'] is False
    assert host['trigger'] is None and host['checkpoint_bytes_modified_by_supervisor'] is False
    assert policy['native_binary_sha256'] == p['expected_native_SHA256'] == SOURCE_NATIVE
    assert policy['instruction_budget'] == BUDGET and policy['host_wall_ms'] == p['host_wall_ms']
    assert policy['logical_RAM_clock_seed_cadence_IRQ_and_codecs_modified'] is False
    assert policy['external_input_plan_present'] is False
    assert kernel['kernel_sha256'] == KERNEL_SHA and kernel['input_unchanged'] is True
    assert kernel['live_kernel_security']['selinux_enforcing'] is True
    assert current['guest']['logical_ram_bytes'] == 2147483648
    assert current['guest']['counter_instructions_per_tick'] == 64
    provenance = current['provenance']
    assert provenance['input_checkpoint_sha256'] == parent['checkpoint_sha256']
    assert provenance['input_report_sha256'] == parent['report_sha256']
    assert provenance['host_snapshot_transport_bytes'] == CAP
    assert provenance['snapshot_round_trip_verified'] is True
    assert provenance['source_cpu_modified_before_resume'] is False
    assert provenance['live_android_properties_written'] is False
    assert provenance['new_uart_input_bytes'] == 0 and 'external_uart_input' not in provenance
    if stage == 1:
        assert parent['checkpoint_sha256'] == SOURCE_SHA and parent['instructions'] == SOURCE_INSTRUCTIONS
        assert checkpoint_bytes > 2147483648
        validate_first_checkpoint(p, checkpoint_sha, gate['instructions'])


def require_first_peripheral_validation(stage, probe_exit_code, live_verified):
    if stage == 1:
        assert probe_exit_code == 0 and live_verified is True, 'C91 reader correction requires fresh live peripherals'


def run_stage(p, base, reference, stage):
    authenticate_profile(p, base, reference)
    assert 1 <= stage <= p['max_stages']
    state_file = REPORTS / 'control-state.json'
    st = read(state_file)
    validate_control_state(p, st)
    assert st['complete'] is False and st['current_campaign_stage'] == stage - 1
    source = Path(st['source'])
    parent = read(source / 'output-contract.json')
    parent_native = read(source / 'host-policy.json')['native_binary_sha256']
    assert parent_native == SOURCE_NATIVE
    if stage == 1:
        assert source == SOURCE and parent['checkpoint_sha256'] == SOURCE_SHA
        assert parent['instructions'] == SOURCE_INSTRUCTIONS
        assert digest(source / p['source_proof_file']) == SOURCE_PROOF
    else:
        assert st['first_checkpoint_reproduces_prior_failed_tail'] is True
        assert source == Path(f'evidence/c91-{stage - 1}')
        assert parent['checkpoint_sha256'] == st['last_admitted_sha256']
        assert parent['instructions'] == st['last_admitted_instructions']
        assert read(source / 'zero-input-proof.json') == st
    output = Path(f'evidence/c91-{stage}')
    execution_commit = os.environ['GITHUB_SHA']
    command = ['node', '--expose-gc', '--max-old-space-size=5000',
               str(RUNTIME / 'tools/reverseandroid-epoch.mjs'), 'run', str(source), str(output),
               str(transport.NATIVE), 'build/media/android-c27.raw', str(transport.FRAME),
               execution_commit, str(BUDGET), '-', str(p['host_wall_ms'])]
    with (REPORTS / f'stage-{stage}.json').open('x') as log:
        subprocess.run(command, stdout=log, check=True)
    gate, host = read(output / 'c46-progress.json'), read(output / 'process-outcome.json')
    kernel, current = read(output / 'kernel-observation.json'), read(output / 'current.json')
    contract, policy = read(output / 'output-contract.json'), read(output / 'host-policy.json')
    assert read(output / 'input-contract.json') == parent
    assert (output / 'source-commit.txt').read_text().strip() == execution_commit
    for name, key in [('current.json', 'report_sha256'), ('result.json', 'result_sha256'),
                      ('process-outcome.json', 'process_sha256'), ('epoch.json', 'epoch_sha256')]:
        assert digest(output / name) == contract[key], name
    checkpoint_sha = digest(output / 'current.rvg')
    validate_stage_result(p, stage, parent, contract, gate, kernel, current, host, policy,
                          checkpoint_sha, (output / 'current.rvg').stat().st_size, execution_commit)
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
                                  str(output / 'current.rvg'), checkpoint_sha, str(probe), p['guest_ELF_path']], stdout=log)
    live = read(probe) if checked.returncode == 0 and probe.exists() else None
    hardware = verify_live_peripherals(live, console, checkpoint_sha)
    require_first_peripheral_validation(stage, checked.returncode, hardware)
    # After the first correction-validation stage, retain C90R semantics: a
    # transient HAL restart is recorded, and prevents completion, without
    # discarding an otherwise admitted native checkpoint.
    st.update(source=str(output), last_admitted_sha256=checkpoint_sha,
              last_admitted_instructions=gate['instructions'], current_campaign_stage=stage,
              actual_campaign_new_events=0, current_stage_new_UART_bytes=0,
              first_checkpoint_reproduces_prior_failed_tail=True,
              live_peripherals_verified=hardware, live_peripheral_probe_exit_code=checked.returncode,
              boot_completed=gate['android_boot_completed_verified'], system_server_alive=gate['system_server_alive_verified'],
              combined_original_console_bytes_sha256=hashlib.sha256(raw).hexdigest(),
              console_observation=str(output / 'console-observation.json'),
              native_binary_SHA256=SOURCE_NATIVE, parent_native_binary_SHA256=parent_native,
              current_native_SHA256=SOURCE_NATIVE, next_native_binary_SHA256=SOURCE_NATIVE,
              selected_native_unchanged_from_parent=True, SELinux_enforcing=True)
    st['complete'] = boot_complete(True, hardware, gate)
    validate_control_state(p, st)
    shutil.copyfile(MANIFEST, output / 'host-observer-manifest.json')
    shutil.copyfile(transport.MANIFEST, output / 'baseline-host-transport-manifest.json')
    save(output / 'zero-input-proof.json', st)
    save(state_file, st)
    shutil.rmtree(output / 'native/journal')
    with open(os.environ['GITHUB_OUTPUT'], 'a') as stream:
        stream.write('complete=' + str(st['complete']).lower() + '\n')
    print(json.dumps(dict(cycle='c91', stage=stage, new_UART_bytes=0, original_gate=gate,
                          native_unchanged_from_parent=True, first_checkpoint_reproduction_verified=True,
                          live_peripherals_verified=hardware, complete_original_BOOT_and_peripherals=st['complete'],
                          actual_UI_response_verified=False)))


def finish(p, base, reference):
    authenticate_profile(p, base, reference)
    st = read(REPORTS / 'control-state.json')
    validate_control_state(p, st)
    assert st['first_checkpoint_reproduces_prior_failed_tail'] is True
    assert 1 <= st['current_campaign_stage'] <= p['max_stages']
    source = Path(st['source'])
    assert source == Path(f"evidence/c91-{st['current_campaign_stage']}")
    assert read(source / 'zero-input-proof.json') == st
    gate, kernel = read(source / 'c46-progress.json'), read(source / 'kernel-observation.json')
    assert gate['checkpoint_sha256'] == st['last_admitted_sha256'] == kernel['checkpoint_sha256']
    assert gate['instructions'] == st['last_admitted_instructions'] == kernel['instructions']
    raw = (REPORTS / 'console-all.bin').read_bytes()
    assert hashlib.sha256(raw).hexdigest() == st['combined_original_console_bytes_sha256']
    assert parse_reply(raw)['guard_succeeded']
    probe = source / 'peripherals-live-required.json'
    live = read(probe) if st['live_peripheral_probe_exit_code'] == 0 and probe.exists() else None
    hardware = verify_live_peripherals(live, read(source / 'console-observation.json'), gate['checkpoint_sha256'])
    assert kernel['live_kernel_security']['selinux_enforcing'] is True
    assert read(source / 'host-policy.json')['native_binary_sha256'] == SOURCE_NATIVE
    summary = dict(cycle='c91', last_stage=st['current_campaign_stage'],
                   checkpoint_sha256=gate['checkpoint_sha256'], instructions=gate['instructions'],
                   original_SystemServer_live=gate['system_server_alive_verified'],
                   original_BOOT_completed=gate['android_boot_completed_verified'],
                   live_peripherals_verified=hardware, SELinux_enforcing=True, new_UART_bytes=0,
                   native_unchanged_from_parent=True, failed_tail_used_as_source=False,
                   first_checkpoint_reproduces_prior_failed_tail=True,
                   selected_host_observer_manifest_SHA256=p['expected_observer_manifest_SHA256'],
                   complete_original_BOOT_and_peripherals=boot_complete(True, hardware, gate),
                   actual_UI_and_gesture_still_require_separate_test=True)
    save(REPORTS / 'final-summary.json', summary)
    print(json.dumps(summary))
    assert st['complete'] is True and boot_complete(True, hardware, gate), 'Bounded stages ended before genuine Android BOOT'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['profile', 'prepare', 'run', 'finish'])
    parser.add_argument('--plan', default=PLAN)
    parser.add_argument('--stage', type=int)
    args = parser.parse_args()
    os.chdir(ROOT)
    p, base, reference = read(args.plan), read(transport.PLAN), read(transport.REFERENCE)
    if args.command == 'profile':
        authenticate_profile(p, base, reference)
        print('Original validation, unchanged native/frame, complete v2 profile and C90R-5 source pins authenticated')
    elif args.command == 'prepare':
        prepare(p, base, reference)
    elif args.command == 'run':
        assert args.stage is not None
        run_stage(p, base, reference, args.stage)
    else:
        finish(p, base, reference)


if __name__ == '__main__':
    main()
