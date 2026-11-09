# C90R: recovered C90 stage 6 with validated host transport

C90R starts from the original C90 stage 6 at **161147727998 instructions**, SHA-256
`c8a2aa15bca412139973343813570a07d8f11d9fb7318a884cd64d45025f5714`.
The original source and every previous valid checkpoint remain preserved.

## Repairs and actual validation

The recovery script now creates missing parent directories while refusing to
reuse an existing piece directory. Archive, piece and complete RVG hashes remain
mandatory. Five offline regressions cover exact restoration, damaged data and
existing destinations.

The host file capacity is explicitly 3840 MiB. The first real trial exposed a
second host problem: Node rejected one SHA-256 update above 2 GiB. The explicit
host preload now processes every byte in zero-copy blocks of at most 64 MiB.
Actual Buffer and offset DataView inputs above 2 GiB match an independently
constructed incremental SHA-256. Original observer and codec files are unchanged.

[Validation run 42584322702](https://github.com/guadalupejuan001-code/reverseAndroid/actions/runs/42584322702)
passed at commit `104fa30f6d0aa59a05b9c346b8d339e7696f828f`:

- The original and candidate execute 50 million instructions from the same
  authenticated control. Every byte of the 2013255318-byte RVG matches, along
  with the entire console, original C46 result and complete kernel observation.
- The candidate executes 300 million instructions from the original C90 stage 6.
  Its **2148197611-byte** checkpoint exceeds the former 2 GiB host limit and
  passes the original roundtrip and admission checks, fresh live peripherals,
  SELinux and the authenticated complete console/input history.
- The admitted trial reaches **161447727998 instructions**, SHA-256
  `17b34e1843e9acab0613687780a42a881b427bbf037a8b7f406740c520809148`.
  All four transport pieces are preserved and pinned. The trial remains
  validation evidence, separate from the canonical continuation.

The downloaded native, validation and metadata archives were checked by complete
SHA-256 and ZIP CRC. The full trial RVG was not separately downloaded or replayed
locally. Its original admission and preservation were performed in the cited CI
run. [Actual report](evidence/c90r/validated-host-transport.json),
[selection evidence](evidence/c90r/host-profile-selection.json), and
[validated host profile](evidence/c90r/validated-host-profile-manifest.json).

## Explicit continuation

The [C90R recipe](../state/experiments/c90r-plan.json) pins the validated native,
frame exporter, report and tool hashes. Regenerated manifests must contain the
same complete data; JSON key order may differ. Every derived tool's actual bytes
must still match its recorded SHA-256. Both the original and extended preflight
must admit the exact C90 stage 6 state and produce equal results.

C90R replays **300 million instructions first** and requires the complete RVG
SHA-256, size and instruction count to reproduce the validated trial. Later
stages use one billion instructions each. Every successful stage is preserved
in full before proceeding or removing a local duplicate. The eight historical
UART events and all console bytes remain authenticated; no new UART is supplied.
The initial native change and each subsequent parent are recorded explicitly.

SystemServer, genuine `sys.boot_completed=1`, enforcing policy, live peripherals,
visible UI and meaningful gesture response remain separate observations.
The validation above does not establish a completed Android boot or usable UI.

The initial recipe is immutable evidence of this recovery. For a future restart,
read `state/latest.json` and `state/continuation.json` and select the newest fully
admitted checkpoint, preserving the validated host profile and full input history.

## First resumed and preserved boundary

[C90R run 42587296800](https://github.com/guadalupejuan001-code/reverseAndroid/actions/runs/42587296800),
at commit `ceb722e62ebc720fa33133761a188199e91a59a5`, recovered the original source,
passed both source admissions, and completed stage 1 with exit code zero. The
first complete RVG has SHA-256
`17b34e1843e9acab0613687780a42a881b427bbf037a8b7f406740c520809148`,
**161447727998 instructions** and **2148197611 bytes**. The controller verified
the full hash, size and instruction count against the independent validation
trial. The entire original kernel-observation bytes and accumulated console
also match that trial.

All four stage 1 transport pieces and its metadata were uploaded successfully
before stage 2 began. The canonical pointer now selects this C90R result. Its
producer, proof and transport are distinct from the earlier experimental trial,
which remains validation evidence. The
[stage 1 authority](evidence/c90r/stage1-original-CI-authority.json) records the
actual artifact IDs and ZIP hashes, authenticated report/process bindings,
source lineage, SystemServer observation and original boot result. The metadata
ZIP was downloaded and checked locally by full SHA-256 and CRC; no full local
RVG download or local native replay is claimed.

The original SystemServer is alive at PID 7354 with Zygote parent 5629. The fresh
peripheral checks and enforcing SELinux pass. The live `sys.boot_completed=1`
condition, usable UI and gesture response remain unverified. The independent
[original-engine validation run 42587296844](https://github.com/guadalupejuan001-code/reverseAndroid/actions/runs/42587296844)
also completed successfully.

## Recovering the newly preserved source

The [stage 1 artifact lock](../state/experiments/c90r-stage-1-artifacts.json)
selects its own four transport pieces and metadata. The initial `c90r-plan.json`
and `c90r-artifacts.json` remain unchanged: the current workflow deliberately
starts from original C90 stage 6. Dispatching it again does not automatically
resume the later canonical pointer. A subsequent campaign must explicitly bind
the latest admitted source and its history in a new recipe.

For stage 1, the full accumulated console was reconstructed from the authenticated
original C90 stage 6 console and C90R stage 1's exact `console-new.txt` bytes. Its
**3946878 bytes** have SHA-256
`359b2eac45e66d532bb8a53317b45f1dae77cbf5a952e4ff172c6b3dd2c91486`,
matching the actual `zero-input-proof.json`. The compressed console, all encoding
hashes and the proof hash are preserved in the stage lock and current pointer.
The eight historical UART events remain unchanged and must not be replayed.

Recovery of a source above 2 GiB must authenticate the retained `host-native`
and `host-validation` artifacts, regenerate the same derived tool profile, and
verify every tool's bytes against the validated manifest. Use the derived
`build/host-transport-runtime/tools/reverseandroid-epoch.mjs preflight` for that
source together with the selected native and frame exporter. The historical
preflight retains its original 2 GiB limit. The logical guest RAM remains 2 GiB;
the 3840 MiB setting applies to host snapshot file capacity.

## Latest preserved boundary and observer failure

C90R subsequently admitted and completely preserved stages 2, 3, 4 and 5,
reaching **165447727998 instructions** at stage 5. Each has a separate transport
lock, original proof and authenticated cumulative console in `state/experiments`.
The [stage 5 authority](evidence/c90r/stage5-original-CI-authority.json) is the
latest admitted boundary of this campaign.

Stage 6's native process completed, but its kernel observer failed while reading
the memory section. The full original admission did not finish, so the stage 6
tail is diagnostic only. The original error and host reproduction are retained
in [stage 6 failure evidence](evidence/c90r/stage6-host-observer-failure.json).

[C91](c91-host-observer-recovery.md) selects the preserved stage 5 as its source
and corrects only the derived host delimiter reader. The C90R initial recipe,
validated native and all prior admitted checkpoints remain preserved.
