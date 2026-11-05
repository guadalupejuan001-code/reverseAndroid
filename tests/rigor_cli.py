#!/usr/bin/env python3
"""REVERSE-PC adversarial tests for the real XI CLI and durable RVPC1 state."""
import hashlib
import json
import random
import subprocess
import sys
import time
from pathlib import Path

exe = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
rng = random.Random(20261025)

def run(args, should_pass=True):
    p = subprocess.run([str(exe), *map(str, args)],
                       text=True, capture_output=True, timeout=35)
    if should_pass and p.returncode:
        raise AssertionError(f"Unexpected failure: {args}\n{p.stdout}\n{p.stderr}")
    if not should_pass and not p.returncode:
        raise AssertionError(f"Unexpected success: {args}\n{p.stdout}")
    return p

def output_lines(stdout):
    return [json.loads(line) for line in stdout.splitlines() if line.strip()]

def value(rows, address):
    values = [r for r in rows
              if r.get("type") == "memory_read" and r.get("address") == str(address)]
    assert len(values) == 1, (address, values)
    assert values[0]["deterministic"], values[0]
    return values[0]["value"]

def sha(data):
    return hashlib.sha256(data).hexdigest()

start = time.monotonic()
run(["demo", out])
source = (out / "initial.rvp").read_bytes()
a = (out / "program-a.rvp").read_bytes()
b = (out / "program-b.rvp").read_bytes()
source_sha = sha(source)
assert source[:5] in (b"RVPC1",b"RVPC2")
assert len(source) < (1 << 20), "unexpected snapshot size"

# Property-based check: random sequences of basis-preserving X/CNOT gates
# must agree with a separate, simple classical bitwise model.
for trial in range(96):
    expected = 5
    instructions = []
    for _ in range(6 + trial % 11):
        if rng.getrandbits(1):
            bit = rng.randrange(8)
            expected ^= 1 << bit
            instructions.append(f"x {100 + bit}")
        else:
            control, target = rng.sample(range(8), 2)
            if expected & (1 << control):
                expected ^= 1 << target
            instructions.append(f"cx {100 + control} {100 + target}")
    instructions += ["mem_read 9", "mem_read 7", "mem_read 8"]
    program = out / "random.xi"
    output = out / "random.rvp"
    program.write_text("\n".join(instructions) + "\n")
    rows = output_lines(run(["run", program, out/"initial.rvp", output]).stdout)
    assert value(rows, "9") == expected, (trial, expected, rows)
    assert value(rows, "7") == 5
    assert value(rows, "8") == 10
    assert sha((out/"initial.rvp").read_bytes()) == source_sha

# Addresses scale logically, but the experiment must not allocate that domain.
# Test both the implicit-zero state and the global H rule at long addresses.
addresses = [("9" * digits) for digits in (64, 256, 1024, 4096)]
address_checks = 0
for address in addresses:
    program = out / "far.xi"
    program.write_text(f"mem_read {address}\nprobz {address}\n")
    initial_rows = output_lines(run(["run", program, out/"initial.rvp",
                                    out/"far-initial.rvp"]).stdout)
    assert value(initial_rows, address) == 0
    initial_probability = [r for r in initial_rows if r.get("type") == "probz"]
    assert len(initial_probability) == 1
    assert initial_probability[0]["probability_one"] == 0
    b_rows = output_lines(run(["run", program, out/"program-b.rvp",
                              out/"far-b.rvp"]).stdout)
    b_probability = [r for r in b_rows if r.get("type") == "probz"]
    assert len(b_probability) == 1
    assert b_probability[0]["probability_one"] == .5
    address_checks += 2

# Reject deliberate damage to headers, lengths, payloads, truncations and tails.
# Existing destinations must remain unchanged on failure.
fuzzed = 0
for i in range(180):
    basis = source if i % 3 == 0 else a if i % 3 == 1 else b
    damaged = bytearray(basis)
    mode = i % 5
    if mode == 0:
        index = rng.randrange(len(damaged))
        damaged[index] ^= 1 << rng.randrange(8)
    elif mode == 1:
        damaged = damaged[:rng.randrange(len(damaged))]
    elif mode == 2:
        damaged.extend(bytes((rng.randrange(256),)))
    elif mode == 3:
        # Length fields at offsets 5:13 and 13:21, stored as little endian.
        start_offset = 5 if rng.getrandbits(1) else 13
        damaged[start_offset:start_offset + 8] = ((1 << 40) + i).to_bytes(8, "little")
    else:
        damaged[:5] = b"BAD!!"
    damaged_path = out / "corrupt.rvp"
    damaged_path.write_bytes(damaged)
    existing = out / "must-preserve.rvp"
    existing.write_bytes(b"TEST-OUTPUT-MUST-NOT-BE-REPLACED")
    run(["run", out.parent.parent/"examples"/"inspect.xi", damaged_path, existing],
        should_pass=False)
    assert existing.read_bytes() == b"TEST-OUTPUT-MUST-NOT-BE-REPLACED"
    fuzzed += 1

# A failing program must not overwrite an existing output or its input state.
invalid = out / "invalid.xi"
invalid.write_text("mem_store 7 77\nnot_an_instruction\n")
existing = out / "must-preserve.rvp"
existing.write_bytes(b"KEEP")
run(["run", invalid, out/"initial.rvp", existing], should_pass=False)
assert existing.read_bytes() == b"KEEP"
assert sha((out/"initial.rvp").read_bytes()) == source_sha

print(json.dumps({
    "suite": "rigorous_cli_and_snapshot",
    "seed": 20261025,
    "random_X_CNOT_programs": 96,
    "long_address_cases": address_checks,
    "maximum_logical_address_digits": 4096,
    "damaged_snapshots_rejected": fuzzed,
    "invalid_program_preserved_input_output": True,
    "initial_sha256": source_sha,
    "seconds": round(time.monotonic()-start, 3)
}, separators=(",", ":")))
