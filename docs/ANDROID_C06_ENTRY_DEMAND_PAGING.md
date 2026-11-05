# C06: let the kernel resolve the original Android /init entry page

The previous genuine C05 run (40945076273) reached 6,250,777,180 native
instruction transitions and PC `0x2a4000` at EL0. Its UART contains
`Run /init as init process`, but the first userspace instruction was not
verified. The observer's executable translation raised `0x82000006` at that
entry. This is a lower-EL instruction translation fault, not an unsupported
CPU opcode. Observing the entry before Linux had demand-paged its code
prevented the existing CPU from delivering the normal exception to Linux.

## Repair and verification contract

The host cycle independently reproduces a lower-EL instruction translation
fault at the unchanged current PC. Only syndromes `0x82000004..0x82000007`
and an exact native address/syndrome match qualify. One ordinary `run(1)`
must then enter the real EL1 synchronous vector with matching ESR/FAR/ELR
and exactly one additional native exception and instruction transition.
The existing kernel subsequently supplies its own mapping and ERET.
The host neither writes a descriptor nor skips, relocates or replaces code.

A pending unmasked native interrupt at the EL0 boundary is delivered first
through that same CPU, with its own IRQ-vector/ELR/count checks. Neither
an interrupt nor a fetch fault establishes that any user instruction ran.
These events are reported separately from the first-instruction milestone.
Other observation faults, wrong addresses, byte mismatches and verifier
rejection preserve the last committed native state and remain failures.

The original verifier is unchanged: entry PC `0x2a4000`, all 64 pinned bytes,
one actual instruction, EL0 and PC `0x2a4004`. A successful milestone stops
immediately and publishes a SHA-bound checkpoint. Existing snapshots,
the kernel, ramdisk, attached disk, RAM and XI/Ether authority are unchanged.

## Exact continuation

`experiments/android-init-cycle.json` now consumes the C05 fault boundary,
checkpoint SHA `dc5cd800e10b15fbde4d35242897fc15c478cb231724edfc102266142d13ecdf`.
It retains the existing seed, 6,285,406 sample cursor, cadence 64 and its
saved fractional phase. The workflow also pins the source report, source
EL and PC. It permits this particular pre-instruction EL0 state, never an
arbitrary later userspace checkpoint, and does not start the boot again.

## Validation

The native test creates actual missing TTBR0 mappings, runs real ARM64
fault delivery, an EL1 page-table store, barriers, ERET and NOP, then compares
the entire state against uninterrupted native execution. A second fixture
adds a pending timer IRQ before the same demand-paged entry. Tests also cover a
one-transition budget, restart inside the real fault handler, mismatched
fault addresses and fabricated faults over already mapped instructions.
The existing pixel, UART, rollback and journal checks remain included.

The genuine workflow is still required to establish the Android milestone;
fixtures alone do not do so. Its final gate requires the verified original
instruction. A bounded timeout or partial continuation still preserves its
checkpoint, but cannot produce a successful milestone job. After first entry, remaining work includes
userspace instruction/syscall coverage, normal mounts (including missing
userdata), measured RAM capacity, Android framework/composer, input,
networking, audio and the 3D path required by native games.
