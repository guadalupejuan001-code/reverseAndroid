#!/usr/bin/env python3
"""Real CLI integration: dynamic 128-qubit circuit, RVPC2 persistence and atomic rejection."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

exe = Path(sys.argv[1]).resolve()

def call(*args, ok=True):
    p = subprocess.run([str(exe), *map(str, args)], text=True, capture_output=True, timeout=90)
    assert (p.returncode == 0) == ok, (args, p.stdout, p.stderr)
    return [json.loads(s) for s in p.stdout.splitlines() if s.strip()]

with tempfile.TemporaryDirectory(prefix='reverse-pc-machine-') as root:
    root = Path(root)
    blank = root / 'blank.rvp'
    active = root / 'active.rvp'
    continued = root / 'continued.rvp'
    program = root / 'build.xi'
    follow = root / 'follow.xi'
    failure = root / 'invalid.xi'
    call('init', blank)
    program.write_text('''#!reverse-pc-machine-v1
set r0 0
set r1 1
set r2 128
h $r0
label chain
cx $r0 $r1
add r0 1
add r1 1
cmp r1 r2
jlt chain
t 0
tdg 127
expect Z:0 Z:$r0
set r4 42
store8 500 r4
print r1
qstats
''')
    result = call('run', program, blank, active)
    assert result[0]['type'] == 'expectation' and abs(result[0]['value'] - 1.0) < 1e-9
    assert result[1] == {'type': 'register', 'register': 'r1', 'value': 128}
    assert result[2]['type'] == 'quantum_stats' and abs(result[2]['norm'] - 1) < 1e-9
    assert active.read_bytes().startswith(b'RVPC2')
    follow.write_text('''#!reverse-pc-machine-v1
load8 r0 500
add r0 1
store8 501 r0
print r0
expect Z:0 Z:127
mem_read 501
''')
    result = call('run', follow, active, continued)
    assert result[0] == {'type': 'register', 'register': 'r0', 'value': 43}
    assert abs(result[1]['value'] - 1.0) < 1e-9 and result[2]['value'] == 43
    assert active.read_bytes() != continued.read_bytes()
    before_in = hashlib.sha256(active.read_bytes()).hexdigest()
    before_out = hashlib.sha256(continued.read_bytes()).hexdigest()
    failure.write_text('''#!reverse-pc-machine-v1
set r0 77
store8 500 r0
h 10
label loop
jmp loop
''')
    call('run', failure, active, continued, ok=False)
    assert hashlib.sha256(active.read_bytes()).hexdigest() == before_in
    assert hashlib.sha256(continued.read_bytes()).hexdigest() == before_out
    failure.write_text('''#!reverse-pc-machine-v1
store8 500 99
not_an_instruction
''')
    call('run', failure, active, continued, ok=False)
    assert hashlib.sha256(active.read_bytes()).hexdigest() == before_in
    assert hashlib.sha256(continued.read_bytes()).hexdigest() == before_out
    print(json.dumps({'suite': 'xi_machine_cli', 'dynamic_qubits': 128,
                      'persisted': True, 'reloaded': True, 'loop_rejected': True,
                      'failed_destination_unchanged': True,
                      'snapshot_bytes': active.stat().st_size}))
