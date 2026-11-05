# C48: repair SurfaceFlinger UCVTF and reduce measured host overhead

C47 passed byte-identical old/view execution and saved a clean boundary at
27,497,785,172 instructions, 1,078,482,139 bytes, SHA-256
`c5e1c735decdb47775a060f8abf52478b8a14ada7e62dfee6046115c11566335`.
The next actual ISA stop is at 28,109,215,989 instructions, PC `0x6fc27b3020`,
opcode `0x6e21d820`: UCVTF V0.4S,V1.4S. A read-only observation identifies
SurfaceFlinger thread 778; source V1 contains the integers 0, 1, 2, 3. The failed
tail (`c7ced705daf4da54c33a375c1c7e5e47719c25eada8e6766edd5c4908330a3ba`)
is diagnostic evidence only. Genuine continuation uses the clean C47 stage 1.

## Exact instruction repair

Implement allocated SCVTF/UCVTF vector 2S, 4S and 2D forms through the existing
ArmFloat integer conversion. Copy the source before destination aliases, convert
only active lanes, accumulate FPSR and obey FPCR, and clear the upper half for
2S. Preserve the FP access trap. Reserved 1D and unsupported FP16 forms remain
rejected atomically. The existing scalar conversion remains unchanged.

Release tests cover 6,690 cases and 241,588 checks. The independent AArch64
oracle executes the actual encodings across signedness, lane widths, aliases,
random integer values, all rounding modes, FZ and DN. Its CI job must pass before
genuine continuation. The official encoding/intrinsic mapping is documented in
[Arm's Neon reference](https://arm-software.github.io/acle/neon_intrinsics/advsimd.html).

## Host work performed on every instruction

Keep the original string-based require function and add a literal-array
overload. Successful literal checks no longer construct temporary strings;
the same error type and message are produced on failure. This changes host
allocation work, not guest checks or the original snapshot decoder.

The original interrupt selector scans 64 IDs for active priorities and then 64
IDs for candidates. Keep it unchanged as next_irq_reference; the new selector
visits only set bits, in the same ascending ID order, with the same strict
priority comparison, masks, targets and active-priority threshold. Timer and
device levels are reevaluated on every call. Every architectural step retains
its original timer and IRQ checks. No instruction batching or cached interrupt
result is introduced. Release/UBSan compare 126,946 checks including 100,000
random states, timer, UART and PCI levels.

## Full host disk authentication

Direct C47 continuation exposed another host cost: authenticating the complete
10,888,413,184-byte immutable Android disk took over a minute and a half before
CPU execution. Every byte is hashed twice: once in the whole-disk SHA-256 and
once in its 64 KiB block digest. Block hashes are checked again on cache misses.
The separate read-only frame observer repeats attachment authentication.

An optional `REVERSE_PC_OPENSSL_DISK_SHA=1` host build uses OpenSSL EVP SHA-256
for exactly those same whole-disk and per-block checks. The original portable
GuestSha256 is unchanged and remains the default and independent test oracle.
The snapshot SHA, RVG/MXIM encoding, original decoder, XI/Ether runtime, logical
RAM, counter seed/cadence, disk bytes and security gates are unchanged.
No supplied digest replaces reading and authenticating the disk. Failed hashing
or a changed block still rejects the transaction.

The local 64 MiB comparison returned identical digests in 241,873 us with the
portable algorithm and 39,491 us with the host backend. Attaching the entire
authentic disk with the host backend took 18.266 s and returned its original
SHA-256 `4c2eaf3fa7d81e9649072fc81f0651f4129bd9f41d4a9f9964cc627741a7b92e`.
These timings measure disk authentication, not a whole Android cycle.

Tests compare both algorithms across padding boundaries and arbitrary input
chunking; exercise multi-block and partial-tail reads; and require rejection
after file mutation, truncation and out-of-range reads. The optional CMake test
is enabled by `REVERSE_PC_TEST_OPENSSL_DISK_SHA=ON`. Native host tools additionally
link `-lcrypto`; ordinary and Android builds gain no new dependency by default.

## Continuation gates

Before genuine continuation, the pinned C47 binary and candidate must produce
byte-identical original RVG output after a million real Android instructions
from the same authenticated checkpoint. A separate 20-million-instruction
comparison measures execution itself, excluding attach/restore/save time, and
must preserve all final bytes with the original guard and IRQ paths as oracles.
Locally, execution took 17.167 s with the original paths and 11.553 s with the
candidate (1.486x throughput); both produced SHA-256
`0bee633570e51e7b3fa85205c79daa6f12937fe64627270d374437f77f76bff0`.

The first genuine budget replays 611,430,817 instructions from clean C47 stage 1
and stops immediately before UCVTF. It must independently reconstruct the exact
bytes previously seen in the diagnostic tail, now with a clean completed budget
and validated output contract. The failed tail is never used as input. Upload
this new clean boundary, then execute exactly one instruction and verify the
actual SurfaceFlinger result 0.0, 1.0, 2.0, 3.0. A following million-instruction
stage probes further graphics startup before longer budgets. Later bounded
stages retain the C47 explicit host-bound contract and C46 boot evidence gates.
Completed boot requires live system_server lineage, framework proof, live PID1
sys.boot_completed=1 and enforcing SELinux. Scanout requires separate evidence
and visual review. Host speed, Zygote or instruction counts alone do not prove
completed boot.
