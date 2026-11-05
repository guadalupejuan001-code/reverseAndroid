# Android kernel CPU enumeration and memory initialization

Starting source: PR #52, 06373c2cdc0270013e31c94de8d4378acbc2ffc2.
Starting RVG SHA-256:
73cc99ba20eed6488858db212aef0473998d482de1462f6c9c6f1479866464e3
at 65,788,362 original ARM64 instructions.

The immutable official Cuttlefish build 16644040 GKI and combined initrd
remain byte identical. The kernel identifies itself as
6.12.74-android16-6-g3ec022196c4e-ab15076761-4k. The pinned Mundo-X dependency
is e2778d962af22f6f1a0794058dd4685fc2573966.

## Explicit CPU identification

The exact upcoming kernel instructions read MMFR4, PFR2 and FPFR0.
These three named identification registers now return the baseline value
zero, with existing EL0 traps and read-only rejection. PFR2 advertises no
FPMR/MTE extensions; FPFR0 advertises no FP8 instructions. MMFR4 E2H0 does
not create an EL2 mode: PFR0 continues to advertise EL2 absent.

The definitions and encodings are independently specified in:
https://github.com/torvalds/linux/blob/v6.12/arch/arm64/tools/sysreg

The kernel passes CPU identification, detects its supported Cortex-A53
errata, applies its own boot alternatives, and parses the real command line.

## Actual paired-exclusive failure and repair

At instruction 69,126,117, the original kernel rejects opcode 0xc87f06c0
at PC 0xffffffc0800e0120: LDXP X0,X1,[X22]. The exact pre-instruction RVG is:
fc84fb8d6f9312d239be2d1365ed72aaa46c7c7e491d0d2b6f3779f438f7588d.

The original CPU now implements the 32/64-bit pair forms of LDXP, LDAXP,
STXP and STLXP using the same XI/Ether bytes. It validates the entire access
before changing either half, tracks physical reservations, respects CLREX
and XI-write invalidation, and retains acquire/release ordering in the
existing coherent single-core machine. It does not enable LSE/CASP.

A live 16-byte reservation uses inner machine-state v4, including its
existing IFLS field. Historical states retain exact v2/v3 serialization.
Once the reservation is cleared, legacy serialization is retained when
the other state fields permit it. No second RAM/runtime is introduced.

1,976 local system checks pass, including both pair widths, all
acquire/release combinations, success/failure status, no-reservation
rejection, CLREX, shared XI invalidation, alignment aborts and byte-exact
restoration. A separately assembled bare-metal pair witness is compared
with QEMU in the existing ARM64 compatibility workflow. QEMU is only an
independent development oracle.

The older ISAR3 milestone test now stops before MMFR4 without requiring
that future register support remain absent. The RAM-formatting performance
oracle compares only its intended bounds-formatting difference; subsequent
CPU instructions are present in both measured variants.

## Translation reuse and observation

The interpreter retains 64 nonpersistent address hints, never guest RAM
bytes. A hint includes the 4 KiB virtual page, access mode, current EL,
TTBR0/TTBR1, TCR and SCTLR. Every store invalidates all hints, including
hints installed while validating the store itself; XI/RXE writes and TLBI
also invalidate them. Faults are not cached. Restoration starts with empty
hints. REVERSE_PC_TRANSLATION_REFERENCE disables lookup/store for independent
byte comparisons. No guest instruction, interrupt or virtual tick is skipped.

Paired stores pin the entire validated physical extent before writing,
even when their first half replaces their own page-table descriptor. Tests
cover this edge and the equivalent single-store invalidation case.

The optional development-probe --stop-el0=1 boundary stops after a real
architectural transition and before the first EL0 instruction. It changes
neither checkpoint layout nor guest behavior. It is an observer, not a
claim that the entered program is Android /init. Program identity must be
verified separately against the real initrd ELF.

## Numeric spelling of the same memory reads

