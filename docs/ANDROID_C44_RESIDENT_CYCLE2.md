# C44, cycle 2: one resident original guest between execution requests

The Android file-operation path restored the complete RVG and wrote a complete
candidate for every request, including each 250,000-instruction continuous block
and display inspection. C43 reduced serialization copies. C44 retains one
original MinimalGuest/QuantumRuntime between requests, reducing repeated load
and save work while preserving the compact representation and decoder.

## Execution and publication

`GuestResidentSession` holds the original VM. Mutations use a private COW
candidate and publish only a successful operation. A failed ISA/XI request
preserves the earlier accepted blocks. Inspection and display execute no CPU.
The registry uses non-reused opaque handles, validates stale handles, serializes
operations and refuses to close a dirty guest.

Continuous requests retain accepted state in RAM. A checkpoint becomes due
after 30 seconds of host wall time or 50 million guest instructions since the
last acknowledged save, checked at request boundaries. An in-flight request can
extend the wall deadline. Neither threshold changes the guest clock, CPU budget
or architectural state. Manual execution, XI, console input, halt/wait, pause,
export and replacement explicitly flush. A crash between acknowledged saves can
lose the bounded unsaved window; periodic checkpoints remain the recovery source.

Staging a complete canonical RVG does not clear the dirty flag. Java publishes
through AtomicFile, syncs its file descriptor and checks the resulting bytes
against the candidate's SHA-256 before acknowledging the exact host revision.
A failed publication leaves the live state dirty and available for retry.
The platform's finishWrite can log a sync/rename error without throwing, so its
return alone is not accepted as proof of publication. Primary source:
https://android.googlesource.com/platform/frameworks/base/+/HEAD/core/java/android/util/AtomicFile.java

Activities share one process-wide resident handle on RuntimeExecutor. Ownership
prevents a delayed callback from an older Activity from closing a newer user's
session. Activity pause/destroy queue a flush and release after the current
native operation; the UI thread does not block on checkpoint I/O. A failed flush
retains the resident state. Abrupt process death can still precede an asynchronous
pause flush; it restores the last complete acknowledged checkpoint.

The guest view drains pause requests after an in-flight block, does not submit
another block while stopping, preserves the original execution error, and avoids
reopening the guest solely to request a frame after pause. The visible checkpoint
size refers to the last durable checkpoint while live changes remain pending.

## Validation

- Native tests: canonical byte equality with direct original execution; run with
  the input checkpoint temporarily unavailable; explicit publication/ack;
  stale acknowledgements; dirty close; failed writes; ISA/XI rollback; wall and
  instruction cadence; stale handles and concurrent requests.
- Release and UBSan; JavaScript race tests with deferred native operations.
- Synthetic 42 MB Linux Image loop: equal requests, original RVG equality, and
  independent direct execution. It is explicitly not an Android boot fixture.
- Genuine C42 stage 10: same disk, same original source, four equal CPU requests
  through reload/save and resident paths, canonical exports compared byte for
  byte. The initial source must remain unchanged. Benchmark outputs are test
  results and are not promoted as continuation sources.
- Android 15 emulator: unchanged-format 42 MB import; four live requests;
  pause/export; actual Activity background/foreground; stale owner; failed
  publication followed by retry; genuine 78 MB Android GKI continuation checked
  against independent host execution. ARM64 and x86_64 packages compile.

## Remaining cycles

Cycle 3 adds incremental journaling, compaction and crash recovery around the
same RVG export. Cycle 4 addresses the 256 MiB phone import limit after measuring
the initial decoder's memory requirements and preparing a device-tested build.
C44 retains that phone limit. The host benchmark explicitly permits the existing
1 GiB host bound to validate C42. The independent unsupported FMLA instruction
is not changed. Host transport timings do not claim a completed Android boot or
phone-wide speedup; the initial decoder still has its original memory peak.
