# QSSD C03: compatibility review for REVERSE Android

This review evaluates the new complete-state bus, not Velocity 3.8's earlier
geometry observation cache. It does not change a guest checkpoint, its codecs,
the selected native executable, or the active C88 continuation.

## Sources and execution

- Virtual-qSSD-: `c9dc0ebf3370a4468e14df92fcebb2a90954c448`, branch
  `feature/c03-complete-state-bus-20261106`.
- Emulator source reviewed: `6b586622b7ddd9655c932cfc987a45b78550d190`.
- Original QSSD cores: Mundo X `e2778d962af22f6f1a0794058dd4685fc2573966`,
  REVERSE-PC `08c363e6c6855c14026c86046af7183a45f45a98`, and GPU-VIRTUAL
  `6f9a8affa0b1cee4028f8145922fd6dbdbefbefb`.
- Native tests and exact results are recorded in
  [qssd-c03-native-review.json](evidence/qssd-c03-native-review.json).

The five native test entry points in the pinned CMake project were built and
run locally. CMake/CTest was unavailable in that environment, so the same C++
sources and CLI test were invoked directly with their declared dependencies.
No new implementation or substitute QSSD core was used.

## What C03 can contribute

`StateBus<State>` publishes a complete immutable component graph. CPU and GPU
clients can retain the same revision and shared resource handles; a stale writer
can be rejected and a retained old graphics job can keep its actual CPU/resource
dependencies. A fork can compare a candidate against a preserved original state.
The concrete state graph is supplied by its consumer: the template does not
automatically discover or capture the emulator.

This is useful infrastructure for a future CPU/GPU adapter. Publishing shared
handles is different from copying or decoding their payload. Retaining old
handles also retains their memory; the history length is not a RAM limit.

## What it does not currently supply

QSSD C03 has no Android virtio/NVMe driver or bridge from Android's graphics
commands to GPU-VIRTUAL/REgraphics. It does not create missing virtio resource
IDs, repair the device's resource lifecycle, change Linux scheduling, or release
an ART futex by itself.

Its `StateArchive` QSB3 transport has a 64 MiB aggregate encoded limit and 512
sections. That archive cannot be substituted for a larger original RVG Android
checkpoint. The in-memory bus is a separate API and can retain shared handles
without using that archive. Any future adapter must preserve the complete
original RVG bytes and bound disk, all CPU/memory/device state, and the original
admission checks. It must not replace them with a metadata-only manifest labelled
as a complete Android state.

## Observed Android problem

The authenticated original console through C87 stage 14 contains repeated
virtio-gpu `0x1203` responses to transfer, set-scanout and flush commands. The
reviewed device code returns this response when its resource lookup fails.
These are log occurrences, including duplicate records, not a count of distinct
failed commands. There are no `0x1201`/`0x1205` response records in this transcript;
resource-slot exhaustion is therefore not established by this evidence.

C87 stage 16 completed its native instruction budget at 125,147,727,998
instructions, preserving checkpoint
`365c4a45f0988a442441da1765925ec545db17b6ea674245ba3737af840ba666`.
Its final proof has SystemServer absent, BOOT false and no verified UI response.
The original kernel observer finds Zygote 5629 in `futex_wait`. That observation
does not identify the futex owner or establish that the graphics errors caused
the wait. QSSD test results do not establish Android boot or UI success.

## Decision

Keep QSSD C03 as a validated candidate for the future state-sharing adapter;
do not insert it into the running C88 boot experiment solely to address these
errors. The current emulator already preserves a complete authenticated RVG
state. The evidence does not show a storage/coherence failure that C03 would
repair, and no emulator-specific C03 adapter has been implemented or benchmarked
by this review.

Before adopting such an adapter, compare the same original admitted source with
and without it: full RVG/disk/console identity, CPU/GPU resource lifecycle,
continuation, peak resident memory, serialization and restore time, and the
actual boot/UI gates. This review preserves C87 and C88 and makes no claim of
an emulator speedup, Android GPU acceleration or successful boot.
