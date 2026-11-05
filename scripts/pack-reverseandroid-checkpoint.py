#!/usr/bin/env python3
"""Create independently hashed, <=200 MiB checkpoint transport pieces."""
import hashlib
import json
from pathlib import Path
import sys
import tarfile

source, destination = map(Path, sys.argv[1:])
assert not destination.exists()
destination.mkdir(parents=True)
limit = 200 * 1024 * 1024

class Parts:
    def __init__(self):
        self.files = []
        self.current = None
    def write(self, data):
        count = len(data)
        while data:
            if self.current is None or self.current.tell() == limit:
                if self.current is not None:
                    self.current.close()
                name = f'checkpoint.part-{len(self.files):03d}'
                self.current = (destination / name).open('xb')
                self.files.append(name)
            size = min(len(data), limit - self.current.tell())
            self.current.write(data[:size])
            data = data[size:]
        return count
    def close(self):
        if self.current is not None:
            self.current.close()

parts = Parts()
with tarfile.open(fileobj=parts, mode='w|gz', compresslevel=1, dereference=True) as archive:
    for path in sorted(source.rglob('*')):
        relative = path.relative_to(source)
        if not path.is_file() or relative.parts[:2] == ('native', 'journal'):
            continue
        if relative.as_posix() in ('native/current.rvg', 'native/frame.ppm'):
            continue
        assert not path.is_symlink(), str(relative)
        archive.add(path, arcname=relative.as_posix(), recursive=False)
parts.close()
manifest = {'schema': 'reverseandroid-checkpoint-transport-v1', 'compression': 'tar+gzip', 'parts': []}
for name in parts.files:
    path = destination / name
    with path.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    manifest['parts'].append({'file': name, 'bytes': path.stat().st_size, 'sha256': digest})
manifest['source_contract'] = json.loads((source / 'output-contract.json').read_text())
manifest['progress'] = json.loads((source / 'c46-progress.json').read_text())
(destination / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps(manifest, indent=2))
