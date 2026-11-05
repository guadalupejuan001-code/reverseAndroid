# C07: continue the proved original Android init state

C06 proved the original BTI at `/init` entry and stopped at EL0 PC
`0x2a4004`, native transition 6,250,795,356. Its pre-entry controller
intentionally rejected that later PC on restart. C07 adds a separate,
explicit continuation from that accepted evidence.

## Diagnosed post-entry failures

The first genuine C07 run (40971727802, commit `4d09bef`) delivered SIGILL
to init and then burned its remaining instruction budget in the kernel's
panic self-branch. Its original progress-only gate accepted this. That run
is negative boot evidence, despite its green workflow conclusion. The final
controller and workflow explicitly reject guest kernel panics.

The baseline CPU correctly traps EL0 MIDR reads; Linux genuinely emulates
both reads at `0x33c000` and `0x33c200` and returns to the next original
instruction. The fatal read is `MRS X5,DCZID_EL0` (`0xd53b00e5`) at
`0x33d670`. DCZID was incorrectly rejected by the old generic privilege
guard. It is now readable at EL0 and reports DZP according to the kernel's
SCTLR.DZE setting. Native DC ZVA honors that setting, zeroes the same
64-byte aligned XI/Ether block, faults on device memory and preserves the
real SYS syndrome on denial. CTR user access also honors SCTLR.UCT with
the actual register/Rt/direction fields in the trap. No direct privileged
MIDR access is granted and no CPU feature is advertised to bypass Linux.

The next original libc instruction, `LD1 {v1.16b,v2.16b},[x2],#32`
(`0x4cdfa041`) at `0x33e6a8`, was unsupported. The same native SIMD bank
now supports LD1/ST1 multiple structures, including all four register counts,
arrangements, wraparound and post-index forms. The nearby actual string
routine also requires CMEQ zero and integer pairwise reductions; these now
operate on the existing vectors with independent lane-value checks.
Subsequent original routines require SHRN/RSHRN, byte CNT and widening
pairwise sums; these use the same sparse native vector state. The allocator
also executes CRC32CX at `0x2fe420`. Native CRC32 and CRC32C register
instructions now consume the real operands and are checked against an
independent MSB-first polynomial oracle and standard `123456789` values.
No CRC result, allocation result or syscall is supplied by a host shortcut.
The later original code also uses immediate and per-lane register shifts;
their signed-low-byte and boundary behavior is checked with an independent
bit-position oracle. REV16/REV32/REV64 preserve element ordering inside
their encoded groups; scalar element copies clear the unused bits of the
destination. SLI/SRI preserve the old destination bits, allowing the kernel's
original vector SHA implementation to verify its module contents. The
expanded FP/SIMD suite has 11,675 checks; the
cache-access suite has 82 and CRC has 76. Cycle, system and PCI regression
suites separately preserve native exception delivery and full-state restore.

Architecture references: Arm's [DC ZVA instruction](https://support.arm.com/documentation/ddi0601/2024-06/AArch64-Instructions/DC-ZVA--Data-Cache-Zero-by-VA?lang=en),
[system-register rules](https://documentation-service.arm.com/static/5fad544dca04df4095c1c90c),
and [LD1 multiple structures](https://support.arm.com/documentation/111108/2026-03/SIMD-FP-Instructions/LD1--multiple-structures---Load-multiple-single-element-structures-to-one--two--three--or-four-registers-).

## Source and authority

`experiments/android-post-init-cycle.json` pins the complete C06 state,
the exact C06 report, persisted counter seed/cursor/cadence/phase and the
unchanged external Android disk. The reviewed C06 state SHA-256 is
`4c20cd09e353f82b032c17636ae2d72cec6988df5a5b00afbd756464c4dcd575`.
The report SHA-256 is
`ec12937b7c33a3aff439f3c388bbd32b1f538e08667d0f83e61736489666bdbf`.

`tools/android_verified_init_source.mjs` independently checks the report,
serialized CPU, RVI payload, internal checksum and machine v7 counter data.
The native controller separately checks both immutable hashes, exact
restoration, EL0 PC/count, original 64 entry bytes and counter identity.
`run_boot_cycle` binds the accepted entry to every byte of the native
CPU/device/XI snapshot before it publishes anything or advances the machine.

The existing original XI/Ether QuantumRuntime remains the sole memory
authority. This change does not replace CPU instructions with host syscalls,
map guest pages, modify the boot arguments, change RAM capacity or attach
a different disk. The new logic is a host continuation controller.

## Reporting and failure behavior

`prior_user_instruction_verified` carries the accepted C06 milestone.
`verified_entry_checkpoint_sha256` identifies that original complete state.
`first_user_instruction_verified` stays false in the continuation: the first
instruction is not executed or proved a second time. Kernel exceptions,
SVC handlers and ERET execute through the same native CPU path.

A rejected native transition retains the exact successful prefix and leaves
the next instruction unexecuted. Atomic journal generations preserve CPU,
XI memory, device state and the report together. A failure remains a failing
workflow even when its diagnostic checkpoint is successfully published.

The CLI accepts `--continue-verified-init=input/current.json` only for the
reviewed C06 artifact. It cannot repeat the first-entry stop or reseed the
clock. A successor artifact needs its own reviewed provenance pin; the
original proof cannot be replayed against changed state.

## Reproduction

Use REVERSE-PC with the original mundo-X commit
`e2778d962af22f6f1a0794058dd4685fc2573966`. Build with the existing verified
Ether compatibility and SoftFloat scripts. Restore C06 from GitHub run
40948887628, artifact `reverse-c05-original-init-continuation`, and the
unchanged disk from run 40910284394, artifact
`reverse-android-c03-genuine-partition-disk`.

```sh
node tools/android_verified_init_source.mjs input/current.rvg input/current.json
build/guest-pci-cycle input/current.rvg disk-input/android-slot-a.raw output-journal 180000 4c20cd09e353f82b032c17636ae2d72cec6988df5a5b00afbd756464c4dcd575 --chunk=1000000 --instructions=100000000 --continue-verified-init=input/current.json
```

The dedicated workflow performs this actual continuation and exports its
newest complete checkpoint, UART, instruction inspection and source proof.
Android framework, display and native games each still require direct
execution evidence; a carried entry proof does not establish those milestones.
