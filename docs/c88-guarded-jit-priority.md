# C88: guarded priority adjustment for the actual runnable JIT

C88 selects the fully original-admitted C87 stage14 checkpoint at
123,147,727,998 instructions, SHA-256
`bfd5570b222a455343bd6ba7158cfaf857107250108219dc01967aaa6943e523`.
The complete pinned transport is in `state/experiments/c87-stage-14-artifacts.json`.
Later zero-input C87 states are reference evidence after this explicit selection.

The original Zygote5629 main thread has identical futex state and accounting in
C87 stages12,13,14: runtime26,794,720,000ns, voluntary switches19, involuntary8768.
The isolated, read-only full thread inspection of stage12 finds all six actual
threads with verified list links. Only JIT5922 is runnable; it is UID0, named
`Jit thread pool`, at nice19, and executing actual libart ClassLinker code.
The main thread is waiting in `art::ConditionVariable::WaitHoldingLocks`; the
other four threads sleep. No ptrace, pending signals or CPU bandwidth limit was
observed. This supports a measured priority trial; it does not establish a CPU
instruction defect. See `evidence/c87/stage12-to13-original-futex-and-starved-JIT.json`.

`state/experiments/c88-input-jit5922.txt` is an explicit596-byte authenticated
diagnostic-console event. It checks the live TID's name, TGID5629, UID0 and nice19
before invoking Android Toybox `renice -n -19 -p 5922`. Toybox's argument is a
relative increment, giving nice0. It reads `/proc` again and prints the success
marker only after verifying actual nice0. The same byte-exact guarded command
has produced a real nice0 reply in a separate experiment; that older branch is
not adopted here. Echoes, incomplete markers and repeated replies cannot pass.

The original seven input events and entire console prefix are authenticated and
preserved, never replayed into UART. Only C88 stage1 adds the new event, bound to
the selected source's SHA and instruction count. Every later stage supplies no
new input. The first successful reply requires an additional read-only kernel
inspection of the actual target's identity and nice0, or its natural exit after
the real success reply.

The selected native957f22e2 binary and both validated ISA extensions are unchanged.
All870 imported files, original epoch admission, whole snapshot roundtrip,
report/process binding, original BOOT and live kernel/property gates, peripherals,
SELinux, codecs, disk, logical2GiB RAM, seed, clock, cadence and IRQ remain in
force. There are no guest property or snapshot edits. Each bounded native run is
at most1B instructions with the original900000ms host cap; all complete transport
pieces must be preserved before the next stage or removal of an older duplicate.

The priority result alone is not Android boot. Completion requires genuine live
PID1 `sys.boot_completed=1`, actual SystemServer and fresh peripheral verification.
An authentic usable display and a meaningful real UI input response remain
separate requirements. No C88 execution or successful boot is claimed by this plan.
