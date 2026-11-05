#!/usr/bin/env python3
"""Independent expected quantities; execute the emitted XI in the owner runtime."""
import json
from pathlib import Path
import struct
import subprocess
import sys

runtime, directory = sys.argv[1], Path(sys.argv[2])
state, result = directory/'initial.rvp', directory/'calculated.rvp'
subprocess.run([runtime, 'init', str(state)], check=True, capture_output=True)
original = state.read_bytes()
def logical(data):
    assert data[:5] == b'RVPC2'
    nc, nm, _ = struct.unpack_from('<QQQ', data, 5)
    assert len(data) == 29+nc+nm
    return data[29:29+nc], data[29+nc:].split(b'\n', 1)[1]
cases = 0
for line in (directory/'calculator-expected.tsv').read_text().splitlines():
    name, expected = line.split('\t')
    result.unlink(missing_ok=True)
    p = subprocess.run([runtime, 'run', str(directory/name), str(state), str(result)], capture_output=True, text=True, timeout=10)
    if expected == 'ERROR':
        assert p.returncode != 0 and not result.exists(), (name, p.stdout, p.stderr)
    else:
        assert p.returncode == 0, (name, p.stderr)
        registers = [json.loads(row) for row in p.stdout.splitlines() if row.startswith('{')]
        answer = next(row['value'] for row in registers if row.get('register') == 'r0')
        assert int(answer) == int(expected), (name, answer, expected)
        assert logical(result.read_bytes()) == logical(original), 'calculator changed core or memory'
    assert state.read_bytes() == original
    cases += 1
print('CALCULATOR_NATIVE_XI_PASS', cases, 'read_only=true rejected_operations_preserve_state=true')
