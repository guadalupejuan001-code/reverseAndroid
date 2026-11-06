#!/usr/bin/env python3
"""Prepare one real Android UI gesture only after original boot and frame checks.

This does not execute a guest. The original epoch runner must independently
admit the same source and verify its live boot gate before applying this plan.
"""
import argparse
import hashlib
import json
import pathlib
import shlex


def read(path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    parser.add_argument('x0', type=int)
    parser.add_argument('y0', type=int)
    parser.add_argument('x1', type=int)
    parser.add_argument('y1', type=int)
    parser.add_argument('--duration-ms', type=int, default=300)
    parser.add_argument('--reviewed-usable-ui', action='store_true')
    args = parser.parse_args()
    gate = read(args.source / 'c46-progress.json')
    contract = read(args.source / 'output-contract.json')
    frame = read(args.source / 'checkpoint-frame.json')
    assert gate['source_boundary_verified'] and gate['system_server_alive_verified']
    assert gate['android_boot_completed_verified'], 'Original completed-boot evidence is required'
    assert frame['status'] == 'scanout_exported' and frame['original_snapshot_unchanged']
    assert contract['checkpoint_sha256'] == gate['checkpoint_sha256'] == frame['source_checkpoint_sha256']
    assert contract['instructions'] == gate['instructions'] == frame['instructions']
    ppm = (args.source / 'checkpoint-frame.ppm').read_bytes()
    assert hashlib.sha256(ppm).hexdigest() == frame['ppm_sha256']
    assert args.reviewed_usable_ui, 'Inspect the authentic before frame and confirm its usable UI before preparing a gesture'
    assert 0 <= args.x0 < frame['width'] and 0 <= args.x1 < frame['width']
    assert 0 <= args.y0 < frame['height'] and 0 <= args.y1 < frame['height']
    assert (args.x0, args.y0) != (args.x1, args.y1)
    assert 100 <= args.duration_ms <= 1000
    body = f'input touchscreen swipe {args.x0} {args.y0} {args.x1} {args.y1} {args.duration_ms}'
    command = ('su 0 sh -c ' + shlex.quote(body) +
               "; r=$?; printf '\\nREVERSE_%s RC_%s\\n' UI_RESPONSE \"$r\"\n")
    raw = command.encode('ascii')
    assert 0 < len(raw) <= 4096
    plan = {'schema': 'reverseandroid-authenticated-console-input-plan-v1',
            'purpose': 'ui_response_test', 'input_file': 'input.txt',
            'input_bytes': len(raw), 'input_sha256': hashlib.sha256(raw).hexdigest(),
            'source_checkpoint_sha256': contract['checkpoint_sha256'],
            'source_instructions': contract['instructions'],
            'before_frame_ppm_sha256': frame['ppm_sha256'],
            'before_usable_UI_manually_reviewed': True,
            'effect': f'Inject one real Android touchscreen swipe from ({args.x0},{args.y0}) to ({args.x1},{args.y1}) in {args.duration_ms}ms.',
            'requires_actual_after_frame_review': True,
            'actual_UI_input_or_response_claimed': False}
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / 'input.txt').write_bytes(raw)
    (args.output / 'input-plan.json').write_text(json.dumps(plan, indent=2) + '\n')
    print(json.dumps({'source_checkpoint_SHA256': contract['checkpoint_sha256'],
                      'input_bytes': len(raw), 'source_bound_before_frame': True,
                      'guest_execution_started': False, 'actual_UI_response_verified': False}))


if __name__ == '__main__':
    main()
