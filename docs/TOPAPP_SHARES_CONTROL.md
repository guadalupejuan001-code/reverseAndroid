# Isolated CPU group shares comparison

The JIT-only control completed its real guarded command at stage 3. Three
independent local replays match the corresponding complete original CI states,
including console, scanout bytes and kernel observations. Select that admitted
116,647,727,998-instruction state, SHA-256
`6134b364f97612b20233938e1aa22933a14ea2fcaf8ab7a340bf0c28d65e6567`.
Both C87 and the JIT-only continuation remain separate references.

A fresh original-kernel probe of the selected source must confirm the actual
Zygote PID 5629, JIT TID 5922 at nice 0, their CPU group ancestry and the
existing `/top-app` task-group pointer. The observed group has shares_raw
1048576 (Linux user-visible shares 1024), unlimited quota and a 100ms period.
In the prior 1.5-billion-instruction interval, the Zygote main's scheduler
runtime increased by 2,375,433,000ns, while the virtual counter advanced
24,437,829 ticks at the original 1MHz counter frequency. Surviving init and
service tasks also made substantial progress and the console records repeated
service restarts. These observations motivate testing group allocation;
they do not establish that changing shares completes Android boot.

Linux group scheduling allocates CPU among groups as well as among their
threads. Increasing a thread's priority affects its place inside its group.
The documented `cpu.shares` control changes the group's relative weight.
Android's default CPU-controller path is `/dev/cpuctl`; the real guest still
verifies CPU-controller membership and reads the actual file before writing.

The one source-bound input verifies thread name, TGID, UID, nice 0, actual
`cpu` membership in `/top-app` and existing shares 1024 before requesting
shares 16384 through the guest's normal cgroup filesystem. The input uses
shell builtins. A fresh read-only original-kernel probe must independently
observe the actual group weight; quotas, root-group shares, RAM, seed,
counter cadence, IRQ model, original CPU and all compact codecs remain
unchanged. The C46, live properties, enforcing policy and real peripheral
checks retain their authority.

The first two bounded runs retire 500 million instructions each, allowing
a comparison at the JIT-only stage4's exact global instruction count.
Further stages retire at most one billion instructions with zero new input.
If original completed boot and live peripheral checks pass, the controller
issues one additional source-bound guarded input to restore shares 1024.
The trial stays isolated until the normal weight is independently verified.
Usable UI response remains a separate requirement after these checks.

The exact inner boost and restore commands run under real local sh and bash
against temporary proc/cgroup fixtures. Identity, priority, group-membership
and previous-value failures leave the fixture unchanged. Command echoes
cannot satisfy the reply parser, and a failed reply overrides a success.

## Measured progress

The first two complete trial states reproduce byte for byte in independent
[CI](https://github.com/guadalupejuan001-code/reverseAndroid/actions/runs/42265960158)
and local original-engine execution. The kernel observations, fresh thread
probes, console bytes and scanout bytes also match. The original C46 and live
peripheral checks pass in both branches. At trial stage2, the original-kernel
probe independently observes shares_raw 16777216 in the same `/top-app`
task-group pointer, with root shares and group quota/period unchanged.

The table measures the same Zygote kernel task from the same admitted source
at identical retired-instruction counts. Values are its scheduler runtime
increments, in seconds.

| Instructions from common source | JIT-only reference | CPU group trial | Ratio |
| --- | ---: | ---: | ---: |
| 1 billion | 0.719811 | 2.215391 | 3.08 |
| 2 billion | 2.493726 | 6.074833 | 2.44 |

This is measured CPU allocation and task progress, not a measurement of
completed-boot or host wall-clock acceleration. The trial includes one
source-bound guest shares request; the JIT-only reference has zero new input.
The authenticated producer bindings, exact counters and original admission
are in [the 1B comparison](evidence/topapp/matched-1B-CPU-comparison.json)
and [the 2B comparison](evidence/topapp/matched-2B-CPU-comparison.json).

At trial stage3, Android emitted `BOOST_DONE SHARES_16384`; the final outer RC
was still pending. The JIT has advanced, the main and JIT currently wait on
separate futexes, and HeapTaskDaemon remains runnable. Original SystemServer,
completed boot and usable UI response remain unverified at that boundary.

Sources:

- [Linux CFS group scheduling](https://www.kernel.org/doc/html/latest/scheduler/sched-design-CFS.html#group-scheduler-extensions-to-cfs)
- [AOSP CPU controller mount configuration](https://android.googlesource.com/platform/system/core/+/master/libprocessgroup/profiles/cgroups.json)
- [Android cgroup abstraction](https://source.android.com/docs/core/perf/cgroups)

## Actual SystemServer reached

CI stage6 admits complete state121,647,727,998, SHA-256
`572a9be831d34e944d9b46b870615bc001a6957777499c5fc7de4553feefa18e`.
The original C46 gate verifies actual `system_server`: PID6512, UID/EUID1000,
parent Zygote5629, `/system/bin/app_process64`, authenticated argv
`["system_server"]`, ten live threads and no exit state. SELinux remains
enforcing, original RAM/codecs/counter/IRQ are unchanged and all original
required peripheral links pass.

The same SystemServer survives the next billion instructions at stage7,
with eleven threads and independently advancing CPU accounting. The main
thread is runnable in linker64/munmap; its new system-server-init worker is
runnable in boot-core-libart.oat. These observations show actual runtime
progress; they do not establish completed service initialization. Android
has naturally moved the process to `/foreground`, with its original1024
shares, while Zygote's tested `/top-app` group remains at16384.

Live PID1 `sys.boot_completed=1`, normal group-weight restoration and usable
UI response remain unverified. The full producer bindings, original-file
hashes, actual process observations and security observations are retained
in [the SystemServer milestone](evidence/topapp/actual-systemserver-stage6-and7.json).

The local stage5 attempt ended without a process outcome or original
admission. It is quarantined, never selected as a source. The host cgroup
has an8GiB limit and recorded OOM kills, but per-process attribution is
unavailable; Android guest failure has not been established. Independent
CI reproduces the first four entire local states and advances through
original admitted stages5–7. Further native execution uses the independent
runner to avoid memory contention on the shared host.
