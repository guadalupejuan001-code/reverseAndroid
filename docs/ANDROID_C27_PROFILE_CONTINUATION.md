# Authenticated continuation of the C27 board

The C27 machine reached 2,833,048,839 instructions from a fresh CPU, observing
the original init entry, second-stage init and enforcing SELinux. Its completed
clean checkpoint is the root of this continuation protocol:
`9db40b80214344583f855ccc777eb74963ece41ca5a1afab8de0737507f0e939`.
The root's four file hashes and source commit are fixed in
`tools/android_c28_contract.json`.

`android_c27_profile_resume.mjs` accepts only that exact completed root, its
specific C28 continuation from commit
`8dcb7159d22497b46b736227f4447df36c40bb23`, or subsequent completed clean
budgets carrying the same independently checked board and root. A contract
binds the snapshot, report, result, host outcome and producing source commit.
Running observations, unsupported instructions, forced host stops, terminal
guests, zero progress and inconsistent source evidence are rejected.

The binary audit checks the kernel, initrd, immutable image, disk descriptor,
CPU registers, RAM, original entropy seed, cadence and actual saved timer
phase. The native loader's compiled board/root pins are checked before it opens
the disk. It rehashes the whole external disk and requires a byte-identical
snapshot round trip before executing. The default host supervisor permits
three fifteen-minute budgets in one CPU instance. It preserves bounded final
evidence and does not treat a host timeout as normal guest completion.

`android_c27_sequence.mjs` adds exact saved scanout export and a read-only
kernel/property observation after each completed invocation. It creates a
new four-hash contract only after the output again passes the source policy.
The next invocation restores that exact state; it does not start a new CPU or
reuse a failed state as healthy. Immutable intermediate observations remain
explicitly ineligible as resume sources.

## Runtime proof

The optional `--live-properties` observer derives task lifecycle and SELinux
field offsets and widths from the byte-pinned kernel's BTF, and addresses from
its kallsyms. It reports task `exit_state` and whether `mm` is present, and reads
the actual `selinux_state.initialized`, `enforcing` and policy pointer. Missing
or unreadable fields do not become enforcement proof. The default observer
output remains byte-identical to the archived real C23 observation.

`android_framework_evidence.mjs` binds the native report, host outcome, kernel
observation and frame to one snapshot. Framework readiness requires all of:

- Clean completed native progress and no detected guest failure or reboot.
- Live initialized enforcing SELinux with a policy present.
- Live userspace leaders for PID 1, `zygote64`, `system_server` and
  `surfaceflinger`, excluding exited tasks and kernel threads.
- The exact stable value `sys.boot_completed=1`, read through the verified
  live PID 1 shared property mapping and Bionic property trie.

An exported frame additionally binds its PC, instruction count, frame number,
dimensions, complete PPM bytes and RGB digest to the same saved boundary. It
executes no CPU instructions. The runtime verifier requests visual review; it
never asserts that unreviewed pixels show a usable Android screen.

## Validation before the next long continuation

- 62 source-policy tests cover completed roots and children, compiled pins,
  mismatched file contracts, running/ISA/terminal sources, altered RAM/clock,
  lost provenance, late mount failures and zero progress.
- The reusable native loader executed 150,000 instructions from the actual C27
  root. Its complete final snapshot digest was
  `bd0e1094726e20b743f051a399b46422c67037adf2eb26730385d70dacad56ce`,
  exactly matching the independently run specific C28 loader at the same
  boundary. Neither run modified the original source snapshot.
- Actual completed local C28 and generic child evidence passed the reusable
  preflight. The generic test was a local precommit integration, not a claim
  of a committed production boot.
- Read-only checks of the actual C27 final and C28 fifteen-minute snapshots
  found initialized enforcing SELinux and a policy present. Neither had a
  live boot-completed property or the required framework processes yet.
- Policy fixtures exercise stale, mixed, dirty, zombie and tampered evidence;
  they are explicitly not Android boot or screen results.

Kernel, initrd/CPIO, boot arguments, AVB, SELinux configuration, CPU capability
registers, XI/Ether and Mundo X remain unchanged. Production compilation
remains O2. A complete Android boot and usable screen remain to be observed.
