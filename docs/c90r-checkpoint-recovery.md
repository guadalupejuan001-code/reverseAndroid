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
