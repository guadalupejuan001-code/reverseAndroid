# Android C01: real checkpoint continuation and a read-only kernel observer

The existing progress analyzer called every stop other than an instruction
budget an architectural stop. Genuine host time pauses were therefore
misclassified. The analyzer now separates host pauses, instruction budgets,
device/IRQ waits, EL0 observation boundaries and architectural errors. Changes
in pointers and memory remain stage-unclassified; they do not identify a
completed Android boot.

This cycle starts at source
5a61473f1f836dc94196b3c0f671c0be20626751 and the genuine RVG
a67981ef832fb7ac097941d5b3afc1542bc50fe586ee37c342a807014c4a27da,
at 3,081,466,448 instructions. Mundo-X remains pinned to
e2778d962af22f6f1a0794058dd4685fc2573966.

## Actual execution

The original, unchanged native probe ran 250,000 instructions directly and
125,000 + restoration + 125,000 instructions from the same input. Both saved
states and the concatenated UART are identical. The result is
0a7eef426c7a90199c7823b5c308f5bbe298fa0374923381e8c1c48c220f26cf.

Four subsequent windows requested 45-second host budgets. Together they ran
66,170,000 more guest instructions in 184.187 seconds, including restoration
and serialization. Each stopped at host_wall_budget, with no new UART and
no unsupported instruction/device stop. These host timings are not a
performance comparison or a phone measurement.

The last genuine RVG contains **3,147,886,448 instructions**, SHA-256
0212d6cc1a98b327e5f14ab74bec3e93a3c09c62350408846cc3b126dc97f854.
The untouched input remains independently verifiable.

## Kernel observations and limits

The new host-only observer reads a persisted copy of RVG2/RVI2, validates
the immutable GKI SHA-256 and the RVG checksum, reconstructs its byte view
from the original immutable backing XOR adaptive deltas, and follows the
saved stage-1 page tables. It never restores, runs or writes guest state.
It rejects quantum bindings, unsupported memory records, other kernel
images, relocated boot profiles and unsupported page granules.

Symbol-table positions are explicitly tied to the entire kernel hash.
Task and context offsets come from that kernel's BTF. Equivalent duplicate
BTF layouts are accepted; inconsistent layouts fail. IRQ register decoding
requires both artificial entry-frame links to agree with pt_regs. The
active task uses the actual CPU frame rather than its old saved context.

Both endpoint snapshots show:

- PID 35 waiting in tracer_init_tracefs_work_func / down_write.
- trace_event_sem owned by PID 13, which is runnable in
  trace_event_eval_update. Its interrupted instruction and lookup registers
  change between the snapshots.
- PID 1 inside scx_init / bpf_struct_ops_desc_init / BTF name lookup.
  X21 changes from 0x17aaf to 0x1f8dd; X22 is 0x28b57. These observations
  are consistent with ongoing BTF lookup, not completion of that initcall.
- PID 39 has an initramfs extraction stack through unlz4, do_copy and
  kernel_write. The sampled header, body and file-position globals do not
  change across the windows. Extraction progress is therefore not proved.

The observed semaphore wait is real and its owner is identified. The
evidence does not establish a circular deadlock, a resolved tracer delay,
or a single complete root cause for the unfinished boot. Android /init,
ADB, services and guest display remain unobserved. EL0 entry by itself is
also insufficient to identify the executable.

## Validation

Local validation completed on the original executor:

- 8 progress-analysis tests, including the genuine late counter range.
- 1,976 native system checks and the original 4,096-qubit witness.
- Direct/resumed state equality and UART equality.
- Observer next PC, physical PC, opcode, ticks and exception count compared
  against the original native probe, on both the input and final RVG.
- Archived task fields, recorded non-current stacks and a real IRQ frame
  compared on the original input.
- Corrupt checkpoint and wrong immutable kernel rejection.
- Input bytes remain unchanged after observation.

Reproduce the native replay comparison with
tests/android_kernel_observer_cli.mjs. It accepts a genuine RVG, the
original probe JSON, its exact SHA-256 and an optional archived task dump.
The dedicated workflow additionally repeats the direct/resumed comparison
and observer/native comparison on the original CI-published 180M checkpoint.
That CI result is separate from the local late-checkpoint evidence.

## Next discriminating experiment

Continue only from the validated last RVG. Sample the BTF lookup position,
PID 13's lookup state and semaphore ownership at a new bounded interval.
Inspect PID 39's actual scheduling and shmem write state if the extraction
globals remain static. Do not hide hung-task warnings, alter kernel/initrd,
change virtual ticks, skip instructions or call a rising instruction
counter a completed Android boot.

This change modifies host diagnostics and tests. It does not change the
CPU, XI/Ether authority, kernel, initrd, product UI or APK. Main integration
and a stable release are outside this cycle.
