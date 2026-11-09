"""Authority regressions; synthetic records never claim native Android execution."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from host_transport_continuation import (CAP, OLD_NATIVE, SOURCE_SHA, SOURCE_PROOF,
    SOURCE_COMMIT, SOURCE_INSTRUCTIONS, SOURCE_RUN, SOURCE_LOCK, validate_plan,
    validate_report, validate_preserved_transport, measured_profile_matches,
    validate_original_source, validate_first_checkpoint, validate_generated_profile)


def fixtures():
    reference = json.loads((ROOT / 'state/experiments/host-transport-plan.json').read_text())
    plan = json.loads((ROOT / 'state/experiments/c90-plan.json').read_text())
    plan.update(schema='reverseandroid-validated-host-zero-input-continuation-plan-v1', cycle='c90r',
                source_cycle='C90', source_stage=6, source_checkpoint_sha256=SOURCE_SHA,
                source_instructions=SOURCE_INSTRUCTIONS, source_execution_commit=SOURCE_COMMIT,
                source_run_id=SOURCE_RUN, source_native_SHA256=OLD_NATIVE,
                source_proof_file='zero-input-proof.json', source_proof_SHA256=SOURCE_PROOF,
                source_original_artifact_lock=SOURCE_LOCK,
                source_artifact_lock='state/experiments/c90r-artifacts.json',
                expected_native_SHA256='1' * 64, expected_frame_SHA256='2' * 64,
                host_snapshot_capacity_bytes=CAP, initial_instruction_budget=300000000)
    for key in ['inherited_source_events', 'source_console_file', 'source_console_base64_SHA256',
                'source_console_gzip_SHA256', 'source_console_bytes', 'source_console_SHA256',
                'ELF_SHA256', 'guest_ELF_path']:
        plan[key] = copy.deepcopy(reference[key])
    plan['host_validation'] = dict(run_id=900, execution_commit='a' * 40,
                                  report_SHA256='4' * 64, manifest_SHA256='3' * 64,
                                  large_checkpoint_sha256='5' * 64,
                                  large_transport_manifest='state/experiments/host-transport-validated-large-transport.json',
                                  large_transport_alias='validated-large')
    control = dict(schema='reverseandroid-host-transport-byte-equality-v1',
                   control_source=copy.deepcopy(reference['control_source']), instructions=50000000,
                   baseline_native_SHA256=OLD_NATIVE, candidate_native_SHA256=plan['expected_native_SHA256'],
                   actual_cmp_every_RVG_byte_equal=True, entire_console_bytes_equal=True,
                   original_C46_gate_equal=True, entire_original_kernel_observation_equal=True,
                   host_transport_policy_not_serialized=True, new_UART_bytes=0, control_is_not_primary_authority=True)
    large = dict(schema='reverseandroid-host-transport-large-original-admission-v1',
                 source=copy.deepcopy(reference['canonical_source']), host_snapshot_capacity_bytes=CAP,
                 native_SHA256=plan['expected_native_SHA256'], frame_native_SHA256=plan['expected_frame_SHA256'],
                 checkpoint_SHA256='5' * 64, instructions=SOURCE_INSTRUCTIONS + 300000000,
                 checkpoint_bytes=2147483649, actual_checkpoint_exceeds_previous_2GiB_host_bound=True,
                 full_original_native_roundtrip_and_original_C46_boundary_verified=True,
                 fresh_live_peripherals_verified=True, SELinux_enforcing=True,
                 inherited_source_events=copy.deepcopy(plan['inherited_source_events']), new_UART_bytes=0,
                 original_BOOT=False, usable_UI_or_response_claimed=False, experimental_validation_is_not_yet_primary=True)
    report = dict(schema='reverseandroid-validated-host-only-transport-extension-v1',
                  validated=True, candidate_is_not_yet_primary=True, CI_run_id=900, execution_commit='a' * 40,
                  baseline_native_SHA256=OLD_NATIVE, candidate_native_SHA256=plan['expected_native_SHA256'],
                  candidate_frame_SHA256=plan['expected_frame_SHA256'], host_extension_manifest_SHA256='3' * 64,
                  host_snapshot_capacity_bytes=CAP, new_UART_bytes=0, original_870_source_files_unchanged=True,
                  guest_RAM_disk_seed_clock_cadence_IRQ_and_snapshot_codecs_unchanged=True,
                  original_BOOT_and_UI_gates_unchanged=True, full_original_control=control,
                  full_larger_original_admission=large)
    return plan, reference, report


class HostTransportAuthority(unittest.TestCase):
    def setUp(self):
        self.plan, self.reference, self.report = fixtures()

    def test_valid_explicit_plan_and_actual_report_contract(self):
        validate_plan(self.plan, self.reference)
        validate_report(self.plan, self.reference, self.report)

    def test_reject_different_checkpoint_replayed_history_or_implicit_policy(self):
        changed_history = copy.deepcopy(self.plan['inherited_source_events'])
        changed_history[-1]['actual_native_event']['bytes'] += 1
        for key, value in [('source_checkpoint_sha256', '0' * 64), ('source_stage', 7),
                           ('source_instructions', SOURCE_INSTRUCTIONS + 1),
                           ('source_proof_SHA256', '0' * 64), ('source_native_SHA256', '0' * 64),
                           ('inherited_source_events', changed_history), ('new_UART_bytes', 1),
                           ('input_file', 'replay.bin'), ('input_plan_file', 'input.json'),
                           ('fresh_hardware_power_cycle', True), ('guest_snapshot_patching', True),
                           ('expected_native_SHA256', OLD_NATIVE), ('host_snapshot_capacity_bytes', 2**32),
                           ('initial_instruction_budget', 1000000000),
                           ('continuation_instruction_budget', 2_000_000_000), ('max_stages', 17)]:
            p = copy.deepcopy(self.plan)
            p[key] = value
            with self.subTest(key=key), self.assertRaises(AssertionError):
                validate_plan(p, self.reference)

    def test_reject_wrong_validation_producer_native_or_unvalidated_report(self):
        for key, value in [('validated', False), ('CI_run_id', 901), ('execution_commit', 'b' * 40),
                           ('baseline_native_SHA256', '0' * 64), ('candidate_native_SHA256', '0' * 64),
                           ('candidate_frame_SHA256', '0' * 64), ('host_extension_manifest_SHA256', '0' * 64),
                           ('original_BOOT_and_UI_gates_unchanged', False), ('new_UART_bytes', 1)]:
            report = copy.deepcopy(self.report)
            report[key] = value
            with self.subTest(key=key), self.assertRaises(AssertionError):
                validate_report(self.plan, self.reference, report)

    def test_reject_missing_original_equality_or_no_real_larger_checkpoint(self):
        for key in ['actual_cmp_every_RVG_byte_equal', 'entire_console_bytes_equal',
                    'original_C46_gate_equal', 'entire_original_kernel_observation_equal']:
            report = copy.deepcopy(self.report)
            report['full_original_control'][key] = False
            with self.subTest(key=key), self.assertRaises(AssertionError):
                validate_report(self.plan, self.reference, report)
        for key, value in [('checkpoint_bytes', 2147483648), ('checkpoint_bytes', CAP + 1),
                           ('fresh_live_peripherals_verified', False), ('SELinux_enforcing', False),
                           ('full_original_native_roundtrip_and_original_C46_boundary_verified', False),
                           ('source', self.reference['control_source']), ('inherited_source_events', []),
                           ('instructions', SOURCE_INSTRUCTIONS), ('new_UART_bytes', 1)]:
            report = copy.deepcopy(self.report)
            report['full_larger_original_admission'][key] = value
            with self.subTest(key=key, value=str(value)[:60]), self.assertRaises(AssertionError):
                validate_report(self.plan, self.reference, report)

    def test_measured_binary_frame_manifest_and_producer_must_match(self):
        expected = [self.plan['expected_native_SHA256'], self.plan['expected_frame_SHA256'],
                    self.plan['host_validation']['manifest_SHA256'], self.plan['host_validation']['execution_commit']]
        measured_profile_matches(self.plan, *expected)
        for index in range(4):
            measured = list(expected)
            measured[index] = '0' * len(measured[index])
            with self.subTest(index=index), self.assertRaises(AssertionError):
                measured_profile_matches(self.plan, *measured)

    def test_first_checkpoint_must_reproduce_complete_validated_trial(self):
        large = self.report['full_larger_original_admission']
        expected = [large['checkpoint_SHA256'], large['instructions'], large['checkpoint_bytes']]
        validate_first_checkpoint(self.report, *expected)
        for index, value in [(0, '0' * 64), (1, large['instructions'] + 1), (2, large['checkpoint_bytes'] + 1)]:
            changed = list(expected)
            changed[index] = value
            with self.subTest(index=index), self.assertRaises(AssertionError):
                validate_first_checkpoint(self.report, *changed)

    def test_generated_manifest_allows_only_key_order_and_preserves_actual_tool_hashes(self):
        with tempfile.TemporaryDirectory() as folder:
            tools = Path(folder)
            first, second = b'export const one = 1;\n', b'export const two = 2;\n'
            (tools / 'one.mjs').write_bytes(first)
            (tools / 'two.mjs').write_bytes(second)
            one = dict(source_SHA256='a' * 64, build_SHA256=hashlib.sha256(first).hexdigest())
            two = dict(source_SHA256='b' * 64, build_SHA256=hashlib.sha256(second).hexdigest())
            validated = dict(host_RVG_file_capacity_bytes=CAP,
                             derived_tool_files={'one.mjs': one, 'two.mjs': two})
            reordered = dict(derived_tool_files={'two.mjs': dict(reversed(list(two.items()))),
                                                'one.mjs': dict(reversed(list(one.items())))},
                             host_RVG_file_capacity_bytes=CAP)
            self.assertNotEqual(json.dumps(validated), json.dumps(reordered))
            validate_generated_profile(validated, json.loads(json.dumps(reordered)), tools)
            changed = copy.deepcopy(reordered)
            changed['host_RVG_file_capacity_bytes'] += 1
            with self.assertRaises(AssertionError):
                validate_generated_profile(validated, changed, tools)
            changed = copy.deepcopy(reordered)
            changed['derived_tool_files']['one.mjs']['build_SHA256'] = '0' * 64
            with self.assertRaises(AssertionError):
                validate_generated_profile(validated, changed, tools)
            (tools / 'one.mjs').write_bytes(first + b'// changed executable bytes\n')
            with self.assertRaises(AssertionError):
                validate_generated_profile(validated, reordered, tools)

    def test_preserved_large_manifest_and_all_piece_pins_are_required(self):
        large = self.report['full_larger_original_admission']
        manifest = dict(schema='reverseandroid-checkpoint-transport-v1', compression='tar+gzip',
                        source_contract=dict(checkpoint_sha256=large['checkpoint_SHA256'],
                                             instructions=large['instructions'], source_commit='a' * 40,
                                             epoch_original_init_verified=True),
                        progress=dict(checkpoint_sha256=large['checkpoint_SHA256'],
                                      instructions=large['instructions'], source_boundary_verified=True,
                                      android_boot_completed_verified=False),
                        parts=[dict(file='checkpoint.part-000', bytes=123, sha256='6' * 64)])
        pin = dict(producer_run_id=900, producer_execution_commit='a' * 40,
                   transport_manifest=self.plan['host_validation']['large_transport_manifest'],
                   pieces=[dict(id=123, repository='guadalupejuan001-code/reverseAndroid', sha256='7' * 64)])
        validate_preserved_transport(self.plan, self.report, manifest, pin)
        for key, value in [('producer_run_id', 901), ('producer_execution_commit', 'b' * 40), ('pieces', [])]:
            altered = copy.deepcopy(pin)
            altered[key] = value
            with self.subTest(key=key), self.assertRaises(AssertionError):
                validate_preserved_transport(self.plan, self.report, manifest, altered)
        altered = copy.deepcopy(manifest)
        altered['source_contract']['checkpoint_sha256'] = SOURCE_SHA
        with self.assertRaises(AssertionError):
            validate_preserved_transport(self.plan, self.report, altered, pin)

    def test_original_source_requires_exact_proof_history_and_both_preflights(self):
        contract = dict(checkpoint_sha256=SOURCE_SHA, instructions=SOURCE_INSTRUCTIONS, source_commit=SOURCE_COMMIT,
                        epoch_sha256='bc5e6f91cc5befb13ba8f8e96ffb74cad22edda25586324a0f469e1b26fd5e75')
        gate = dict(checkpoint_sha256=SOURCE_SHA, instructions=SOURCE_INSTRUCTIONS, source_boundary_verified=True)
        kernel = dict(kernel_sha256='f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8',
                      live_kernel_security=dict(selinux_enforcing=True), input_unchanged=True)
        proof = dict(events=copy.deepcopy(self.plan['inherited_source_events']), phase=8, waiting=False,
                     failed=False, hardware_commands_completed=True, C88_guard_succeeded=True,
                     C88_priority_postprobe_checked=True, native_binary_SHA256=OLD_NATIVE,
                     combined_original_console_bytes_sha256=self.plan['source_console_SHA256'])
        preflight = dict(contract=contract)
        policy = dict(native_binary_sha256=OLD_NATIVE)
        args = [contract, gate, kernel, proof, preflight, copy.deepcopy(preflight), policy]
        validate_original_source(self.plan, *args)
        for index, key, value in [(0, 'instructions', SOURCE_INSTRUCTIONS + 1),
                                  (1, 'source_boundary_verified', False), (2, 'input_unchanged', False),
                                  (3, 'events', []), (3, 'waiting', True), (3, 'failed', True),
                                  (3, 'native_binary_SHA256', self.plan['expected_native_SHA256']),
                                  (3, 'combined_original_console_bytes_sha256', '0' * 64),
                                  (5, 'contract', dict(contract, checkpoint_sha256='0' * 64)),
                                  (6, 'native_binary_sha256', self.plan['expected_native_SHA256'])]:
            altered = copy.deepcopy(args)
            altered[index][key] = value
            with self.subTest(index=index, key=key), self.assertRaises(AssertionError):
                validate_original_source(self.plan, *altered)


if __name__ == '__main__':
    unittest.main()
