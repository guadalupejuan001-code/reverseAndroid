# Live processes in normal Linux sleep states

The genuine C29 round-two observation contains a live `keystore2` leader with
PID 396, `__state = 8193 (0x2001)`, `exit_state = 0` and a non-null userspace
`mm`. The original readiness policy accepted only numeric states 0, 1 and 2,
which would incorrectly reject a required framework leader in this same normal
sleep state.

Linux [v6.12 sched.h](https://github.com/torvalds/linux/blob/v6.12/include/linux/sched.h)
defines `TASK_INTERRUPTIBLE = 1`, `TASK_FREEZABLE = 0x2000` and
`TASK_WAKEKILL = 0x100`. Scheduling state and exit state are separate fields.
The freezable flag marks a sleep that can participate in freezing; it does not
mean the task is already frozen. `TASK_FROZEN` is a different flag, `0x8000`.

The read-only verifier now accepts running userspace tasks and ordinary
interruptible/uninterruptible sleeps with the documented freezable or wake-kill
modifiers. It still requires a positive PID, zero exit state and a userspace
`mm`, and rejects stopped, traced, parked, dead, frozen, malformed and unknown
states. All checkpoint bindings, live SELinux checks, PID 1 property mapping
verification and pixel hashes remain required for full runtime evidence.

Measured source: run `41151900393`, artifact `13025347999`, snapshot SHA-256
`5350fce882e3eae872fc3ecc1a27d86498085fd88ed400b19c763ff5ee39e2f1`.
Its current task is the actual `mke2fs` process executing the kernel's
`crypto_aes_encrypt` while preparing encrypted userdata. It has not completed
the Android framework or emitted a scanout. This policy correction makes no
boot claim about that observation.

Tests cover the documented ordinary sleep combinations and reject zombie,
kernel-thread, stopped/frozen, invalid PID and malformed-state evidence. The
actual saved `keystore2` observation passes the corrected liveness predicate.
No CPU instructions execute during this check and no guest state changes.
