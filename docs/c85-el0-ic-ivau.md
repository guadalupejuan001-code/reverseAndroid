# C85: actual ART JIT IC IVAU failure

The original Android log bytes captured from the admitted diagnostic snapshot at
95,047,727,998 instructions report `SIGILL`, `ILL_ILLOPC`, in PID 3386
(`system_server`), TID 3583 (`Jit thread pool`), at `0x7544ce7e64`.
This is distinct from the subsequent ptrace stop of the process's other threads.

A read-only indexed RFP reader inspected that address in the exact earlier
90,547,727,998 snapshot, before the process disappeared. It authenticated the
whole snapshot and original kernel, used the kernel's actual BTF and page tables,
verified PID/TGID/name, and independently reproduced the full original observer's
64 code bytes and ptrace field. The fault word is `0xd50b7529`: `IC IVAU, X9`.
The actual `SCTLR_EL1` is `0x200002034f4591d`, with UCI bit 26 enabled.
The unmodified baseline nevertheless raises an undefined-instruction exception
for this valid EL0 cache instruction. The later observed Zygote exit is SIGKILL;
it is not evidence of a separate illegal crypto instruction.

The correction applies only to the exact IC IVAU encoding at EL0. UCI disabled
still generates a complete EC=0x18 SYS trap, including the operand register.
Existing EL1 behavior and every other SYS encoding remain unchanged. This
coherent, cacheless model clears only derived host instruction-fetch hints.
It performs no data access and chooses not to raise the implementation-defined
IC IVAU address permission fault. It does not grant access to RAM or devices.

The original 870 files remain immutable. The build script creates a separate
source copy containing both the independently validated CLZ correction and this
explicit cache correction. RAM, disk, seed, ticks, IRQ checks, XI/Ether authority,
RVG/MXIM/RFP formats, SELinux and all original boot gates are unchanged.

Local validation reproduces the exact baseline error, then checks all 32 Rt
encodings and all 64 byte offsets: 2,048 cases and 79,978 checks, including
unchanged integer/SIMD/quantum state, full snapshot restore, exact UCI trap
syndromes, rejection of neighboring privileged SYS operations, and a real guest
store/cache/fetch code replacement. The previous 82 cache checks, 1,992 system
checks and 30,887 CLZ cases also pass. The dedicated workflow additionally runs
the exact instruction on real AArch64 hardware while replacing executable code.
That independent hardware result must pass before runtime adoption.

Validation run 42221035217 has now passed on both x86 and real AArch64. The
warmed native ARM function returned 7, then 42 after replacement using the exact
`0xd50b7529` instruction. Both signed archives, their original test reports and
the x86 runner `957f22e2c8c8d3591029529855c733a05fc5c468e9a6e881ff5238bea6ff102f`
are pinned in the C85 evidence and artifact lock. The local IC suite also passes
with undefined-behavior sanitization.

Evidence is in `docs/evidence/c85`. Reproduce the memory observation with the
exact C83 stage6 snapshot and its original read-only thread report:

```sh
python3 scripts/diagnose-c85-ic-ivau.py SOURCE_DIRECTORY NEW_OUTPUT.json
bash scripts/build-reverseandroid-ic-ivau.sh
```

Architectural references: Arm's [self-modifying code explanation](https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/caches-self-modifying-code-implementing-clear-cache),
[Arm Architecture Registers](https://documentation-service.arm.com/static/60df1943677cf7536a55ddef),
and Arm DDI 0487B.a, sections on EL0 cache maintenance access and IC IVAU
([Arm-authored manual copy](https://kib.kiev.ua/x86docs/ARM/ARMARMv8/DDI0487B_a_armv8_arm.pdf)).

The current primary continuation is still C84. Adoption must resume its latest
complete admitted state, including the full accumulated console and every input
event. It must not replay the receiver, transfer or activation commands, rewind
to the pre-fault diagnostic state, patch a snapshot or synthesize boot properties.
The original init/zygote recovery and actual usable UI still require verification.

C85 selects the then-latest fully admitted C84 stage4 state at 99,547,727,998,
with complete local transport recovery and original preflight verified. Its
five existing input events, pending activation and entire accumulated console
are restored without replay. The first correction run is bounded to 100 million
instructions, waiting runs to one billion, and ordinary continuation to two
billion. These are host checkpoint boundaries; guest time and timing policy
are unchanged. Later old-engine C84 states are a reference after this explicit
correction fork.

A temporary HAL restart no longer discards an otherwise admitted native
boundary. Completion still requires all the original three-channel live checks,
unchanged policy, original Binder service observations, live SystemServer and
the original PID1 boot-completed gate. Missing live checks remain pending;
they never count as boot success or cause another input to be replayed.
