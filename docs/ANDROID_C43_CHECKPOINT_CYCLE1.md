# C43, cycle 1: original-format checkpoint streaming

Android C42 stores a 971,694,233-byte RVG checkpoint. Its original serializer
constructs the complete persistent memory text, then copies that text and the
immutable image into another complete RVG buffer. Saving or hashing that RVG
therefore materializes additional large buffers.

This cycle preserves the exact RVG1/RVG2/RVG3 container, MXIM2 revision envelope,
MXIM3 page bitmap/value hexadecimal spelling, address ordering, large addresses,
quantum bindings, CPU/device/FP bytes, and final checksum. `snapshot()` and the
original decoder remain unchanged and serve as the independent compatibility
reference. No binary replacement format is introduced.

The new `snapshot_to` counts the existing memory representation and emits it in
chunks of at most 65,536 bytes, with at most one page's temporary hexadecimal
text. The immutable image is streamed as well. `snapshot_sha256` hashes these
same bytes without constructing a complete RVG. Bounded legacy memory retains
its original serializer; its payload is small and governed by existing limits.

The Linux boot journal writes and hashes the stream inside its existing pending
generation, then publishes the original files and atomic generation pointer.
Android's file-operation bridge stages a separate candidate, syncs it, and
renames it only after the entire RVG is complete. Capacity, write or checksum
errors do not mutate the guest or replace a previously valid candidate. C27's
host continuation verifies its source round trip using streamed byte comparison
and computes its final SHA without another full RVG allocation.

## Validation

- Original serializer versus streaming serializer, exact byte comparison.
- All RVG variants, bounded/adaptive memory, sparse/dense pages, addresses beyond
  uint64, quantum bindings, MMU/device and FP/SIMD state.
- Restore through the original decoder and compare subsequent CPU execution.
- Capacity rejection, failed sink, valid-candidate preservation and cleanup.
- Existing 42,039,103-byte file transport and atomic boot-journal regressions.
- Release and UBSan stream tests.
- Host benchmark restores first, releases the input buffer, then forks a fresh
  child so serializer peak RSS excludes the unchanged decoder's earlier peak.
  It measures serialization plus file output and SHA in both modes, with zero
  guest instructions. These are host I/O measurements, not phone boot speed.

The C43 workflow also authenticates the original C42 stage 10 contract, restores
the same verified disk, and requires both legacy and streamed exports to match
the original checkpoint byte for byte: 25,200,742,731 instructions; SHA-256
`96c73b59100d80030c53ef9b84324e3dde7fa26c8cb062ebfd5d171b96022706`.
The workflow publishes evidence, not a new continuation source.

Local fixture measurement (42,039,103-byte original RVG, five runs per mode):
median serializer-process peak RSS was 124,104 KiB with the legacy path and
42,068 KiB with the stream. The SHA-256 was identical in all ten runs:
`955afa6ab9d9479110081208ff7fe1a0fe09c60dbe3aa08e255310900a4446f4`.
This is the isolated post-restore child described above, not total startup RAM
or the Galaxy A16. Timings collected while native builds shared the host are
not used to claim a boot-speed improvement.

## Next cycles

2. Keep a native guest session resident between execution chunks, with explicit
   flush/pause/export lifecycle and preservation of the last durable checkpoint.
   The initial decoder and its memory peak need their own measurement.
3. Add a bounded incremental journal around the original format, verify crash
   recovery/compaction, and reconstruct the same canonical RVG on export.
4. Address the Android 256 MiB import ceiling only with measured restoration
   capacity and device checks; build and validate the APK bridge.

This cycle retains the existing 256 MiB phone file-operation ceiling and the
explicit 1 GiB host transport ceiling. It does not resolve the independent
unsupported FMLA instruction, advance Android, or claim a completed boot.
