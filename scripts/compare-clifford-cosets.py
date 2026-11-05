#!/usr/bin/env python3
"""Fresh-process timings, separate allocation counts, identical graph circuits."""
import argparse
import hashlib
import json
import math
import platform
import statistics
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('probe')
parser.add_argument('--repeats', type=int, default=7)
parser.add_argument('--checkpoint')
args = parser.parse_args()
if args.repeats < 1:
    parser.error('positive repeats required')
root = Path(__file__).resolve().parent.parent
report = {
    'platform': platform.platform(),
    'compiler': subprocess.check_output(['c++', '--version'], text=True).splitlines()[0],
    'flags': 'C++17 -O3 -DNDEBUG, same executable and circuit; fourth argument selects preserved reference',
    'method': 'Fresh process per sample; one warmup per mode; alternating paired samples. Timing/RSS use disabled allocation counters; separate counted processes report allocation traffic. Process RSS includes graph/state preparation; validation follows the RSS reading.',
    'previous_main': 'dabc4af7bf52e0b51e9bdb26f73bd35a8190646b',
    'repeats': args.repeats,
    'probe_sha256': hashlib.sha256(Path(args.probe).read_bytes()).hexdigest(),
    'source_sha256': {
        name: hashlib.sha256((root / name).read_bytes()).hexdigest()
        for name in ['src/integration/reduced_state_extensions.inc', 'tests/clifford_cosets.cpp', 'scripts/compare-clifford-cosets.py']
    },
    'cases': [],
}

def checkpoint():
    if args.checkpoint:
        target = Path(args.checkpoint)
        pending = Path(str(target) + '.pending')
        pending.write_text(json.dumps(report, indent=2) + '\n')
        pending.replace(target)

def run(n, t, family, optimized, counted=False):
    row = json.loads(subprocess.check_output([
        args.probe, str(n), str(t), family, str(int(optimized)), str(int(counted))
    ], text=True, timeout=90))
    assert row['coefficients'] == 1 << t and row['max_error'] < 1e-10, row
    return row

def quantiles(values):
    ordered = sorted(values)
    return {'median': statistics.median(ordered), 'p95': ordered[math.ceil(.95 * len(ordered)) - 1]}

for n in [128, 256, 512, 1000]:
    for t in [8, 16]:
        for family in ['commuting', 'mixed', 'affected']:
            record = {'qubits': n, 'T': t, 'family': family, 'modes': {}}
            digest = None
            for optimized, label in [(False, 'reference'), (True, 'optimized')]:
                warm = run(n, t, family, optimized)
                if digest is None:
                    digest = warm['state_digest']
                assert warm['state_digest'] == digest
                counted = run(n, t, family, optimized, counted=True)
                assert counted['state_digest'] == digest
                record['modes'][label] = {'samples': [], 'allocation_probe': counted}
            for repeat in range(args.repeats):
                for optimized in ([False, True] if repeat % 2 == 0 else [True, False]):
                    row = run(n, t, family, optimized)
                    assert row['state_digest'] == digest
                    record['modes']['optimized' if optimized else 'reference']['samples'].append(row)
            for mode in record['modes'].values():
                mode['summary'] = {
                    name: quantiles(row[name] for row in mode['samples'])
                    for name in ['ms', 'RSS_before_kib', 'RSS_after_kib', 'peak_RSS_kib', 'payload_bytes', 'max_error']
                }
            report['cases'].append(record)
            checkpoint()
report['complete'] = True
checkpoint()
print(json.dumps(report, indent=2))
