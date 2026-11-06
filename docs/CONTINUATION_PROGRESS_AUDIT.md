# Inspect the actual Android continuation without restarting the guest

`scripts/audit-android-continuation.py` consumes existing original producer
reports. It distinguishes a runnable or waiting Zygote, per-task CPU progress,
a replaced Zygote identity, a candidate SystemServer process, original boot
acceptance and the separate usable-UI requirement. It never executes the
guest, sends UART, changes scheduling or properties, or promotes a state.

The audit binds the original report, result and native-process hashes to a
pinned checkpoint and instruction count. It requires the original admitted
boundary, unchanged epoch, pinned executable, original 2 GiB RAM and
64-instruction cadence, enforcing SELinux, the inherited event ledger, and
zero new UART. A process name alone cannot establish SystemServer identity.
A failed or forcibly killed native run is rejected even if the metadata's
hashes are internally consistent.

## Reproduce on the current original results

Prepare one JSON object with `expected_epoch_sha256`,
`expected_native_sha256`, `proof_file`, `expected_events`, and `boundaries`.
Use the pinned values from the selected campaign's immutable plan. Each
boundary contains its local `directory`, independently selected
`checkpoint_sha256`, and `instructions`. Supply boundaries in order.

For C87 the proof is `c87-proof.json`; the native SHA-256 is
`957f22e2c8c8d3591029529855c733a05fc5c468e9a6e881ff5238bea6ff102f`;
the epoch SHA-256 is
`bc5e6f91cc5befb13ba8f8e96ffb74cad22edda25586324a0f469e1b26fd5e75`.
Copy the exact seven `inherited_source_events` from
`state/experiments/c87-plan.json` into `expected_events`; do not synthesize
or replay them.

```sh
python3 scripts/audit-android-continuation.py pins.json new-audit.json --verify-latest-snapshot
python3 tests/android_continuation_progress_test.py
```

The whole snapshot is hashed as a stream. Archived parents whose local RVG
has already been released can still contribute their authenticated producer
metadata. The result explicitly reports that their complete snapshot was
not hashed locally. `--verify-snapshots` instead requires every complete
local snapshot. Metadata inputs have a 16 MiB size bound; a single audit
accepts at most 32 boundaries and refuses to replace a prior output.

The C87 stage-2-to-stage-3 observation shows the same actual Zygote PID 5629
advancing by 1,569,959,000 ns in user mode and 177,220,000 ns in kernel mode.
Its original gate still reports no current SystemServer or completed boot.
This is actual task progress, not a claim based only on another billion
global instructions. A new Zygote or recycled PID is never compared against
the previous task's CPU counters.

No new original CPU or decoder implementation is introduced. The existing
epoch preflight and the original boot and UI checks retain authority.
