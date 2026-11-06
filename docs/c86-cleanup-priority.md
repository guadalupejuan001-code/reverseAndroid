# C86: finish original Zygote cleanup through a guarded ordinary OS input

The latest selected and fully admitted C85 boundary is stage 3, at
101,647,727,998 instructions, SHA-256
`f92de63b69e8921eab0ac5dd7a436b859ef9cbd0f70287d85a2a8e6e8a7729b0`.
Its complete four-piece transport was independently recovered, every outer
archive and inner piece authenticated, and the whole original RVG passed the
original epoch preflight. The activation acknowledgement is actually complete:
phase 5, waiting false, five inherited native UART events. Boot and live
SystemServer remain false. SELinux remains enforcing.

A fresh read-only original-BTF thread inspection proves that PID 562 is the
dead Zygote leader, exit state 32, exit code 9, SIGCHLD parent PID 1, with no
user mm. Its reciprocal signal thread list has exactly two members. The only
remaining live member is root TID 3387, `Jit thread pool`, normal policy,
static priority 139 (nice +19), runnable on the run queue. Its real stack is
inside `free_unref_folios`, `unmap_vmas`, `exit_mmap`, `do_exit`,
`do_group_exit`, and `get_signal`. This cleanup must finish before the original
init can reap the group leader and recover its service. The worker has made
only limited progress: its accounted execution was 69,084,000 ns at C85 stage
1 and 92,776,000 ns at stage 3.

C86 explicitly selects that latest admitted, console-ready stage 3 as its
parent. Later outputs of the old C85 recipe are reference evidence after this
selection. They must not replace C86 or silently introduce its old inspection
event. Every old event and the actual failed C83 activation remain preserved.
The original activation and received peripheral binary are not repeated.

The new phase 5 is one authenticated 1,000-byte ordinary Android shell input.
It checks `/proc/3387` directly: exact command name, TGID 562, root UID,
`PF_EXITING`, and nice 19. Only then may Toybox execute
`renice -n -19 -p 3387`. Toybox `-n` is relative: adding zero would not restore
normal priority. The command verifies nice 0, or records actual disappearance
after successful renice. Already absent or already normal tasks have distinct
skip acknowledgements. Foreign, non-exiting, or unexpectedly prioritized tasks
fail closed. An outer `REVERSE_PTS_C86_CLEANUP RC_0` is required before the
next command may be sent.

The unchanged 932-byte original HAL inspection moves to phase 6 and runs once
after that acknowledgement. The seven-phase controller retains every original
source, kernel, process, snapshot roundtrip, peer/PTY, Binder, policy and boot
completion check. It emits no further UART after these two pending phases.
Unit checks cover the actual thread fixture, stale or foreign state rejection,
relative Toybox arguments, no-action guards, natural task exit, and every
terminal workflow path. Source preparation preserves the original C85 proof
byte for byte and reconstructs the entire authenticated console from C83,
C84 stages 1–4, and C85 stages 1–3.

The native executable remains the independently validated IC IVAU/CLZ binary
`957f22e2c8c8d3591029529855c733a05fc5c468e9a6e881ff5238bea6ff102f`.
All 870 original files, RVG/MXIM/RFP decoders, XI/Ether authority, zero
background, original disk, 2 GiB RAM, seed, 64-instruction cadence, IRQ,
properties, and enforcing policy remain unchanged. No guest snapshot is edited.
Preparation or input acknowledgement alone does not prove Android boot. The
original live PID 1 boot property, live SystemServer, authentic usable frame,
and an actual visible UI response still require observation.
