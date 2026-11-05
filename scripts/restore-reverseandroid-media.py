#!/usr/bin/env python3
"""Restore exactly the original disk, using bounded buffers and sparse zero runs."""
import gzip
import hashlib
import io
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
source, destination = map(Path, sys.argv[1:])
expected = json.loads((root / 'docs/evidence/c68-source/media-transport.json').read_text())
assert json.loads((source / 'manifest.json').read_text()) == expected
assert [p['file'] for p in expected['parts']] == ['disk.part-aa', 'disk.part-ab', 'disk.part-ac']
for part in expected['parts']:
    path = source / part['file']
    assert path.is_file() and not path.is_symlink() and path.stat().st_size == part['bytes']
    with path.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    assert digest == part['sha256'], part['file']

class Parts(io.RawIOBase):
    def __init__(self):
        super().__init__()
        self.files = iter(expected['parts'])
        self.current = None
    def readable(self):
        return True
    def readinto(self, buffer):
        while True:
            if self.current is None:
                part = next(self.files, None)
                if part is None:
                    return 0
                self.current = (source / part['file']).open('rb')
            count = self.current.readinto(buffer)
            if count:
                return count
            self.current.close()
            self.current = None

destination.parent.mkdir(parents=True, exist_ok=True)
digest = hashlib.sha256()
size = 0
with io.BufferedReader(Parts()) as compressed, gzip.GzipFile(fileobj=compressed) as original, destination.open('xb') as output:
    while chunk := original.read(1024 * 1024):
        size += len(chunk)
        assert size <= expected['disk_bytes']
        digest.update(chunk)
        if chunk.count(0) == len(chunk):
            output.seek(len(chunk), 1)
        else:
            output.write(chunk)
    output.truncate(size)
assert size == expected['disk_bytes'] and digest.hexdigest() == expected['disk_sha256']
print(json.dumps({'original_disk_restored': True, 'disk_bytes': size, 'disk_sha256': digest.hexdigest()}))
