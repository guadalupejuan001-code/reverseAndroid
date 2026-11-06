#!/usr/bin/env python3
"""Prepare a full authenticated ABI3 transfer for a future fresh guest source.

This writes reviewable inputs and tests the real blocking capture on a host PTY.
It never runs the emulator. Actual private-PTY HAL integration must pass before
adoption is bound to the newest fully admitted canonical source.
"""
import argparse
import base64
import gzip
import hashlib
import json
import os
import pathlib
import pty
import select
import shlex
import subprocess
import tempfile
import time
import tty


def digest(data):
    return hashlib.sha256(data).hexdigest()


def marker(name):
    return "printf '\\nREVERSE_%s RC_0\\n' " + name


def root(body, name):
    return ('su 0 sh -c ' + shlex.quote(body) +
            "; r=$?; printf '\\nREVERSE_%s RC_%s\\n' " + name + ' "$r"\n')


def exercise_capture(capture, chunks, data_file, encoded, corrupt=False):
    with tempfile.TemporaryDirectory(prefix='reverseandroid-buffered-pty-') as temp:
        target = pathlib.Path(temp) / 'peer.b64'
        body = capture.replace(data_file, str(target))
        master, slave = pty.openpty()
        tty.setraw(slave)
        process = subprocess.Popen(['bash', '-c', body], stdin=slave, stdout=slave, stderr=slave)
        os.close(slave)
        observed = bytearray()

        def receive_until(expected):
            deadline = time.monotonic() + 10
            while expected not in observed:
                assert time.monotonic() < deadline, 'capture did not reach its actual response'
                ready, _, _ = select.select([master], [], [], 0.1)
                if ready:
                    data = os.read(master, 16384)
                    assert data
                    observed.extend(data)

        try:
            receive_until(b'REVERSE_PTS_TRANSFER_RX_0 RC_0\n')
            for i, chunk in enumerate(chunks):
                if corrupt and i == 0:
                    chunk = ('B' if chunk[0] != 'B' else 'A') + chunk[1:]
                data = (chunk + '\n').encode()
                while data:
                    data = data[os.write(master, data):]
                if corrupt:
                    assert process.wait(timeout=10) != 0
                    assert b'REVERSE_PTS_TRANSFER_RX_1 RC_0' not in observed
                    return
                receive_until(f'REVERSE_PTS_TRANSFER_DATA_{i} RC_0\n'.encode())
                if i == 0:
                    receive_until(b'REVERSE_PTS_TRANSFER_RX_1 RC_0\n')
            assert process.wait(timeout=10) == 0
            assert target.read_bytes() == encoded.encode()
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            os.close(master)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    args = parser.parse_args()
    repo = pathlib.Path(__file__).resolve().parent.parent
    manifest = json.loads((args.build / 'build-manifest.json').read_text())
    executable = (args.build / 'cuttlefish-peripheral-peer-aarch64').read_bytes()
    compressed = (args.build / 'peer-aarch64.gz').read_bytes()
    assert (args.build / 'execution-commit.txt').read_text().strip() == 'c657a2395d8ebc9b2df2d3e0d7d9e5e28556ce32'
    assert digest(executable) == manifest['ELF_SHA256'] == '548714c27bfb7efe8b6300b99b2dcd77054b30d58b31c3c44a529e056a0bf42b'
    assert digest(compressed) == manifest['gzip_SHA256'] == '671fd2b742811b13eb861fb848f5a033989de991561c064caf09e310ee8e0522'
    assert gzip.decompress(compressed) == executable
    assert json.loads((args.build / 'arm64-abi-test.json').read_text()) == {'official_AArch64_UAPI_flags_match': True, 'O_DIRECTORY': 16384, 'O_NOFOLLOW': 32768}
    encoded = base64.b64encode(compressed).decode('ascii')
    chunks = [encoded[i:i+3000] for i in range(0, len(encoded), 3000)]
    assert len(chunks) == 2
    base = '/data/local/tmp/reverseandroid-peer-abi3'
    data_file, elf_file = base + '.b64', base + '.elf'
    capture = 'set -e; umask 077; test ! -e ' + data_file + '; test ! -L ' + data_file + '; '
    for i in range(2):
        capture += marker('PTS_TRANSFER_RX_' + str(i)) + '; IFS= read -r data || exit 21; '
        capture += 'printf %s "$data" ' + ('> ' if i == 0 else '>> ') + data_file + '; '
        capture += 'h=$(sha256sum ' + data_file + '); test "${h%% *}" = ' + digest(''.join(chunks[:i+1]).encode()) + '; '
        capture += marker('PTS_TRANSFER_DATA_' + str(i)) + '; '
    capture += 'true'
    decode = ('set -e; umask 077; h=$(sha256sum ' + data_file + '); test "${h%% *}" = ' + digest(encoded.encode()) + '; '
              'test ! -e ' + elf_file + '; test ! -L ' + elf_file + '; base64 -d ' + data_file + ' | gzip -dc > ' + elf_file + '; '
              'h=$(sha256sum ' + elf_file + '); test "${h%% *}" = ' + manifest['ELF_SHA256'] + '; ')
    # Reuse the explicit EOF-safe candidate verbatim. This is a proposal, not a
    # statement that its still-independent Android integration already passed.
    proposal = json.loads((repo / 'state/experiments/peripheral-privatepts-eof-control-plan.json').read_text())
    bodies = []
    for phase in proposal['phases']:
        raw = (repo / phase['input_file']).read_bytes()
        assert digest(raw) == phase['input_sha256']
        tokens = shlex.split(raw.decode())
        assert tokens[:4] == ['su', '0', 'sh', '-c']
        bodies.append(tokens[4])
    # Frozen AOSP IOemLock v1, blob977746c4dd669c404e01eec51b438731d649ec1c:
    # Binder transactions2/3 are the read-only carrier/device getters. Keep
    # their actual Parcel replies and the peer's real request/reply log.
    oem = 'android.hardware.oemlock.IOemLock/default'
    inspection = ('set -e; ' + bodies[1] + "; printf '\\nREVERSE_%s\\n' PTS_OEM_CARRIER_QUERY; "
                  'service call ' + oem + ' 2; ' +
                  "printf '\\nREVERSE_%s\\n' PTS_OEM_DEVICE_QUERY; " +
                  'service call ' + oem + ' 3; cat ' + base + '.log')
    commands = [root(capture, 'PTS_TRANSFER_CAPTURE'), chunks[0] + '\n', chunks[1] + '\n',
                root(decode + bodies[0], 'PTS_TRANSFER_ACTIVATION'), root(inspection, 'PTS_TRANSFER_INSPECT')]
    for i in [0, 3, 4]:
        subprocess.run(['bash', '-n'], input=commands[i].encode(), check=True)
    exercise_capture(capture, chunks, data_file, encoded)
    exercise_capture(capture, chunks, data_file, encoded, corrupt=True)
    with tempfile.TemporaryDirectory(prefix='reverseandroid-buffered-decode-') as temp:
        data = pathlib.Path(temp) / 'peer.b64'
        elf = pathlib.Path(temp) / 'peer.elf'
        data.write_text(encoded)
        tested = decode.replace(data_file, str(data)).replace(elf_file, str(elf)) + 'true'
        subprocess.run(['bash', '-c', tested], check=True)
        assert elf.read_bytes() == executable
    args.output.mkdir(parents=True, exist_ok=False)
    names = ['PTS_TRANSFER_RX_0', 'PTS_TRANSFER_DATA_0', 'PTS_TRANSFER_DATA_1', 'PTS_TRANSFER_ACTIVATION', 'PTS_TRANSFER_INSPECT']
    phases = []
    for i, (command, name) in enumerate(zip(commands, names)):
        raw = command.encode('ascii')
        assert 0 < len(raw) <= 4096
        filename = f'input-{i}.txt'
        (args.output / filename).write_bytes(raw)
        phase = {'input_file': filename, 'input_bytes': len(raw), 'input_sha256': digest(raw),
                 'response_marker': 'REVERSE_' + name, 'failure_markers': ['REVERSE_PTS_TRANSFER_CAPTURE'],
                 'data_for_waiting_guest_read': i in [1, 2], 'instruction_budget': 500000000 if i in [1, 2] else 1000000000}
        if i in [1, 2]:
            phase['response_markers'] = ['REVERSE_' + name, 'REVERSE_PTS_TRANSFER_RX_1' if i == 1 else 'REVERSE_PTS_TRANSFER_CAPTURE']
            phase['all_response_markers_required'] = True
        phases.append(phase)
    result = {'schema': 'reverseandroid-buffered-privatePTY-full-transfer-proposal-v1', 'phases': phases,
              'ELF_SHA256': digest(executable), 'gzip_SHA256': digest(compressed), 'guest_ELF_path': elf_file,
              'host_actual_PTY_blocking_capture_and_prefix_hashes_passed': True,
              'host_actual_PTY_corrupt_prefix_rejected_before_next_read': True,
              'host_exact_production_decode_reconstructs_compiled_ELF': True,
              'host_mount_or_SELinux_provisioning_attempted': False,
              'OEM_query_AIDL_v1_git_blob_SHA1': '977746c4dd669c404e01eec51b438731d649ec1c',
              'OEM_Binder_query_transactions': [2, 3],
              'OEM_setter_transactions_requested': False,
              'source_bound_or_executed': False, 'actual_Android_integration_claimed': False,
              'requires_successful_original_HAL_control_before_adoption': True,
              'requires_newest_canonical_source_SHA_and_count_before_input': True}
    (args.output / 'proposal.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'phase_sizes': [p['input_bytes'] for p in phases], 'real_host_PTY_capture_and_corruption_tests_passed': True,
                      'emulator_executed': False, 'canonical_adoption_started': False}))


if __name__ == '__main__':
    main()
