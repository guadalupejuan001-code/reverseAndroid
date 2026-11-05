#!/usr/bin/env python3
"""Fetch a pinned migration ZIP with gh; verify all archive bytes before extraction."""
import hashlib
import json
from pathlib import Path
import stat
import subprocess
import sys
import zipfile

root = Path(__file__).resolve().parents[1]
name, target = sys.argv[1:]
target = Path(target)
lock = json.loads((root / 'state/bootstrap-artifacts.json').read_text())
pin = lock['artifacts'][name]
archive = target.parent / f'reverseandroid-{name}.zip'
archive.parent.mkdir(parents=True, exist_ok=True)
with archive.open('xb') as output:
    subprocess.run(['gh', 'api', f"repos/{lock['repository']}/actions/artifacts/{pin['id']}/zip"],
                   stdout=output, check=True, timeout=600)
with archive.open('rb') as stream:
    assert hashlib.file_digest(stream, 'sha256').hexdigest() == pin['sha256'], name
with zipfile.ZipFile(archive) as package:
    assert sum(x.file_size for x in package.infolist()) <= 4 * 1024**3
    for info in package.infolist():
        path = Path(info.filename)
        assert not path.is_absolute() and '..' not in path.parts
        assert not stat.S_ISLNK(info.external_attr >> 16)
    package.extractall(target)
archive.unlink()
print(json.dumps({'artifact': name, 'id': pin['id'], 'complete_zip_sha256_verified': pin['sha256']}))
