#!/usr/bin/env python3
"""Verify the complete transport and restore only ordinary files into a new directory."""
import gzip
import hashlib
import io
import json
from pathlib import Path
import shutil
import sys
import tarfile

source, destination = map(Path, sys.argv[1:])
manifest = json.loads((source / 'manifest.json').read_text())
assert manifest['schema'] == 'reverseandroid-checkpoint-transport-v1'
expected_names = [f'checkpoint.part-{i:03d}' for i in range(len(manifest['parts']))]
assert expected_names and [p['file'] for p in manifest['parts']] == expected_names
for part in manifest['parts']:
    path = source / part['file']
    assert path.is_file() and not path.is_symlink() and path.stat().st_size == part['bytes']
    with path.open('rb') as stream:
        assert hashlib.file_digest(stream, 'sha256').hexdigest() == part['sha256']
assert not destination.exists()
destination.mkdir(parents=True)

class Parts(io.RawIOBase):
    def __init__(self):
        super().__init__()
        self.paths = iter(expected_names)
        self.current = None
    def readable(self):
        return True
    def readinto(self, buffer):
        while True:
            if self.current is None:
                name = next(self.paths, None)
                if name is None:
                    return 0
                self.current = (source / name).open('rb')
            count = self.current.readinto(buffer)
            if count:
                return count
            self.current.close()
            self.current = None

total = 0
seen = set()
with io.BufferedReader(Parts()) as data, tarfile.open(fileobj=data, mode='r|gz') as archive:
    for item in archive:
        relative = Path(item.name)
        assert item.isfile() and not relative.is_absolute() and '..' not in relative.parts
        assert item.name not in seen
        seen.add(item.name)
        total += item.size
        assert total <= 4 * 1024**3
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        with archive.extractfile(item) as original, target.open('xb') as output:
            shutil.copyfileobj(original, output, length=1024*1024)
assert json.loads((destination / 'output-contract.json').read_text()) == manifest['source_contract']
with (destination / 'current.rvg').open('rb') as stream:
    assert hashlib.file_digest(stream, 'sha256').hexdigest() == manifest['source_contract']['checkpoint_sha256']
print(json.dumps({'checkpoint_transport_restored': True, 'files': len(seen),
                  'checkpoint_sha256': manifest['source_contract']['checkpoint_sha256']}))
