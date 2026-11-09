"""C91 authority regressions; fixtures never claim native Android execution."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import host_observer_continuation as observer
from c85_peripheral_completion import boot_complete


def fixtures():
    read = lambda path: json.loads((ROOT / path).read_text())
    return (read(observer.PLAN), read('state/experiments/c90r-plan.json'),
            read('state/experiments/host-transport-plan.json'), read(observer.SOURCE_LOCK))


def source_records(plan):
    contract = dict(checkpoint_sha256=observer.SOURCE_SHA, instructions=observer.SOURCE_INSTRUCTIONS,
                    source_commit=observer.SOURCE_COMMIT, epoch_sha256=observer.EPOCH_SHA,
                    epoch_original_init_verified=True, report_sha256='a' * 64)
    gate = dict(checkpoint_sha256=observer.SOURCE_SHA, instructions=observer.SOURCE_INSTRUCTIONS,
                source_boundary_verified=True, system_server_alive_verified=True,
                android_boot_completed_verified=False)
    kernel = dict(checkpoint_sha256=observer.SOURCE_SHA, instructions=observer.SOURCE_INSTRUCTIONS,
                  kernel_sha256=observer.KERNEL_SHA, input_unchanged=True,
                  live_kernel_security=dict(selinux_enforcing=True))
    proof = dict(events=copy.deepcopy(plan['inherited_source_events']), phase=8,
                 waiting=False, failed=False, complete=False, boot_completed=False,
                 hardware_commands_completed=True, C88_guard_succeeded=True,
                 C88_priority_postprobe_checked=True, live_peripherals_verified=True,
                 system_server_alive=True, SELinux_enforcing=True,
                 first_checkpoint_reproduces_validated_trial=True,
                 selected_native_unchanged_from_parent=True,
                 zero_input_continuation_campaign='c90r', current_campaign_stage=5,
                 actual_campaign_new_events=0, current_stage_new_UART_bytes=0,
                 last_admitted_sha256=observer.SOURCE_SHA, last_admitted_instructions=observer.SOURCE_INSTRUCTIONS,
                 native_binary_SHA256=observer.SOURCE_NATIVE, current_native_SHA256=observer.SOURCE_NATIVE,
                 next_native_binary_SHA256=observer.SOURCE_NATIVE,
                 selected_host_validation_report_SHA256=plan['host_validation']['report_SHA256'],
                 selected_host_extension_manifest_SHA256=plan['host_validation']['manifest_SHA256'],
                 combined_original_console_bytes_sha256=plan['source_console_SHA256'])
    preflight = dict(contract=contract, report=dict(actual_source_report='fixture'),
                     epoch=dict(actual_epoch='fixture'))
    policy = dict(controller_commit=observer.SOURCE_COMMIT, native_binary_sha256=observer.SOURCE_NATIVE,
                  instruction_budget=observer.BUDGET, host_wall_ms=plan['host_wall_ms'],
                  logical_RAM_clock_seed_cadence_IRQ_and_codecs_modified=False,
                  external_input_plan_present=False)
    return [contract, gate, kernel, proof, preflight, copy.deepcopy(preflight), policy]


def stage_records(plan):
    parent = source_records(plan)[0]
    commit = 'b' * 40
    contract = dict(checkpoint_sha256=observer.TAIL_SHA, instructions=observer.TAIL_INSTRUCTIONS,
                    source_commit=commit, epoch_sha256=observer.EPOCH_SHA,
                    epoch_original_init_verified=True, initialized_hardware_root=False)
    gate = dict(checkpoint_sha256=observer.TAIL_SHA, instructions=observer.TAIL_INSTRUCTIONS,
                source_boundary_verified=True, system_server_alive_verified=True,
                android_boot_completed_verified=False)
    kernel = dict(checkpoint_sha256=observer.TAIL_SHA, instructions=observer.TAIL_INSTRUCTIONS,
                  kernel_sha256=observer.KERNEL_SHA, input_unchanged=True,
                  live_kernel_security=dict(selinux_enforcing=True))
    current = dict(checkpoint_sha256=observer.TAIL_SHA, initial_instructions=observer.SOURCE_INSTRUCTIONS,
                   advanced_instructions=observer.BUDGET, stop_reason='instruction_budget', error='',
                   guest=dict(instructions=observer.TAIL_INSTRUCTIONS, logical_ram_bytes=2147483648,
                              counter_instructions_per_tick=64),
                   provenance=dict(input_checkpoint_sha256=parent['checkpoint_sha256'],
                                   input_report_sha256=parent['report_sha256'],
                                   host_snapshot_transport_bytes=observer.CAP,
                                   snapshot_round_trip_verified=True, source_cpu_modified_before_resume=False,
                                   live_android_properties_written=False, new_uart_input_bytes=0))
    host = dict(status='process_completed', exit_code=0, forced_kill=False, trigger=None,
                checkpoint_bytes_modified_by_supervisor=False)
    policy = dict(controller_commit=commit, native_binary_sha256=observer.SOURCE_NATIVE,
                  instruction_budget=observer.BUDGET, host_wall_ms=plan['host_wall_ms'],
                  logical_RAM_clock_seed_cadence_IRQ_and_codecs_modified=False,
                  external_input_plan_present=False)
    return [parent, contract, gate, kernel, current, host, policy, observer.TAIL_SHA, 2147483649, commit]


def source_pointers(plan):
    console = dict(SHA256=plan['source_console_SHA256'], bytes=plan['source_console_bytes'])
    profile = dict(manifest_SHA256=plan['expected_observer_manifest_SHA256'],
                   reader_SHA256=plan['expected_observer_reader_SHA256'])
    common = dict(cycle='C90R', current_source_proof_SHA256=observer.SOURCE_PROOF,
                  next_explicit_campaign='C91', next_plan=observer.PLAN, pending_C91_first_admission=True,
                  current_source_entire_console=console, next_host_observer_profile=profile)
    latest = dict(common, stage=5, checkpoint_sha256=observer.SOURCE_SHA,
                  instructions=observer.SOURCE_INSTRUCTIONS, source_artifact_lock=observer.SOURCE_LOCK,
                  execution_commit=observer.SOURCE_COMMIT, source_boundary_verified=True,
                  android_boot_completed_verified=False, native_binary_SHA256=observer.SOURCE_NATIVE,
                  total_campaign_ledger_events=8)
    continuation = dict(common, current_stage=5, current_source_checkpoint_sha256=observer.SOURCE_SHA,
                        current_source_instructions=observer.SOURCE_INSTRUCTIONS,
                        current_source_artifact_lock=observer.SOURCE_LOCK,
                        current_source_execution_commit=observer.SOURCE_COMMIT,
                        current_source_native_SHA256=observer.SOURCE_NATIVE,
                        current_input_phase=8, current_input_waiting=False)
    return [copy.deepcopy(latest), copy.deepcopy(continuation)]


class HostObserverAuthority(unittest.TestCase):
    def setUp(self):
        self.plan, self.base, self.reference, self.lock = fixtures()

    def test_valid_plan_and_exact_immutable_source_lock(self):
        observer.validate_plan(self.plan, self.base, self.reference, self.lock)
        actual = json.loads((ROOT / self.plan['source_artifact_lock']).read_text())
        manifest = json.loads((ROOT / self.lock['artifacts']['seed']['transport_manifest']).read_text())
        observer.validate_source_lock(self.plan, actual, self.lock, manifest)
        self.assertEqual((ROOT / self.plan['source_artifact_lock']).read_bytes(),
                         (ROOT / observer.SOURCE_LOCK).read_bytes())
        for key in ['seed', 'host-native', 'host-validation', 'native-cache', 'media-aa']:
            altered = copy.deepcopy(actual)
            altered['artifacts'][key] = dict(id=1, sha256='0' * 64)
            with self.subTest(alias=key), self.assertRaises(AssertionError):
                observer.validate_source_lock(self.plan, altered, self.lock, manifest)

    def test_reject_wrong_source_tail_as_source_or_replayed_ledger(self):
        changed_history = copy.deepcopy(self.plan['inherited_source_events'])
        changed_history[7]['actual_native_event']['bytes'] += 1
        for key, value in [('source_checkpoint_sha256', observer.TAIL_SHA), ('source_stage', 6),
                           ('source_instructions', observer.TAIL_INSTRUCTIONS),
                           ('source_proof_SHA256', '0' * 64), ('source_native_SHA256', '0' * 64),
                           ('expected_native_SHA256', self.reference['baseline_native_SHA256']),
                           ('base_host_profile_plan', observer.PLAN), ('source_console_SHA256', '0' * 64),
                           ('source_kernel_observation_SHA256', '0' * 64), ('source_peripherals_SHA256', '0' * 64),
                           ('inherited_source_events', changed_history), ('new_UART_bytes', 1),
                           ('input_file', 'input.bin'), ('input_plan_file', 'input.json'),
                           ('fresh_hardware_power_cycle', True), ('guest_snapshot_patching', True),
                           ('initial_instruction_budget', 300000000), ('max_stages', 17),
                           ('max_record_bytes', 65536), ('max_allowed_record_bound_bytes', 131072),
                           ('expected_observer_reader_SHA256', 'TBD'), ('expected_observer_manifest_SHA256', 'TBD')]:
            altered = copy.deepcopy(self.plan)
            altered[key] = value
            with self.subTest(key=key), self.assertRaises(AssertionError):
                observer.validate_plan(altered, self.base, self.reference, self.lock)
        altered = copy.deepcopy(self.plan)
        altered['first_checkpoint_reproduction']['failed_tail_used_as_source'] = True
        with self.assertRaises(AssertionError):
            observer.validate_plan(altered, self.base, self.reference, self.lock)

    def test_native_frame_reader_and_manifest_measurements_are_bound(self):
        expected = [observer.SOURCE_NATIVE, observer.SOURCE_FRAME,
                    self.plan['expected_observer_manifest_SHA256'], self.plan['expected_observer_reader_SHA256']]
        observer.measured_profile_matches(self.plan, *expected)
        for index in range(4):
            changed = list(expected)
            changed[index] = '0' * 64
            with self.subTest(index=index), self.assertRaises(AssertionError):
                observer.measured_profile_matches(self.plan, *changed)

    def test_entire_observer_profile_and_every_tool_byte_are_required(self):
        with tempfile.TemporaryDirectory() as directory:
            tools = Path(directory)
            original, replacement, unchanged = b'original finite reader\n', b'bounded local window reader\n', b'original epoch\n'
            old_sha = hashlib.sha256(original).hexdigest()
            new_sha = hashlib.sha256(replacement).hexdigest()
            other_sha = hashlib.sha256(unchanged).hexdigest()
            (tools / 'rvg_memory_lines.mjs').write_bytes(replacement)
            (tools / 'reverseandroid-epoch.mjs').write_bytes(unchanged)
            plan = copy.deepcopy(self.plan)
            plan['expected_observer_reader_SHA256'] = new_sha
            validated = dict(derived_tool_files={
                'rvg_memory_lines.mjs': dict(source_SHA256=old_sha, build_SHA256=old_sha),
                'reverseandroid-epoch.mjs': dict(source_SHA256='e' * 64, build_SHA256=other_sha)})
            files = copy.deepcopy(validated['derived_tool_files'])
            files['rvg_memory_lines.mjs']['build_SHA256'] = new_sha
            h = plan['host_validation']
            manifest = dict(schema=observer.PROFILE_SCHEMA, baseline_validation_run_id=h['run_id'],
                            baseline_validation_commit=h['execution_commit'], baseline_manifest_SHA256=h['manifest_SHA256'],
                            native_SHA256=observer.SOURCE_NATIVE, frame_SHA256=observer.SOURCE_FRAME,
                            host_snapshot_capacity_bytes=observer.CAP, original_870_files_unchanged=True,
                            original_rvg_memory_lines_SHA256=old_sha, replacement_rvg_memory_lines_SHA256=new_sha,
                            max_record_bytes=16384, max_allowed_record_bound_bytes=65536,
                            changed_tool_files=['rvg_memory_lines.mjs'], derived_tool_files=files)
            observer.validate_observer_manifest(plan, validated, manifest, tools)
            for key, value in [('baseline_validation_run_id', 1), ('native_SHA256', '0' * 64),
                               ('replacement_rvg_memory_lines_SHA256', old_sha), ('max_record_bytes', 16385),
                               ('original_870_files_unchanged', False), ('changed_tool_files', [])]:
                changed = copy.deepcopy(manifest)
                changed[key] = value
                with self.subTest(key=key), self.assertRaises(AssertionError):
                    observer.validate_observer_manifest(plan, validated, changed, tools)
            for filename in ['rvg_memory_lines.mjs', 'reverseandroid-epoch.mjs']:
                path = tools / filename
                before = path.read_bytes()
                path.write_bytes(before + b'unvalidated tool mutation\n')
                with self.subTest(tool=filename), self.assertRaises(AssertionError):
                    observer.validate_observer_manifest(plan, validated, manifest, tools)
                path.write_bytes(before)
            (tools / 'unexpected-extra.mjs').write_bytes(b'not in manifest')
            with self.assertRaises(AssertionError):
                observer.validate_observer_manifest(plan, validated, manifest, tools)

    def test_source_bytes_proof_and_producer_must_match(self):
        expected = [observer.SOURCE_SHA, observer.SOURCE_PROOF, observer.SOURCE_COMMIT]
        observer.validate_source_bytes(self.plan, *expected)
        for index in range(3):
            altered = list(expected)
            altered[index] = '0' * len(altered[index])
            with self.subTest(index=index), self.assertRaises(AssertionError):
                observer.validate_source_bytes(self.plan, *altered)

    def test_source_preflight_and_all_inherited_authority_are_required(self):
        records = source_records(self.plan)
        observer.validate_original_source(self.plan, *records)
        for index, key, value in [(0, 'instructions', observer.TAIL_INSTRUCTIONS),
                                  (1, 'source_boundary_verified', False), (1, 'system_server_alive_verified', False),
                                  (2, 'input_unchanged', False), (2, 'checkpoint_sha256', observer.TAIL_SHA),
                                  (3, 'events', []), (3, 'phase', 7), (3, 'waiting', True), (3, 'failed', True),
                                  (3, 'native_binary_SHA256', '0' * 64), (3, 'next_native_binary_SHA256', '0' * 64),
                                  (3, 'combined_original_console_bytes_sha256', '0' * 64),
                                  (3, 'selected_host_extension_manifest_SHA256', self.plan['expected_observer_manifest_SHA256']),
                                  (5, 'report', dict(actual_source_report='different')),
                                  (6, 'native_binary_sha256', '0' * 64), (6, 'external_input_plan_present', True)]:
            changed = copy.deepcopy(records)
            changed[index][key] = value
            with self.subTest(index=index, key=key), self.assertRaises(AssertionError):
                observer.validate_original_source(self.plan, *changed)

    def test_new_source_observations_require_whole_original_bytes(self):
        kernel = b'{"checkpoint":"fixture","processes":[1,2],"input_unchanged":true}\n'
        probe = b'{"channels":[1,2,3],"errors":[],"input_unchanged":true}\n'
        plan = copy.deepcopy(self.plan)
        plan['source_kernel_observation_SHA256'] = hashlib.sha256(kernel).hexdigest()
        plan['source_peripherals_SHA256'] = hashlib.sha256(probe).hexdigest()
        observer.validate_source_observations(plan, kernel, kernel, probe, probe)
        for index, changed in [(0, kernel.replace(b'[1,2]', b'[1,3]')),
                                (1, kernel.replace(b'[1,2]', b'[1,3]')),
                                (2, probe.replace(b'[1,2,3]', b'[1,2,4]')),
                                (3, probe.replace(b'"errors":[]', b'"errors":["read failed"]')),
                                (1, json.dumps(json.loads(kernel), sort_keys=True).encode() + b'\n')]:
            values = [kernel, kernel, probe, probe]
            values[index] = changed
            with self.subTest(index=index, changed=changed), self.assertRaises(AssertionError):
                observer.validate_source_observations(plan, *values)

    def test_lease_rejects_either_advanced_pointer_or_different_profile(self):
        pointers = source_pointers(self.plan)
        observer.validate_source_lease(self.plan, *pointers)
        for index, key, value in [(0, 'checkpoint_sha256', observer.TAIL_SHA), (0, 'stage', 6),
                                  (0, 'native_binary_SHA256', '0' * 64),
                                  (1, 'current_source_checkpoint_sha256', observer.TAIL_SHA),
                                  (1, 'current_source_proof_SHA256', '0' * 64),
                                  (1, 'current_source_instructions', observer.TAIL_INSTRUCTIONS),
                                  (1, 'next_plan', 'state/experiments/c90r-plan.json'),
                                  (1, 'pending_C91_first_admission', False)]:
            changed = copy.deepcopy(pointers)
            changed[index][key] = value
            with self.subTest(index=index, key=key), self.assertRaises(AssertionError):
                observer.validate_source_lease(self.plan, *changed)
        changed = copy.deepcopy(pointers)
        changed[0]['next_host_observer_profile']['reader_SHA256'] = '0' * 64
        with self.assertRaises(AssertionError):
            observer.validate_source_lease(self.plan, *changed)

    def test_stage_requires_real_boundary_roundtrip_native_and_zero_input(self):
        records = stage_records(self.plan)
        observer.validate_stage_result(self.plan, 1, *records)
        for index, key, value in [(1, 'source_commit', 'c' * 40), (1, 'epoch_original_init_verified', False),
                                  (2, 'source_boundary_verified', False), (3, 'input_unchanged', False),
                                  (3, 'live_kernel_security', dict(selinux_enforcing=False)),
                                  (4, 'error', 'native error'), (5, 'exit_code', 1), (5, 'forced_kill', True),
                                  (6, 'native_binary_sha256', '0' * 64), (6, 'external_input_plan_present', True)]:
            changed = copy.deepcopy(records)
            changed[index][key] = value
            with self.subTest(index=index, key=key), self.assertRaises(AssertionError):
                observer.validate_stage_result(self.plan, 1, *changed)
        for key, value in [('input_checkpoint_sha256', observer.TAIL_SHA), ('input_report_sha256', '0' * 64),
                           ('snapshot_round_trip_verified', False), ('source_cpu_modified_before_resume', True),
                           ('live_android_properties_written', True), ('new_uart_input_bytes', 1),
                           ('external_uart_input', {})]:
            changed = copy.deepcopy(records)
            changed[4]['provenance'][key] = value
            with self.subTest(provenance=key), self.assertRaises(AssertionError):
                observer.validate_stage_result(self.plan, 1, *changed)

    def test_first_replay_needs_exact_tail_hash_and_instructions(self):
        observer.validate_first_checkpoint(self.plan, observer.TAIL_SHA, observer.TAIL_INSTRUCTIONS)
        for sha, instructions in [(observer.SOURCE_SHA, observer.TAIL_INSTRUCTIONS),
                                   (observer.TAIL_SHA, observer.TAIL_INSTRUCTIONS - 1)]:
            with self.subTest(sha=sha, instructions=instructions), self.assertRaises(AssertionError):
                observer.validate_first_checkpoint(self.plan, sha, instructions)

    def test_correction_needs_fresh_peripherals_and_boot_is_never_assumed(self):
        observer.require_first_peripheral_validation(1, 0, True)
        for exit_code, live in [(1, True), (0, False), (1, False)]:
            with self.subTest(exit_code=exit_code, live=live), self.assertRaises(AssertionError):
                observer.require_first_peripheral_validation(1, exit_code, live)
        observer.require_first_peripheral_validation(2, 1, False)
        gate = stage_records(self.plan)[2]
        self.assertFalse(boot_complete(True, True, gate))
        gate['android_boot_completed_verified'] = True
        self.assertFalse(boot_complete(True, False, gate))
        gate['system_server_alive_verified'] = False
        self.assertFalse(boot_complete(True, True, gate))
        gate['system_server_alive_verified'] = True
        self.assertTrue(boot_complete(True, True, gate))

    def test_active_control_history_native_and_profile_cannot_drift(self):
        state = source_records(self.plan)[3]
        state.update(zero_input_continuation_campaign='c91', current_campaign_stage=0,
                     original_parent_proof_SHA256=observer.SOURCE_PROOF,
                     original_source_checkpoint_SHA256=observer.SOURCE_SHA,
                     failed_tail_used_as_source=False, original_source_observations_reproduced_byte_for_byte=True,
                     selected_host_transport_manifest_SHA256=self.plan['host_validation']['manifest_SHA256'],
                     selected_host_extension_manifest_SHA256=self.plan['expected_observer_manifest_SHA256'],
                     selected_host_observer_manifest_SHA256=self.plan['expected_observer_manifest_SHA256'],
                     selected_host_observer_reader_SHA256=self.plan['expected_observer_reader_SHA256'],
                     host_observer_profile_schema=observer.PROFILE_SCHEMA, host_snapshot_capacity_bytes=observer.CAP)
        observer.validate_control_state(self.plan, state)
        events = copy.deepcopy(state['events'])
        events[-1]['actual_native_event']['sha256'] = '0' * 64
        for key, value in [('events', events), ('phase', 9), ('actual_campaign_new_events', 1),
                           ('next_native_binary_SHA256', '0' * 64), ('current_native_SHA256', '0' * 64),
                           ('selected_host_observer_reader_SHA256', '0' * 64),
                           ('selected_host_extension_manifest_SHA256', self.plan['host_validation']['manifest_SHA256']),
                           ('failed_tail_used_as_source', True)]:
            changed = copy.deepcopy(state)
            changed[key] = value
            with self.subTest(key=key), self.assertRaises(AssertionError):
                observer.validate_control_state(self.plan, changed)


if __name__ == '__main__':
    unittest.main()
