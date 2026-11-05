# Android guest: SCS exit, real UART, bootconfig and ISAR3

This cycle continues draft PR #51, exact source commit
de6243c4b8f87b88f1afcf97cf6b06b0736906ff. The only execution change is the
named read of ID_AA64ISAR3_EL1 in the original MinimalGuest interpreter.
The pinned Mundo-X dependency, single QuantumRuntime/InverseMemory,
adaptive memory, immutable kernel and initrd remain unchanged.

## Reproduced input

- Official Cuttlefish image build: 16644040.
- Embedded GKI version: 6.12.74-android16-6-g3ec022196c4e-ab15076761-4k.
- Kernel SHA-256: f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8.
- Initial RVI2 RVG: 12,020,000 real ARM64 instructions.
- Initial RVG SHA-256: 174a4582b80139f7fe00213068fe3a091bb7fff15a438ca0e0e290d377934733.
- Source artifact: run 40810724189, artifact 12870630189.

The exact embedded kernel also matches Git blob
72a705f0a93c5373a3600dd3f468e7daf6e8c392 in the official Android prebuilt
revision a34bf55c778e84c5e6c1c9bfdca5500f8dbc1199. The image build and
embedded kernel build are distinct identifiers. The historical development
probe's fixed expected_upstream_linux field still names its Linux 6.1 test
fixture; it is not the identity of this Android kernel. Use the byte-pinned
image, embedded version and actual UART as authoritative evidence.

## Actual execution

Local uninterrupted transactions and host-wall-paused/resumed execution
progressed through 19,060,000; 39,050,000; 58,190,000; then 65,788,352
instructions. PCs left the early physical SCS region around 48 million
instructions and entered the virtual kernel. There were zero architectural
exceptions at the first unsupported register read.

The original kernel emitted 1,922 characters of genuine PL011 UART,
including its Linux version, REVERSE machine model, early console setup,
PSCI discovery, dynamic shadow call stack setup, memory setup and:

    Load bootconfig: 118 bytes 7 nodes

This proves actual bootconfig consumption and early kernel progress.
Android first-stage /init, Binder, ADB and UI have not been observed.

## Minimal correction and acceptance

At instruction 65,788,352, PC 0xffffffc0804007a8 / physical 0x404007a8,
the genuine opcode 0xd5380668 reads ID_AA64ISAR3_EL1 into X8. Its encoding
is op0=3, op1=0, CRn=0, CRm=6, op2=3. The baseline CPU implements none of
the identified ISAR3 extensions, so its identification value is zero.
EL0 access retains the undefined-instruction trap; writes and unimplemented
neighboring registers remain rejected with atomic state rollback.

Primary register metadata:
https://github.com/torvalds/linux/blob/v6.12/arch/arm64/tools/sysreg

Independent Clang assembly of MRS X8,S3_0_C0_C6_3 must reproduce the actual
0xd5380668 opcode in CI. The 1,828-check system suite covers the real read,
unchanged unrelated registers, sole XI/Ether authority, exact restoration,
read-only rejection, unknown-register rejection and EL0 traps.

The old CPU stops without changing the 65,788,352-instruction RVG:
4cefac0e7d8271999c759010d35ac9911d3ad029ab20723d4376b04607b2b49f.
The repaired CPU advances to 65,788,353 with X8=0. A further nine real
instructions reach the next unsupported register, ID_AA64MMFR4_EL1, at
PC 0xffffffc0804007d0, opcode 0xd5380788 (system register 0xc03c).
Direct 10-step and resumed 1+9-step execution are byte identical.

The dedicated workflow independently reproduces the old exact failure from
the 12M source checkpoint, checks its RVG hash, then validates the repair
and next failure. It records actual console and final checkpoint.

## Next cycle

Resolve the observed MMFR4 read using its actual architectural baseline
field rules and privilege tests, then resume the exact saved RVG.
Continue to the next genuine CPU/device blocker. Storage/partitions and
Android userspace/display remain later gates. No main merge, dependency
change, second memory authority or third-party game execution is included.
