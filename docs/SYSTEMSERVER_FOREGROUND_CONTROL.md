# Actual SystemServer foreground allocation

The completed original top-app stage8 is the latest admitted boundary
selected when this isolated trial was prepared:123,647,727,998 instructions,
SHA-256 `66b59d8fdd967c93800f072f15bb2b265050aeb231620d5819cff3591745f5fd`.
All four original transport ZIPs, their inner manifest and original C46
admission are pinned. The source is recovered and fully hashed in CI;
this current full RVG has not been independently reproduced locally.
Earlier trial stages1–4 have independent whole-state reproduction.

Actual SystemServer6512 remains alive with UID1000, authenticated
app_process64/argv, parent Zygote5629 and eleven threads. Android naturally
put it in `/foreground`, whose observed task-group pointer is
`0xffffff8002d9e840`, shares_raw1048576 (user1024), unlimited quota and
100ms period. Zygote remains in the previously tested `/top-app` group.
The original root-group weight is unchanged.

Between stage7 and stage8, SystemServer main's accounted scheduler runtime
increased176,242,000ns over one billion instructions. At stage8, main waits
in mprotect's mmap write semaphore while HeapTaskDaemon is runnable and
advancing in libart. This motivates a controlled allocation test for the
whole foreground group; it is not evidence of a kernel lock bug or a
measured completed-boot speedup. No forced lock release, snapshot patch,
property injection or clock change is performed.

The one source-bound guest request verifies actual SystemServer name,
TGID, UID1000, real cpu-controller membership in `/foreground`, original
foreground shares1024 and inherited top-app trial shares16384. It requests
foreground shares16384 through the guest's normal cgroup filesystem.
Fresh original-kernel probes must observe the same group pointers,
credentials, ancestry, quotas and root weight after every admitted run.
The current top-app run continues as the unchanged-input reference.

First budget:1B instructions for a matched comparison. Further stages use
at most2B each, with zero new input. Original native957f22, all870 imported
files, compact RVG/MXIM/RFP authority, logical2GiB RAM, seed,64-instruction
cadence, IRQ and original admission remain unchanged. Required real
peripheral and enforcing-policy evidence retain their authority.

After original completed boot, one additional source-bound guarded guest
request must restore both foreground and inherited top-app shares to1024.
Real replies and fresh original-kernel observations must verify both
restorations before this controller can complete. UI response stays false
and is evaluated by the separately prepared Settings validation.

Four meaningful local cases execute the exact boost and restore bodies
under sh and bash: valid membership/controller orderings, identity and
old-value failures before any write, restoration of both groups, and
echo/failure rejection. Native execution is required for an outcome.

Source process/group/accounting observations are retained in
[the stage8 evidence](evidence/topapp/systemserver-foreground-source8.json).
The unboosted JIT-only reference also independently reaches real
SystemServer by129,647,727,998 instructions, with boot still incomplete;
the original top-app weight change is an acceleration experiment, not a
claim that Android inherently requires a modified group weight.

Linux CPU group weight semantics:
[kernel scheduler documentation](https://www.kernel.org/doc/html/latest/scheduler/sched-design-CFS.html#group-scheduler-extensions-to-cfs).
