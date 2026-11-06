#!/usr/bin/env python3
"""Read-only comparison of the real before/after Android CLZ snapshots."""
import hashlib
import json
import pathlib
import struct
import sys


def admitted(directory):
    directory = pathlib.Path(directory)
    read = lambda name: json.loads((directory / name).read_text())
    contract, gate, host, report = map(read, ['output-contract.json', 'c46-progress.json', 'process-outcome.json', 'current.json'])
    assert gate['source_boundary_verified'] is True
    assert gate['checkpoint_sha256'] == report['checkpoint_sha256'] == contract['checkpoint_sha256']
    assert gate['instructions'] == report['guest']['instructions'] == contract['instructions']
    assert host['status'] == 'process_completed' and host['exit_code'] == 0 and not host['forced_kill']
    path = directory / 'current.rvg'
    with path.open('rb') as f:
        assert hashlib.file_digest(f, 'sha256').hexdigest() == contract['checkpoint_sha256']
    with path.open('rb') as f:
        assert f.read(8) == b'RVG3\r\n\x1a\n'
        fields = struct.unpack('<44Q', f.read(44 * 8))
        machine_at = 360 + sum(fields[39:42])
        f.seek(machine_at)
        machine = f.read(fields[42])
        fp = f.read(fields[43])
    assert len(machine) == fields[42] and len(fp) == fields[43]
    assert fp[:8] == b'RFP1\r\n\x1a\n'
    cr, sr, mask = struct.unpack_from('<III', fp, 8)
    vectors, at = [[0, 0] for _ in range(32)], 20
    for r in range(32):
        if mask & (1 << r):
            vectors[r] = list(struct.unpack_from('<QQ', fp, at))
            at += 16
    assert at == len(fp)
    return path, contract, report, fields, machine, cr, sr, vectors


def verify(before_dir, after_dir):
    a, b = admitted(before_dir), admitted(after_dir)
    assert a[1]['instructions'] == 85047727997 and b[1]['instructions'] == 85047727998
    assert a[1]['checkpoint_sha256'] == '9b67411e2faefdd2c3e200d4c41157c2513f738fde934cf9d9947651bc47d866'
    assert a[2]['guest']['pc'] == '0x7283e70a78' and b[2]['guest']['pc'] == '0x7283e70a7c'
    assert b[2]['provenance']['input_checkpoint_sha256'] == a[1]['checkpoint_sha256']
    assert b[2]['advanced_instructions'] == 1 and b[2]['provenance']['new_uart_input_bytes'] == 0
    assert a[3][43] <= 532 and b[3][43] <= 532
    for n in range(44):
        if n not in (2, 4, 43):
            assert a[3][n] == b[3][n], ('Unexpected RVG header change', n)
    # Exact same immutable image, quantum core and effective RAM/disk sections.
    extent = sum(a[3][39:42])
    with a[0].open('rb') as fa, b[0].open('rb') as fb:
        fa.seek(360)
        fb.seek(360)
        while extent:
            size = min(extent, 1024 * 1024)
            assert fa.read(size) == fb.read(size), 'CLZ changed image, XI/Ether or effective memory'
            extent -= size
    # The original clock advances one subtick (25 -> 26), with no timer tick.
    assert a[4][:-8] == b[4][:-8]
    old_subtick, new_subtick = (struct.unpack('<Q', x[4][-8:])[0] for x in (a, b))
    assert old_subtick == 25 and new_subtick == 26
    assert a[5:7] == b[5:7], 'CLZ changed FPCR or FPSR'
    source_lanes = [(word >> shift) & 0xffffffff for word in a[7][0] for shift in (0, 32)]
    expected = [32 - x.bit_length() for x in source_lanes]
    actual_lanes = [(word >> shift) & 0xffffffff for word in b[7][0] for shift in (0, 32)]
    assert actual_lanes == expected
    assert a[7][1:] == b[7][1:], 'CLZ changed another SIMD register'
    return {'schema': 'reverseandroid-actual-CLZ-retirement-v1',
            'before_SHA256': a[1]['checkpoint_sha256'], 'after_SHA256': b[1]['checkpoint_sha256'],
            'before_instructions': a[1]['instructions'], 'after_instructions': b[1]['instructions'],
            'original_admission_and_clean_outcomes_verified': True,
            'actual_input_v0_32bit_lanes': source_lanes, 'actual_output_v0_32bit_lanes': actual_lanes,
            'CLZ_matches_independent_integer_bit_length': True,
            'integer_registers_flags_other_SIMD_and_FPCR_FPSR_unchanged': True,
            'immutable_image_XI_Ether_effective_memory_byte_identical': True,
            'all_machine_bytes_unchanged_except_original_subtick_25_to_26': True,
            'snapshot_bytes_written_by_verifier': False, 'new_UART_input_bytes': 0,
            'Android_boot_or_usable_UI_claimed': False}


if __name__ == '__main__':
    assert len(sys.argv) == 4
    proof = verify(sys.argv[1], sys.argv[2])
    with pathlib.Path(sys.argv[3]).open('x') as output:
        json.dump(proof, output, indent=2)
        output.write('\n')
    print(json.dumps(proof))
