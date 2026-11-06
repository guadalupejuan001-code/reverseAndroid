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

Sources:

- [Linux CFS group scheduling](https://www.kernel.org/doc/html/latest/scheduler/sched-design-CFS.html#group-scheduler-extensions-to-cfs)
- [AOSP CPU controller mount configuration](https://android.googlesource.com/platform/system/core/+/master/libprocessgroup/profiles/cgroups.json)
- [Android cgroup abstraction](https://source.android.com/docs/core/perf/cgroups)
