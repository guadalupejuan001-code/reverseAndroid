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

def fetch(item, label, destination):
    archive = destination.parent / f'reverseandroid-{label}.zip'
    archive.parent.mkdir(parents=True, exist_ok=True)
    repository = item.get('repository', lock['repository'])
    with archive.open('xb') as output:
        subprocess.run(['gh', 'api', f"repos/{repository}/actions/artifacts/{item['id']}/zip"],
                       stdout=output, check=True, timeout=600)
    with archive.open('rb') as stream:
        assert hashlib.file_digest(stream, 'sha256').hexdigest() == item['sha256'], label
    with zipfile.ZipFile(archive) as package:
        assert sum(x.file_size for x in package.infolist()) <= 4 * 1024**3
        for info in package.infolist():
            path = Path(info.filename)
            assert not path.is_absolute() and '..' not in path.parts
            assert not stat.S_ISLNK(info.external_attr >> 16)
        package.extractall(destination)
    archive.unlink()
    print(json.dumps({'artifact': label, 'repository': repository, 'id': item['id'],
                      'complete_zip_sha256_verified': item['sha256']}))

if 'transport_manifest' in pin:
    relative = Path(pin['transport_manifest'])
    assert relative.parts[0] == 'state' and '..' not in relative.parts
    transport = json.loads((root / relative).read_text())
    assert len(pin['pieces']) == len(transport['parts'])
    pieces = target.parent / f'reverseandroid-{name}-pieces'
    pieces.mkdir()
    (pieces / 'manifest.json').write_text(json.dumps(transport, indent=2) + '\n')
    for i, item in enumerate(pin['pieces']):
        fetch(item, f'{name}-{i:03d}', pieces)
    subprocess.run([sys.executable, str(root / 'scripts/unpack-reverseandroid-checkpoint.py'),
                    str(pieces), str(target)], check=True)
else:
    fetch(pin, name, target)
