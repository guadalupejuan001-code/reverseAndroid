#!/usr/bin/env python3
"""Run real XI circuits in fresh processes, retaining matched raw samples."""
import argparse
import hashlib
import json
import math
import os
import platform
import statistics
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("probe")
parser.add_argument("--legacy")
parser.add_argument("--parallel")
parser.add_argument("--full", action="store_true")
parser.add_argument("--checkpoint")
parser.add_argument("--repeats", type=int, default=5)
args = parser.parse_args()
if args.repeats < 1:
    parser.error("repeats must be positive")


def optional_text(path):
    file = Path(path)
    return file.read_text().strip() if file.exists() else None


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


root = Path(__file__).resolve().parent.parent
report = {
    "platform": platform.platform(),
    "sizes": [128, 256, 512, 1000],
    "compiler": subprocess.check_output(["c++", "--version"], text=True).splitlines()[0],
    "build": "CMake Release: -O3 -DNDEBUG C++17 for both large probes",
    "previous_main": "0d8926bd65b01877eeef6e3172259284f8944a88",
    "cpus_visible": os.cpu_count(),
    "cgroup_cpu_max": optional_text("/sys/fs/cgroup/cpu.max"),
    "cgroup_memory_max": optional_text("/sys/fs/cgroup/memory.max"),
    "random_seed": 20261025,
    "probe_sha256": sha256(args.probe),
    "source_sha256": {
        name: sha256(root / name)
        for name in ["tests/integrated_large.cpp", "scripts/integrated-matrix.py",
                     "src/integration/parallel_observation.hpp"]
    },
    "measurement": (
        "Native steady_clock; execution process RSS/HWM before validation, including "
        "the independent Clifford oracle and circuit inputs; total RSS/HWM also includes "
        "inverses, snapshots and copied verification branches. noncliff_ms includes "
        "atomicity checkpoints; T_gate_ms separately times successful T executions. "
        "The full matrix and warmups verify restore/inverse/RNG; timing-only repeated "
        "samples retain forward invariants and skip repeated restore/inverse."
    ),
    "repeats": args.repeats,
    "cases": [],
    "comparisons": {},
    "parallel": {},
}
if args.legacy:
    report["previous_probe_sha256"] = sha256(args.legacy)


def checkpoint():
    if args.checkpoint:
        target = Path(args.checkpoint)
        pending = Path(str(target) + ".pending")
        pending.write_text(json.dumps(report, indent=2) + "\n")
        pending.replace(target)


def run(n, family, depth, cap, *, fast=1, shortcut=1, direct=1, exe=None, timing=False):
    command = [exe or args.probe, str(n), family, str(depth), str(cap), str(fast)]
    if not exe:
        command.extend([str(shortcut), str(direct)])
    env = dict(os.environ)
    if timing:
        env["REVERSE_PC_TIMING_ONLY"] = "1"
    else:
        env.pop("REVERSE_PC_TIMING_ONLY", None)
    result = json.loads(subprocess.check_output(command, text=True, timeout=240, env=env))
    if not result.get("reference_budget_rejected_atomically"):
        assert result["max_error"] < 2e-8 and abs(result.get("norm", 1) - 1) < 2e-8, result
        if family == "adversarial":
            assert result["budget_rejected_atomically"], result
            assert result["T_accepted_before_budget"] == int(math.log2(cap)), result
    return result


def quantiles(values):
    values = sorted(values)
    return {"median": statistics.median(values), "p95": values[math.ceil(.95 * len(values)) - 1]}


def compare(n, family, depth, cap, baseline, metric):
    # Validate each mode completely before collecting matched timing samples.
    baseline(False)
    run(n, family, depth, cap)
    samples = {"baseline": [], "candidate": []}
    for repeat in range(args.repeats):
        order = ["baseline", "candidate"] if repeat % 2 == 0 else ["candidate", "baseline"]
        for label in order:
            result = baseline(True) if label == "baseline" else run(n, family, depth, cap, timing=True)
            samples[label].append(result)
    return {
        "metric": metric,
        "samples": samples,
        "summary": {
            label: {
                key: quantiles(row[key] for row in rows)
                for key in [metric, "execution_rss_kib", "execution_peak_rss_kib"]
            }
            for label, rows in samples.items()
        },
    }


for n in report["sizes"]:
    cases = [
        ("clifford", 8, 256), ("random", 8, 256), ("ghz", 16, 256),
        ("separable", 4, 256), ("partial", 4, 256), ("redundant", 4, 256),
        ("mixed", 16, 256), ("ether", 8, 256),
        ("adversarial", 5, 16), ("adversarial", 9, 256),
    ]
    if args.full:
        cases += [
            ("clifford", 32, 256), ("ghz", 64, 256), ("separable", 8, 256),
            ("adversarial", 13, 4096), ("adversarial", 17, 65536),
        ]
    for family, depth, cap in cases:
        report["cases"].append(run(n, family, depth, cap))
        checkpoint()

if args.full:
    for n in report["sizes"]:
        report["comparisons"][str(n)] = {
            "deferred_Clifford": compare(
                n, "mixed", 32, 256,
                lambda timing, n=n: run(n, "mixed", 32, 256, fast=0, timing=timing),
                "post_clifford_ms"),
            "direct_Ether": compare(
                n, "ether", 8, 256,
                lambda timing, n=n: run(n, "ether", 8, 256, direct=0, timing=timing),
                "noncliff_ms"),
            "factorization": {
                "without_shortcuts": run(n, "redundant", 4, 256, shortcut=0),
                "with_shortcuts": run(n, "redundant", 4, 256),
            },
        }
        if args.legacy:
            report["comparisons"][str(n)]["previous_main_Clifford"] = compare(
                n, "clifford", 16, 256,
                lambda timing, n=n: run(n, "clifford", 16, 256, exe=args.legacy, timing=timing),
                "prefix_ms")
        checkpoint()
    if args.parallel:
        for workers in [1, 2, 4, 8]:
            def one():
                return json.loads(subprocess.check_output([args.parallel, str(workers)], text=True, timeout=90))
            one()
            samples = [one() for _ in range(args.repeats)]
            values = quantiles(row["ms"] for row in samples)
            report["parallel"][str(workers)] = {
                "median_ms": values["median"], "p95_ms": values["p95"], "samples": samples,
            }
            checkpoint()
report["complete"] = True
checkpoint()
print(json.dumps(report, indent=2))
