# Android C30 — diagnose the authentic C29 final checkpoint

Scope: host-only code and tests. No guest CPU is executed, no Android ABI or XI/Ether code
is rewritten, and no existing checkpoint is edited. This is a diagnostic gate, not an
Android APK or an additional successful boot.

## Exact source

GitHub Actions run [41151900393](https://github.com/guadalupejuan001-code/REVERSE-PC/actions/runs/41151900393)
completed four consecutive **clean budget** continuations of the original C27 machine.
The C29 fourth boundary reached **13,615,798,839** guest instructions and SHA-256
`454e29a90cc1af34df95b2c60f1fc90d29927096c72be962e95b2e9fca16598d`,
with producer commit `16d670048a2072862dbdb732c21444c9dc35575d`.
Source artifact: `reverse-c29-budget-4` (artifact ID `13029661038`).
The checkpoints after C29 rounds 1–4 recorded instruction counts
7,294,248,839; 9,654,948,839; 11,888,198,839; 13,615,798,839.

These values demonstrate CPU execution, **not** completion of an Android filesystem,
framework, display, or APK execution. The earlier C29 round-two observation found
a live `mke2fs` task in the kernel AES path, but that does not establish the exact
position or duration of the final C29 round-four task.

## Checks

1. Reauthenticate the source C29 `output-contract.json`, all four original files,
   exact source commit and final checkpoint SHA against a pinned value, and reuse the
   pre-existing independent C27 CPU/RAM/clock/disk/original-init audit.
2. Reject a false clean completion, changed native outcome, contradictory checkpoint
   instruction count, missing observer half, or cross-boundary observer evidence.
3. Recompute read-only framework policy using the actual kernel observer, stored
   scanout metadata and unmodified PPM when available. An empty scanout cannot become
   an Android screen merely because GitHub Actions succeeded.
4. Identify the currently executing process **only if** the observer bound it to a
   verified process or thread entry. Distinguish formatter observed, formatter merely
   present, other userspace, and insufficient evidence. Never equate instruction
   progress with storage progress.

CI only uploads the derived JSON and host test results. It does not republish the
large original Android userdata disk or the checkpoint.

## Next engineering gate

If the final observation still shows `mke2fs`, compare clean checkpoint-to-checkpoint
guest progress and virtual userdata write deltas before allocating more long CPU budgets.
If the formatter has exited, inspect the next userspace init action and live
`sys.boot_completed`, `zygote64`, `system_server`, `surfaceflinger`, and exact scanout.
Neither conclusion may be assumed without a new verified observation.

Run locally with the authentic GitHub artifact already downloaded:

```sh
node --test tests/android_c30_checkpoint.test.mjs
node tools/android_c30_checkpoint.mjs /path/to/reverse-c29-budget-4
```

The output reports `android_display_content_verified: false` by design; an actual
scanout requires visual review after independent runtime validation.
