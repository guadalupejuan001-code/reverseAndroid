# Actual GPU allocation failure and explicit resource-capacity repair

The original virtio GPU has four resource slots. Its CREATE_2D handler returns
`0x1201` only when all four are occupied. The authenticated C80 stage 9 console
contains two failed CREATE_2D requests, followed by failed backing attachments.
The independent C79 stage 18 console contains the same failure. Later, C88
stage 1 retains resource IDs `[0,4,5,6]` while Android submits transfer, scanout
and flush requests for missing ID 8.

The bounded C88 observer passed in run 42279660027: both full native RVG outputs
have SHA-256 `f70c2eed518ac0f7b00bc974f1b18778167a87a6addb79e454c5e44f058986de`,
and all new console and frame bytes match. The absent ID 8 was already absent at
the source; the 50-million-instruction window contains no CREATE, UNREF or reset.
That trace alone cannot establish its earlier lifetime. The earlier console
does not print the failed IDs, so the historical replay must identify them.

## Device correction

`prepare-gpu-capacity-extension.py` creates a separate verified build copy with
16 resource slots. Slot 16 (zero-based) is the existing reserved scanout plane, so larger
tables would require a different address mapping and are deliberately refused.
The pixel backend, format conversion, DMA, interrupt logic and explicit flush
semantics remain the original XI/Ether implementation. Exhaustion at the new
declared limit remains a real `0x1201` response.

The aggregate backing metadata budget remains 8,192 entries, the original
maximum of four times 2,048. This keeps the original 196,608-byte PCI and
262,144-byte machine admission limits. The logical RAM remains 2 GiB. No imported
source file, guest disk, timing seed, 64-instruction cadence or guest input is
changed.

### Explicit checkpoint compatibility

The expanded decoder accepts the original PCI subformat 1, re-encoding it
byte-for-byte while the extra slots remain empty. Once extra slots contain
resources it writes explicit PCI subformat 2 with declared capacity 16. It
restores all 16 resource identities, backing vectors and scanout/cursor state.
The outer RVG, MXIM and RFP compact formats retain their original codecs.

**The original four-slot native cannot decode PCI subformat 2.** Its rejection
is tested. A candidate checkpoint must retain its device-profile manifest and
use the expanded native. It is not eligible for the primary four-slot
continuation merely because its epoch admission checks pass.

The native protocol fixture reproduces the exact observed original table:
create IDs1,4,5,6, fail7/8, retire1, then reject all requests for8. With the
expanded profile,7/8 are successfully created and8 receives the exact original
pixel and alpha bits, including after a whole-state restore. All 16 slots and
the scanout plane are checked for overlap; the 17th resource remains refused.
These are protocol fixtures, not Android screenshots or boot proof.

## QSSD C03 bridge

`extensions/gpu-state-bridge.hpp` uses the actual QSSD C03 StateBus at pinned
commit `c9dc0ebf3370a4468e14df92fcebb2a90954c448`. It retains complete immutable
native VM versions using the original copy-on-write state, rather than a
separate framebuffer authority or the 64 MiB archive format. CPU and GPU clients
share one version authority. A retained surface owns its original resource
version even after publication or UNREF; current readers cannot acquire a
retired ID. Stale writers and foreign VM epochs are rejected.

Publication requires a completed native boundary; callers must not mutate the
source VM concurrently with its capture. Pixel projection is explicit and
bounded to one original 32-bit word. Unresolved original quantum data requires
explicit observation and must not be silently measured. Surface observations
are synchronized within each retained root. Whole-state serialization is an
explicit diagnostic operation, not a per-pixel copy.

This is a tested in-process bridge API. It is not yet used by the Android
execution loop or connected to the GPU C10 renderer. No new GPU acceleration,
FPS improvement, running system_server or completed Android boot is claimed.

## Historical actual-Android validation

The independent workflow uses C79 stage 17 at 47 billion instructions, whole-state
SHA-256 `bfd5feee5264a2e43f21684a25b7ceb7b27222c94264f188983b9cff1f8ac801`,
as an explicitly historical diagnostic reference immediately before the
creation errors. It is not the current primary checkpoint. Both profiles resume
that exact state with zero new UART bytes and a 1 billion-instruction budget,
using the same CLZ/IC IVAU corrections. Every output must pass the unmodified
epoch, report binding, kernel, live-property and frame admission checks, with
the appropriate device decoder.

Both host traces identify the real failed CREATE IDs and compare them against
successful candidate CREATEs. Logs must be complete, the source must remain
unchanged, and each distinct admitted checkpoint is preserved in full.
Success requires actual original `0x1201` failures and their successful
candidate counterparts, with no candidate `0x1201` or `0x1203` in the measured
window. Boot and usable UI remain separate observations, never inferred from
an instruction-budget completion.

The current primary C88 run and both primary state pointers are untouched.
Increasing the capacity cannot recreate an ID that was already absent from
that primary state. A later recovery would need genuine guest resource
recreation or an explicitly selected new continuation, with its own validation.

## Reproduction

Check out Mundo X at `e2778d962af22f6f1a0794058dd4685fc2573966` in
`deps/mundo-x` and QSSD at the pinned C03 commit in `deps/qssd`, then run:

```sh
bash scripts/build-reverseandroid.sh
bash scripts/build-gpu-capacity-extension.sh
```

The generated capacity and control source directories must be fresh. Native
fixture reports, original PCI checks, original QSSD concurrency checks and
profile manifests are written under `build/gpu-capacity`. The independent
workflow retains the authenticated actual-Android comparison separately.
