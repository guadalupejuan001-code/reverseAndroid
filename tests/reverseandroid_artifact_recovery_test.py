#!/usr/bin/env python3
"""Offline recovery regression: real transport/authentication, fixture-only gh.

The opaque payload tests checkpoint transport, not Android boot or admission.
All state, archives and downloaded bytes live in a temporary mini repository.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile


REPOSITORY = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


class ArtifactRecoveryTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='reverseandroid-recovery-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        for directory in ('scripts', 'state', 'bin', 'archives', 'original'):
            (self.root / directory).mkdir()
        for name in ('fetch-reverseandroid-artifact.py',
                     'pack-reverseandroid-checkpoint.py',
                     'unpack-reverseandroid-checkpoint.py'):
            shutil.copyfile(REPOSITORY / 'scripts' / name, self.root / 'scripts' / name)

        self.payload = b'opaque transport fixture\0' + bytes(range(256)) * 8
        self.expected_hash = hashlib.sha256(self.payload).hexdigest()
        self.files = {
            'current.rvg': self.payload,
            'output-contract.json': json.dumps({
                'checkpoint_sha256': self.expected_hash,
                'fixture_only': True,
            }).encode(),
            'c46-progress.json': json.dumps({
                'fixture_only': True,
                'android_boot_completed_verified': False,
            }).encode(),
            'frame.ppm': b'P6\n2 1\n255\n\x00\x00\x00\xff\xff\xff',
            'nested/input-ledger.json': b'{"events": [], "fixture_only": true}\n',
        }
        for relative, data in self.files.items():
            path = self.root / 'original' / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        subprocess.run([
            sys.executable, str(self.root / 'scripts/pack-reverseandroid-checkpoint.py'),
            str(self.root / 'original'), str(self.root / 'packed'),
        ], check=True, capture_output=True, text=True)
        manifest = json.loads((self.root / 'packed/manifest.json').read_text())
        (self.root / 'state/transport.json').write_text(json.dumps(manifest))
        pieces = []
        for index, part in enumerate(manifest['parts']):
            artifact_id = 100 + index
            archive = self.root / 'archives' / f'{artifact_id}.zip'
            with zipfile.ZipFile(archive, 'w') as package:
                package.write(self.root / 'packed' / part['file'], part['file'])
            pieces.append({'id': artifact_id, 'sha256': digest(archive)})
        self.lock = {
            'repository': 'fixture/recovery',
            'artifacts': {'seed': {
                'transport_manifest': 'state/transport.json',
                'pieces': pieces,
            }},
        }
        self.write_lock()

        fake_gh = self.root / 'bin/gh'
        fake_gh.write_text(
            f'#!{sys.executable}\n'
            'import os, pathlib, sys\n'
            'assert len(sys.argv) == 3 and sys.argv[1] == "api"\n'
            'endpoint = sys.argv[2].split("/")\n'
            'assert endpoint[:5] == ["repos", "fixture", "recovery", "actions", "artifacts"]\n'
            'assert len(endpoint) == 7 and endpoint[-1] == "zip"\n'
            'root = pathlib.Path(os.environ["REVERSE_ARTIFACT_TEST_DIR"])\n'
            'with (root / "gh-calls.txt").open("a") as log:\n'
            '    log.write(sys.argv[2] + "\\n")\n'
            'sys.stdout.buffer.write((root / "archives" / (endpoint[5] + ".zip")).read_bytes())\n'
        )
        fake_gh.chmod(0o755)
        self.environment = dict(os.environ)
        self.environment['PATH'] = str(self.root / 'bin') + os.pathsep + self.environment.get('PATH', '')
        self.environment['REVERSE_ARTIFACT_TEST_DIR'] = str(self.root)
        self.target = self.root / 'build/host-canonical/source'
        self.pieces = self.target.parent / 'reverseandroid-seed-pieces'

    def write_lock(self):
        (self.root / 'state/lock.json').write_text(json.dumps(self.lock))

    def recover(self):
        return subprocess.run([
            sys.executable, str(self.root / 'scripts/fetch-reverseandroid-artifact.py'),
            'seed', str(self.target), 'state/lock.json',
        ], cwd=self.root, env=self.environment, capture_output=True, text=True)

    def test_recovers_into_missing_nested_parent_without_changing_bytes(self):
        self.assertFalse(self.target.parent.exists())
        result = self.recover()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(digest(self.target / 'current.rvg'), self.expected_hash)
        actual_files = {str(path.relative_to(self.target)): path.read_bytes()
                        for path in self.target.rglob('*') if path.is_file()}
        self.assertEqual(actual_files, self.files)
        records = [json.loads(line) for line in result.stdout.splitlines()]
        restored = next(record for record in records if record.get('checkpoint_transport_restored'))
        self.assertEqual(restored['checkpoint_sha256'], self.expected_hash)
        self.assertFalse(list(self.target.parent.glob('*.zip')))

    def test_bad_archive_digest_is_rejected_before_extracting_any_piece(self):
        self.target.parent.mkdir(parents=True)
        self.lock['artifacts']['seed']['pieces'][0]['sha256'] = '0' * 64
        self.write_lock()
        result = self.recover()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('AssertionError: seed-000', result.stderr)
        self.assertFalse(self.target.exists())
        self.assertFalse(list(self.pieces.glob('checkpoint.part-*')))

    def test_authenticated_zip_with_corrupt_transport_piece_is_rejected(self):
        self.target.parent.mkdir(parents=True)
        archive = self.root / 'archives/100.zip'
        with zipfile.ZipFile(archive, 'w') as package:
            package.writestr('checkpoint.part-000',
                             (self.root / 'packed/checkpoint.part-000').read_bytes() + b'corrupt')
        self.lock['artifacts']['seed']['pieces'][0]['sha256'] = digest(archive)
        self.write_lock()
        result = self.recover()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('unpack-reverseandroid-checkpoint.py', result.stderr)
        self.assertIn('AssertionError', result.stderr)
        self.assertFalse(self.target.exists())

    def test_existing_transport_directory_is_preserved_without_fetching(self):
        self.pieces.mkdir(parents=True)
        marker = self.pieces / 'preserve-me'
        marker.write_bytes(b'previous recovery evidence')
        result = self.recover()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('FileExistsError', result.stderr)
        self.assertEqual(marker.read_bytes(), b'previous recovery evidence')
        self.assertFalse((self.root / 'gh-calls.txt').exists())
        self.assertFalse(self.target.exists())

    def test_existing_checkpoint_directory_is_not_overwritten(self):
        self.target.mkdir(parents=True)
        marker = self.target / 'preserve-me'
        marker.write_bytes(b'previous checkpoint')
        result = self.recover()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('unpack-reverseandroid-checkpoint.py', result.stderr)
        self.assertIn('AssertionError', result.stderr)
        self.assertEqual(marker.read_bytes(), b'previous checkpoint')
        self.assertEqual(list(self.target.iterdir()), [marker])


if __name__ == '__main__':
    unittest.main(verbosity=2)
