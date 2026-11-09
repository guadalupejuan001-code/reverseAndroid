#!/usr/bin/env python3
"""Derive only the bounded host memory-record reader from a validated profile.

The original imported reader, guest execution, snapshot bytes and finite record
limits stay unchanged. Every other derived tool must match the admitted host
transport profile byte for byte.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

from host_transport_continuation import validate_generated_profile

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/host-transport-runtime'
VALIDATED = ROOT / 'build/host-native/host-transport-manifest.json'
READER = ROOT / 'extensions/host-memory-lines/rvg_memory_lines.mjs'


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read(path):
    return json.loads(Path(path).read_text())


def derive(destination, plan):
    destination = Path(destination).resolve()
    assert not destination.exists(), 'The derived observer destination must be new'
    reference = read(ROOT / 'state/experiments/c90r-plan.json')
    validated = read(VALIDATED)
    assert digest(VALIDATED) == reference['host_validation']['manifest_SHA256']
    validate_generated_profile(validated, read(BASE / 'host-transport-manifest.json'), BASE / 'tools')
    assert digest(READER) == plan['expected_observer_reader_SHA256']
    original = ROOT / 'tools/rvg_memory_lines.mjs'
    assert digest(original) == validated['derived_tool_files']['rvg_memory_lines.mjs']['source_SHA256']
    assert digest(original) == validated['derived_tool_files']['rvg_memory_lines.mjs']['build_SHA256']
    subprocess.run([sys.executable, str(ROOT / 'scripts/verify-reverseandroid-import.py')],
                   cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    destination.mkdir(parents=True)
    shutil.copytree(BASE / 'tools', destination / 'tools')
    for name in ['state', 'tests']:
        (destination / name).symlink_to(ROOT / name, target_is_directory=True)
    shutil.copyfile(READER, destination / 'tools/rvg_memory_lines.mjs')
    files = {}
    changed = []
    for name, entry in sorted(validated['derived_tool_files'].items()):
        assert Path(name).name == name
        actual = digest(destination / 'tools' / name)
        expected = digest(READER) if name == 'rvg_memory_lines.mjs' else entry['build_SHA256']
        assert actual == expected, name
        if actual != entry['build_SHA256']:
            changed.append(name)
        files[name] = dict(source_SHA256=entry['source_SHA256'], build_SHA256=actual)
    assert changed == ['rvg_memory_lines.mjs']
    assert {p.name for p in (destination / 'tools').iterdir() if p.is_file()} == set(files)
    manifest = dict(
        schema='reverseandroid-explicit-host-memory-line-reader-v1',
        baseline_validation_run_id=reference['host_validation']['run_id'],
        baseline_validation_commit=reference['host_validation']['execution_commit'],
        baseline_manifest_SHA256=digest(VALIDATED),
        native_SHA256=reference['expected_native_SHA256'],
        frame_SHA256=reference['expected_frame_SHA256'],
        host_snapshot_capacity_bytes=reference['host_snapshot_capacity_bytes'],
        original_870_files_unchanged=True,
        original_rvg_memory_lines_SHA256=digest(original),
        replacement_rvg_memory_lines_SHA256=digest(READER),
        max_record_bytes=16384,
        max_allowed_record_bound_bytes=65536,
        changed_tool_files=changed,
        derived_tool_files=files,
    )
    output = destination / 'host-observer-manifest.json'
    output.write_text(json.dumps(manifest, sort_keys=True, indent=2) + '\n')
    shutil.copyfile(VALIDATED, destination / 'baseline-host-transport-manifest.json')
    print(json.dumps(dict(manifest=str(output), manifest_SHA256=digest(output),
                          changed_tool_files=changed, native_binary_changed=False,
                          original_870_files_unchanged=True)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination')
    parser.add_argument('--plan', default='state/experiments/c91-plan.json')
    args = parser.parse_args()
    path = Path(args.plan)
    derive(args.destination, read(path if path.is_absolute() else ROOT / path))


if __name__ == '__main__':
    main()
