# REVERSE-PC Android Guest: real boot-latency diagnosis and reversible pause

## Real starting state (not a fabricated Android boot)

The guest is the **original XI/Ether MinimalGuest** running the authentic public Android Cuttlefish ARM64 GKI kernel, PLATFORM vendor+generic compressed ramdisks and exact RVI2 Android bootargs from **official build 16644040**. The preserved source checkpoint (parent PR #49) is at **12,020,000 ARM64 instructions**, RVG SHA-256 `174a4582b80139f7fe00213068fe3a091bb7fff15a438ca0e0e290d377934733`, [CI 40804061395](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/40804061395). No Android `/init` EL0 transition, UART, Binder, ADB, Android display or Launcher has been observed.

## Why a large instruction budget looks frozen

Six consecutive **real original guest** 2M-instruction checkpoints repeatedly sampled physical PCs between `0x40407588` and `0x404075c8`, but their x19 data counters changed `0x3021a4 -> 0x258654`, x20 advanced, and classical InverseMemory byte deltas increased `34,562 -> 124,610`. Each 2M-instruction block took about **11.2 seconds** in the previous CI host (~178k instructions/s). This is **active CPU work**, not a stationary infinite loop. Independent disassembly of exact immutable GKI at the physical PC shows an ARM64 bytecode/CFA dispatch with PACIASP/AUTIASP/SCS rewrite operations, matching the early kernel SCS `.eh_frame` patch family, not a finished Android service. Exact same-build symbol assignment still requires a pinned symbol map. Kernel upstream reference: https://android.googlesource.com/kernel/common/+/10d97b74e2eef787d823f9bc05cb044b47d25c41/arch/arm64/kernel/pi/patch-scs.c

### What was measured, not assumed

[CI 40809312623](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/40809312623) verified the original SHA-pinned 12M RVG, **5/5** liveness classifier tests and **7/7** RVI auditor tests. It ran the same **1,000,000 real ARM64 transitions** with original XI/Ether runtime under 10K, 100K and 500K transaction chunks. Every final RVG was **byte-for-byte identical**. Wall times were **11.439 / 11.221 / 11.431 seconds** respectively; the largest observed improvement was only about **1.9%** and is not a robust fix for minutes of kernel boot. No default CPU semantics or original 10K chunk size was changed based on this result.

## Implemented safe host control (development emulator runner)

Original `tools/guest_linux_probe.cpp` now accepts two optional bounded **checkpoint-only** host controls, with **no change to guest CPU registers, timer semantics, startup code, ELF image or persistent XI/Ether memory**:

```sh
build/linux-probe checkpoint.rvg 1000000 paused.rvg --chunk=10000 --wall-ms=2000
build/linux-probe paused.rvg 10000 resumed.rvg --chunk=10000
```

- `--chunk`: original XI/Ether byte transaction boundary size, 1..1,000,000 instructions. Default stays 10,000 until a meaningful benchmark proves better.
- `--wall-ms`: soft host wall-time budget, 1..600,000 ms. Checked **between full atomic transactions**, never mid-instruction; the actual process can exceed the nominal wall limit while loading/saving the immutable snapshot or finishing a chunk. This is not an exact real-time guarantee.
- A `REVERSE_ANDROID_GUEST_HEARTBEAT` line appears on **stderr** every ~2 seconds with actual executed instruction count, PC, EL and host elapsed. Stdout remains one machine-readable final JSON; `stop_reason=host_wall_budget` distinguishes a voluntary host pause from actual guest ISA/device failure.
- RVG2/RVI2 and the sole original QuantumRuntime/InverseMemory remain the persistent state; no second emulator, hidden RAM mirror, kernel patch bypass or synthetic boot messages.

**Verified watchdog test:** [CI 40809738238](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/40809738238) **passed**. An actual 2,000 ms host budget stopped after **150,000 original ARM64 instructions** (from 12,020,000 to **12,170,000**) with `stop_reason=host_wall_budget`, one actual heartbeat, EL1 PC `0x40407578`. The saved RVG SHA-256 was `c25b6da0385408eb7d8cacc7f5473c0c1ef51f55d68c827936d8ccaaeb27e9c6`, and it was **byte-identical** to a 150,000-instruction uninterrupted run. Resuming for 10,000 additional instructions produced RVG SHA-256 `957c20b845175423ad535fc0a3690768d902026b92bca63a35c42ac058741f28`, also **byte-identical** to an uninterrupted 160,000-instruction run. These are actual guest instructions and snapshots, not Android `/init` or UI boot.

## Remaining bottlenecks and future real integration

1. Exact CPU hot-path profile is being investigated using the same original XI/Ether interpreter and independently identical snapshots. Do **not** advertise a large speedup from chunk size alone.
2. The first-stage guest still has not executed `/init`. Once the SCS `.eh_frame` processing exits, identify the next real PC/opcode/device blocker rather than continuing arbitrary instruction budgets.
3. The guest must implement the Cuttlefish-compatible virtio/block/partition devices (and validate Android AVB/fstab/required modules) before real first-stage mounts; the currently staged `.ko` files do **not** create hardware.
4. Only after Android services and SurfaceFlinger/graphics devices work can the **Android Guest window inside REVERSE-PC desktop** show an authentic interface. The development watchdog is not an installed Android guest window or APK and must be connected to REVERSE event handling separately.

No `main` merge, forced history rewrite, pinned dependency changes, second XI/Ether RAM authority or proprietary Wild Rift assets.

## Verified original XI/Ether RAM hot-path repair

An independent **real AOSP guest** CPU hotspot profile in
https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/40809872756
passed with exact normal-vs-profiled guest RVG parity. Its gprof flat profile
sampled 1,000,000 authentic ARM64 guest transitions:

- AdaptiveOverrideMap::get: **33.16%** sampled host CPU time.
- MinimalGuest::read8: **12.15%**.
- Eager guest::hex(address) formatting: **9.87%**.
- std::to_string(physical address): **9.37%**.

The original MinimalGuest::ram_range() always evaluated the error-message
expression `"Guest address outside logical RAM: " + hex(address)` even when
the bounds check succeeded, creating and formatting a string for virtually
every memory read. In exactly **one existing header site** it now uses the
**same** short-circuit bounds predicate and creates the **same exact**
runtime_error text **only if the predicate fails**. No guest address check
is removed, no read is skipped, and the single existing XI/Ether runtime
and InverseMemory store remain authoritative.

### Exact before/after same-runner measurement (2026-10-28)

[CI 40810412947](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/40810412947)
**PASSED**. The workflow independently loaded the exact parent `src/minimal_guest.hpp`
from commit `b8733350402fb6c9be088d6b923294e5c9a0a43e`, checked that
the current source changed **only this one branch's RAM bounds message**,
and built both old/optimized original XI/Ether binaries with the **same**
pinned Mundo-X SHA and O2 options. Native system regression: **1,816
checks passed**, including exact exception text, accepted last legal
RAM byte and invalid-address rollback.

Real input: original official Cuttlefish ARM64 build 16644040 RVI2 guest
12,020,000-step checkpoint SHA-256
`174a4582b80139f7fe00213068fe3a091bb7fff15a438ca0e0e290d377934733`.
Four **1,000,000-original-ARM64-instruction** executions alternated
old, optimized, optimized, old:

| Version | Real guest end steps | Wall time (CI runner) |
| --- | ---: | ---: |
| Before run 1 | 13,020,000 | 11.308 s |
| After run 1 | 13,020,000 | 4.743 s |
| After run 2 | 13,020,000 | 4.745 s |
| Before run 2 | 13,020,000 | 11.308 s |

**Measured improvement: 2.384x** on this reproducible workload,
approximately **88,435 -> 210,789 guest instructions/s** in that run.
Every output RVG was **byte-for-byte identical**, checkpoint SHA-256
`17b7ff81c058d72de07dcd7fac2a0cbab02909c9cc8086f86eaf955cf1cd806c`.
No different kernel, instruction skipping, ISA emulation change, or
memory/QuantumRuntime implementation was introduced. This is a
**measured local workload speedup**, not an end-to-end Android boot,
graphics FPS or phone-hardware performance claim.

### Liveness and nonblocking console proof

[CI 40809738238](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/40809738238)
**PASSED** with the same original guest state. A soft host-time budget
of 2,000 ms returned reason `host_wall_budget` at **12,170,000 real ARM64
guest instructions**, emitted `REVERSE_ANDROID_GUEST_HEARTBEAT` on
stderr and retained a fully restorable RVG. Uninterrupted 150K-step
execution gave the **same complete RVG** SHA256
`c25b6da0385408eb7d8cacc7f5473c0c1ef51f55d68c827936d8ccaaeb27e9c6`.
After 10,000 resumed steps the checkpoint also equaled direct 160K-step
execution, SHA256
`957c20b845175423ad535fc0a3690768d902026b92bca63a35c42ac058741f28`.
Soft wall time is checked at atomic transaction boundaries, so exact
wall deadline guarantees are **not** made.

**Still pending:** Android first-stage `/init` execution and actual guest
virtio/partition devices; the original REVERSE desktop GUI does not
yet expose this development watchdog. Next priorities are a verified
same-build kernel symbol/early-patch completion check, and then genuine
virtual block and AOSP partition mounting. Android Launcher, ADB,
SurfaceFlinger and Wild Rift are not demonstrated.
