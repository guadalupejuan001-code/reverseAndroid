# C92 — reverseAndroid authenticated boot-recovery experiment

**Last observed fully preserved C91 source:** stage9, 174447727998 instructions,
checkpoint SHA-256 `1b237bc819d2aff0db609207922538d0c4a38540e3438875082ed10bbdefb2d3`.
The branch does not promote it from the stale `state/latest.json` pointer.
The actual source selector verifies metadata, original process evidence, four
transport pieces and original console at workflow runtime.

## Root cause of prior C91F1 failure

Run 42609369278 executed 1B further instructions, but its derived read-only
kernel observer stopped with `task-list cycle or limit` at the historical
512-task ceiling. Thus it did NOT finish original checkpoint admission.
Its 255-byte console command was only partially consumed; no execution ACK
was observed, so it is diagnostic evidence, not proof of corrected priorities.

## This experiment

- Original guest/native disk, 2 GiB RAM, ISA, transport, clock, SELinux,
  checkpoints, prior 8 UART events and imported 870 files remain unchanged.
- Derive from the authenticated host observer. Replace *only* its task-list
  traversal bound: reject real cycles separately; stop at 4096 tasks.
  Authenticated source tool hashes and changed-file list are enforced.
- Inject one explicitly documented 71-byte UART command:
  `su 0 renice -n 20 -p $(pidof cppreopts.sh fsync cp mv preopt2cachenam)`.
- Source gate verifies candidate processes root-owned and nice -20 before
  command. Do not infer success from accepted UART bytes: measure actual
  normalized kernel priorities, SystemServer runtime, SELinux and full native
  admission. The command intentionally has no synthetic success ACK.
- Only after original checkpoint verified and preserved, and genuine
  SystemServer progress proven, continue one zero-input bounded stage.
- Never inject Android properties or fake framebuffer/Launcher.
- No automatic commit or merge to main, no APK.

The suspected cause of blocked Android boot is hundreds of root-owned
`cppreopts` and `fsync` processes at nice -20 starving runnable
SystemServer on a single emulated vCPU. This is an experimentally grounded
hypothesis, not yet proven to resolve Android boot. Acceptance requires
live PID1 `sys.boot_completed=1` and usable Android UI/real input.
