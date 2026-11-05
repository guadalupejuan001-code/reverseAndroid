# C04: resume Android and diagnose its time-dependent RNG initialization

The first C04 continuation resumed the complete C03 PCI state at
1,817,250,000 retired instructions. Run 40924584145 reached 3,171,300,000,
passed BTF validation and finished unpacking the genuine normal ramdisk.
Its final RVG SHA-256 is
`10fc574e0b6fc582a1bc120c53d1f5bb50a95b35bd0f325034554da5a95409c7`.
The original `/init` has not executed. A heartbeat, elapsed time or an
unpacked rootfs does not establish an Android boot.

Read-only stack inspection places PID 1 in `jent_entropy_init` through
`jent_entropy_collector_alloc`, `jent_gen_entropy`, `jent_measure_jitter`,
`jent_condition_data`, `jent_hash_time` and SHA3/Keccak. The pinned GKI source
rejects samples when the first, second or third time differences are zero:

- https://github.com/aosp-mirror/kernel_common/blob/3ec022196c4e/crypto/jitterentropy.c
- https://github.com/aosp-mirror/kernel_common/blob/3ec022196c4e/crypto/jitterentropy-kcapi.c

The historical interpreter advances its architectural counter by exactly
one tick per retired instruction. The RNG's need for timing variation is a
specific hypothesis for this new frontier; a bounded A/B experiment must
test it against the actual Android checkpoint.

## Opt-in counter-read latency

The original fixed counter remains the default and retains historical
snapshot bytes. An explicit `--counter-latency=host-entropy` initializes
32 bytes with Linux `getrandom` once, outside CPU transactions. Actual
CNTPCT_EL0/CNTVCT_EL0 MRS instructions then incur 0..15 additional counter
ticks. HMAC-SHA256 over a domain and stream index supplies those bounded
latencies. CNTFRQ remains 1 MHz, timer comparisons use the same monotonically
increasing counter, and every ARM instruction still executes normally.

Machine state version 6 records the seed and sample cursor, enabling exact
rollback and restart without calling host randomness inside a transaction.
The small HMAC cache is derived and is never an additional RAM authority.
Old v1..v5 state readers remain supported. A v6 state needs this updated
native engine; an older C03 APK cannot restore it. Fixed seeds are confined
to diagnostic fixtures, not the genuine Android experiment. This timing
model is not a claim of hardware entropy certification.

The experiment compares 50 million instructions from the same pinned
3,171,300,000-instruction input with and without latency. Two initial runs
also exercise separate 15-minute seeded continuations from that input.
Routine workflow validation then bounds its separate continuation to
200 million instructions or five minutes, while preserving the longer-run
evidence. Each seeded run receives its own host seed. Its report records the
seed origin and SHA-256 identity,
actual counter sample count, full checkpoint hash, UART and exact CPU stop.
The counter model does not patch or skip a crypto initcall, alter the kernel,
move its PC or replace XI/Ether's memory engine.

## Checkpoint and first-entry guarantees

The kernel, ramdisk, bootargs and 8,738,832,384-byte disk remain independently
pinned. Native restore must reproduce the input RVG bytes before enabling
the optional model. One immutable disk attachment and the original XI/Ether
guest persist across bounded CPU transaction chunks. RAM, limits and kernel
configuration remain those of the checkpoint. Chunks of one million
instructions have full-state parity against the former 50,000-step chunks;
a small local microbenchmark found roughly 12% lower CPU time, not a proven
end-to-end Android speedup.

Periodic saves use two complete generations and an atomically published,
SHA-bound pointer. Partial generations cannot replace the complete state.
Existing output directories are rejected. SIGINT/SIGTERM request a save at
the next CPU transaction boundary. A rejected transition rolls back and
binary search retains its exact successful prefix, UART and device state,
leaving the rejected instruction unexecuted for a concrete next repair.

Execution stops at the first EL0 boundary. The verifier checks the original
`/init` virtual entry and 64 bytes, executes one actual instruction and
requires the expected PC/counter transition before continuing. Failure,
including an architectural translation fault, preserves the prior state.
Fixture tests exercise this contract; only a genuine run can prove `/init`.

## Remaining boot requirements

After kernel initialization and genuine `/init`, prove PCI driver loading,
GPT enumeration and mounting the logical partitions inside `super`.
The selected normal-boot fstab requires `/data` from a `userdata` partition,
which the current eight-partition base lacks. `misc` is absent as an auxiliary
partition; the absent `vendor_boot_a` fstab entry is recovery-only. Zram,
removable media and optional virtiofs are not missing physical partitions.
Do not silently change the attached disk identity or resize a live kernel.
Prepare and pin a separate complete disk/boot profile when that stage is
reached, with real filesystem and logical-partition validation.

The current 128 MiB boot profile also needs a measured capacity decision
before starting Android services. Read-only pinned-symbol inspection of the
3.1713B state reports 21,361 managed pages and 15,566 globally free pages
(about 61 MiB free); per-CPU vmstat batches may lag. This is not a framework
capacity proof. Then prove framework startup, Android's
composer/EGL display path, input, networking and audio. The existing virtio
GPU 2D pixel witness does not establish Android graphics or native game
compatibility; those require separate Android and 3D driver evidence.

`tools/android_partition_readiness.mjs` automates the selected-fstab audit.
The workflow extracts the fstab from the pinned ramdisk and checks its exact
hash independently of the acquisition manifest's disk identity. Required
physical mounts, auxiliary entries, inactive recovery entries and logical
partitions are reported separately. An eight-partition boot-input inventory
cannot establish that `/data` exists. This manifest comparison does not claim
GPT-sector, logical-filesystem, AVB or mount validation; six semantic tests
cover that distinction and prevent optional/external devices becoming false
missing-disk requirements.

## Validation

`guest_boot_cycle_test` covers exact continuation, pixel persistence,
successful-prefix localization, entry rollback and journal recovery under
UBSan. `minimal_guest_counter_test` runs real ARM MRS/timer/IRQ/ERET and PCI
graphics instructions, compares direct execution against repeated restart,
and emits its complete fixture stream for an independent Node HMAC oracle.
Android instrumentation compares the same full native state hash and then
force-stops/relaunches the host process to verify v6 seed/cursor persistence.
The established system, PCI, floating-point, byte-memory and original
4096-qubit contract checks still apply.