A uint64 byte address can now query the same AdaptiveOverrideMap index
without formatting and reparsing a decimal string for each read. This is
an additional read API on the same InverseMemory/ByteAccess authority, not
a new memory or a materialized cache. The pinned checkout is unchanged;
the hash-checked local compatibility generator supplies the overload.

The original canonical-string path remains authoritative whenever the
address-digit budget is below 20 or any quantum binding exists. This
preserves digit-budget rejection, deterministic quantum bytes, unresolved
handles and arbitrary-precision addresses. Tests also distinguish a zero
override from a nonzero global background. Reads preserve exact core,
memory, revisions and serialization. REVERSE_PC_NUMERIC_READ_REFERENCE
selects the original string path for comparisons.

Native tests and two local ABBA comparisons produce byte-identical RVGs,
including the actual BTF/timer-heavy Android interval at 560 million
instructions. The dedicated CI records separate ABBA oracles for numeric
reads and translation hints; throughput measurements apply only to those
specific workloads.

## Genuine debug-slot initialization

The unchanged kernel reaches MSR DBGBCR0_EL1,X2 (0xd51000a2) at instruction
179,326,448, PC 0xffffffc08040f2cc, with X2=0. Linux hw_breakpoint_reset clears
the control and value registers of its one reported breakpoint/watchpoint
slot during cold boot. Only the named slot-zero BCR/BVR/WCR/WVR inactive
state is supported: zero writes and zero reads. Nonzero programming,
additional slots and EL0 access remain rejected. No hardware debug
exceptions or performance-monitoring support is claimed.

Primary description of that initialization:
https://github.com/torvalds/linux/blob/v6.12/arch/arm64/kernel/hw_breakpoint.c

## Observed kernel progress

The actual guest reached and passed 106 million instructions with zero
architectural exceptions. Real UART includes CPU features, command-line
processing, cache tables, zones, software IO TLB and:

    SLUB: HWalign=64, Order=0-3, MinObjects=0, CPUs=1, Nodes=1

The dedicated continuation CI replays the real MMFR4 failure from the
published input, reaches the exact pre-LDXP RVG, proves that the old CPU
rejects LDXP without modifying it, then compares continuous and resumed
100,000-step execution across the live pair reservation. It continues the
unmodified kernel to the real allocator milestone and preserves the RVG.

Read-only initrd analysis found 42,513,920 vendor CPIO bytes and 5,680,128
generic CPIO bytes (48,194,048 total). The original guest still has 128 MiB
logical RAM; an analysis of its size is not proof that Android services fit.

Local continuation has passed 641 million original instructions. Real UART
also shows RCU, ARM timer, SELinux/landlock/eBPF, NETLINK routing, audit,
thermal governors, ASID allocator, and registration of the PL011 serial
console. Timer IRQ exceptions after this point are expected architectural
activity, not interpreter failures. First-stage /init has not yet been
observed at this checkpoint. The real kernel warns that its tracer-init
worker is blocked on trace_event_sem. Independent read-only task/stack
inspection, using the immutable kernel's own BTF layout, shows PID 1
advancing through btf_check_all_metas during inet_init. That is a diagnostic
observation of the current slow stage, not proof that the warning is resolved.

The dedicated workflow additionally compares cached and uncached execution
in ABBA order with byte-identical RVGs, replays the exact debug-slot failure
on the old CPU, and verifies continued execution after its zero reset.

Continue from the latest genuine checkpoint rather than repeating SCS.
Next gates are actual first-stage /init execution, required storage and
partition devices, Android services and display. None is claimed complete
by the CPU-identification or allocator milestones alone.

The Android integration CI exercises its existing smaller system witnesses.
The current Java/JNI guest UI still has a 32 MiB checkpoint transport and
16 MiB import path; the real GKI continuation here uses the native Linux
probe. Product import/streaming and UI progress remain integration work.
Passing the Android application tests does not prove this 80 MiB RVG boots
inside the phone application.
