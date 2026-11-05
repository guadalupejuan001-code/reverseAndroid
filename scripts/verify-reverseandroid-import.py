#!/usr/bin/env python3
"""Authenticate the complete imported C68 source without changing the engine."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / 'state/upstream-snapshot.json').read_text())
assert manifest['commit'] == '37f5b4610c5c31981e359fcf8fd0b303c14f01fd'
assert len(manifest['files']) == 870
seen = set()
for item in manifest['files']:
    name = item['path']
    assert name not in seen and not Path(name).is_absolute() and '..' not in Path(name).parts
    seen.add(name)
    path = root / name
    assert path.is_file() and not path.is_symlink(), name
    data = path.read_bytes()
    actual = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
    assert actual == item['git_blob_sha1'], 'Changed imported source: ' + name
print(json.dumps({'import_verified': True, 'upstream_commit': manifest['commit'],
                  'verified_files': len(seen), 'original_engine_and_codecs_unchanged': True}))
