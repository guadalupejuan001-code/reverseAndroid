# C87: bounded continuation of the original Android boot

C87 selects the latest fully admitted C86 stage 8, at 109,147,727,998
instructions, SHA-256
`cabadb046fc8f352bdf82d1c8cd1bb07baf66c78bc959a28b28d2968556da7c6`.
The complete original snapshot and entire console independently match local
execution and CI. All four original transport pieces have been recovered,
their archive and inner-piece checks authenticated, and the recovered whole
snapshot passed the original epoch preflight.

All seven inherited UART events are complete. Actual original OEM and sensor
services register their own Binder interfaces. Both original OEM read-only
getters returned successful Parcels with boolean true, consistent with the
validated peripheral's initial state byte 3. The fresh source-8 kernel probe
also verifies the actual peer, stock executables and three distinct reciprocal
PTY connections. The original policy hash remains unchanged and SELinux is
enforcing. SystemServer, completed boot, usable UI and an actual visible UI
response are still pending at this parent.

The new Zygote is the original init's PID 5629. Its fresh read-only thread
inspection finds one runnable thread, no pending fatal signal or ptrace, and
Android's own nice -20 priority. The actual stack is inside
`JNI_CreateJavaVM` and `art::Runtime::Create` in the original libart.so.
No further priority command is justified or planned by this continuation.

Recent one-billion-instruction native runs took 488,743 ms in CI and
532,105 ms locally. The old continuation's two-billion-instruction budget
could exceed the unchanged 900,000 ms host wall limit at that observed rate.
C87 therefore retains one-billion-instruction boundaries, including its first
step. This changes the host work partition. The guest clock, 64-instruction
cadence, timing seed, IRQ checks, RAM, media and device profile remain unchanged.

The executable is exactly the independently validated CLZ/IC IVAU binary
`957f22e2c8c8d3591029529855c733a05fc5c468e9a6e881ff5238bea6ff102f`.
There is no new ISA change or guest input. The C87 runner always passes the
literal no-input argument to the original epoch controller and checks zero
new UART bytes after every native run. It preserves every inherited event,
including the actual historical failed C83 activation. Later C86 outputs are
reference evidence after this explicit selection; the selected stage 8
remains the authenticated parent.

The source C86 proof is copied byte for byte, and the complete console is
reconstructed from the original C83 console, C84 stages 1–4, C85 stages 1–3,
and C86 stages 1–8. Bootstrap refuses a waiting or incomplete hardware phase,
changed ledger, native, snapshot, console, policy, unadmitted process outcome,
or a future budget above one billion. New regression checks cover those
guards. Every subsequent stage retains full original admission, fresh live
peripheral checks, complete checkpoint preservation and the workflow's
terminal/preservation barriers.

Original live PID 1 boot completion and live SystemServer remain mandatory.
A valid native boundary, completed peripheral adoption, or logo is not a
completed usable Android boot. The authentic before/after UI test remains a
separate final requirement.
