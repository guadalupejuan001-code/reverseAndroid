# C05: continue the enumerated PCI state to the original /init

C04 run 40933488319 reached 4,187,300,000 retired instructions and genuinely
enumerated both 1af4:1042 block and 1af4:1050 GPU PCI endpoints, including BAR
resource assignment. Its checkpoint SHA-256 is
`1e06d2f22b6f34f0ec6a27f21e937b1a814267e2ac3b5634bd88cf4e3d5c7086`.
PID 1 had left the jitterentropy allocation path and was registering the
serial driver. It had not executed the original Android init.

The first C05 run (40938318558) continued that same counter stream to
5,406,300,000 instructions. Its last stack is the genuine X.509/RSA
certificate verification path (`mpi_powm` / `mpihelp_submul_1`). It reached
the host time bound with no unsupported operation and no init entry yet.
The next cycle pins that newer state and report, retaining all progress.

The next run (40940689989) reached 6,244,300,000 instructions, successfully
loaded the genuine X.509 certificate and registered fscrypt. Its final
trace contains repeatedly nested timer/softirq/preemption frames. The old
one-counter-tick-per-instruction cadence models a 1 MHz instruction stream
against a 1 MHz architected counter, making interrupt handling expensive
relative to timer deadlines.

An explicit `--counter-instructions-per-tick=64` upgrade now models 64 CPU
steps per counter tick, retaining the published 1 MHz CNTFRQ and monotonic
counter value. Every CPU instruction still executes. Seed and sample cursor
are preserved; a finite fractional phase is also saved. Idle timer jumps
clear that phase at the native deadline. Machine v7 / counter model v2
encode this cadence and phase; v1..v6 restore and historical v6 bytes remain
unchanged. The upgrade changes hardware timing explicitly, not guest kernel,
PC, RAM or disk. A later v7 continuation retains its exact phase.

Tests cover all 64 physical/virtual MRS samples, unchanged CNTFRQ, nonzero
phase restart, actual timer/IRQ/ERET execution with one-step chunks, PCI
pixels and rejection of invalid cadence/phase metadata. Android host
instrumentation adds native-hash and cold-restart checks for v7. The genuine
cycle now pins the 6.2443B input and its old cadence separately from the new
requested timing policy. Existing v6 APKs need the updated native engine to
load a v7 checkpoint.

The first paced run (40945076273) finished kernel initialization in another
6,477,180 steps and printed `Run /init as init process`. It stopped at EL0
PC 0x2a4000 before executing an opcode: the observer encountered the expected
lower-EL instruction translation fault 0x82000006 because Linux demand-pages
the ELF text. Its checkpoint SHA-256 is
`dc5cd800e10b15fbde4d35242897fc15c478cb231724edfc102266142d13ecdf`.

The entry controller now lets the same native CPU deliver an instruction
abort to the guest kernel, verifying ESR, FAR, ELR, exception count and the
single architectural transition before committing it. The guest's actual
handler must populate the mapping and return to EL0 before the original
bytes and instruction are verified. An unmasked pending IRQ is likewise
handled by the native CPU before checking an opcode. Neither event counts
as an executed init instruction. Forged fault observations, unrelated
faults and mismatched transitions remain rejected without caller mutation.
The report records entry faults and interrupts separately. Tests exercise
real EL1 table writes, ERET, an absent EL0 code mapping, a guest mapping
handler, pending timer acknowledgement and exact direct-versus-observed
full-state equality. The next source pin is the saved, still-unexecuted
entry; its v7 cadence and nonzero phase are retained exactly.

C05 resumes that exact state, with its original disk attachment, RAM and
persisted counter seed/cursor. It never draws a replacement seed. The source
checkpoint, complete source report and timing seed identity are independently
pinned in `experiments/android-init-cycle.json`.

The optional `--stop-after-init` policy commits only the single independently
verified original first instruction and saves the resulting EL0 boundary
immediately. Native verification still requires PC 0x2a4000, all 64 original
entry bytes and an actual one-instruction transition to 0x2a4004. A stopped
fixture exercises real ERET/EL0 execution; tests also prove that the verifier
is mandatory and its rejection cannot mutate the caller's state.

The host controller also distinguishes a WFI at a transaction boundary from
an actual blocked wait. If a chunk retired instructions and ended on WFI,
the next ordinary native run decides whether a scheduled timer can wake it.
Only a zero-instruction native wait returns control as a blocked device.
The former condition was reproduced with one-instruction chunks of the real
EL1/MMU/timer/IRQ/EL0/SVC fixture. The repaired full snapshot and UART match
uninterrupted execution; a WFI without a timer still blocks without mutation.

The bounded genuine cycle runs for at most ten minutes or two billion
instructions and preserves its last successful state on an architectural
stop. A completed time budget does not count as a successful init milestone.
Only `milestone.json.original_init_first_instruction_verified` attests the
actual native entry check; the read-only observer does not infer execution.

This boundary establishes one Android userspace instruction, not framework,
composer, filesystem-mount or game readiness. The C04 fstab audit's missing
userdata requirement and the memory, input, audio, network and 3D milestones
remain separate work. A later automated continuation beyond this EL0 stop
must consume its hash-bound first-entry proof; the pre-entry CLI deliberately
rejects treating an arbitrary later userspace PC as a fresh init entry.
