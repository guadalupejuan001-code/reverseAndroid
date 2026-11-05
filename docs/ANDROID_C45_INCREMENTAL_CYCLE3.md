# C45: incremental journal around the original compact checkpoint

C44 retains one original MinimalGuest/QuantumRuntime between Android requests.
C45 makes periodic durable checkpoints retain only changed canonical records,
instead of writing the complete RVG every time. It does not change snapshot(),
snapshot_to(), from_snapshot(), the XI/Ether grammar, guest CPU or guest clock.

## Records and identity

The journal contains a text descriptor and the original record bytes. It does
not hex-encode a whole RVG, flatten logical RAM or introduce another quantum
memory. MXIM2/MXIM3 page records preserve their original hexadecimal spelling.
The existing COW page index identifies changed leaves; unchanged pages are not
encoded again. Deletions, large decimal addresses, bindings, quantum core,
registers, devices and FP/SIMD components are retained exactly.

Each journal is bound to the SHA-256 of its immutable canonical RVG base. Its
first checkpoint verifies that the decoded authority round-trips to that base,
builds an index of original record extents, and retains a COW baseline for the
next comparison. This one-time validation is substantial for a large checkpoint.
Subsequent saves do not serialize the unchanged image or memory pages.

Each generation has a bounded payload and a SHA-linked metadata file. Writes
are synced before the generation is published; the directory is synced before
the head pointer is published. Independently synced current/previous pointers
select committed history. An unselected staged generation is never recovered.
The in-memory baseline advances only after this publication succeeds.

The chain is limited to 16 generations and the smaller of 64 MiB or one quarter
of the base size, with a 1 MiB minimum. Reaching the bound requests a full
canonical save. Pause, export, manual changes and replacement also compact to
the original RVG and retire the journal after successful file publication and
sync of its parent directory. The base file is also synced before journal setup.
Retirement renames and syncs the directory before cleanup so a crash during
cleanup cannot expose a half-deleted active journal. Failure to detach is
reported and leaves the resident state available; deleting retired files is
best effort.

## Recovery and replacement

Recovery validates the bound base, pointers, metadata links and payload hashes.
It reconstructs the original RVG order and original FNV checksum, then invokes
the unchanged original decoder. It executes no guest instructions. An incomplete
or corrupt selected payload selects the last complete verified prefix. An invalid
head can use the previous pointer. A verified pointer can reconstruct a damaged
identity marker's base binding. With that binding established, unverifiable
generations fall back conservatively to the complete base. Reports expose rollback; the guest view indicates when an
incomplete segment was discarded.

An explicit import first compacts any pending journal and retires its identity,
including an import of the same original base. This prevents an old overlay from
undoing a deliberate restore. A failed journal write preserves the live state and
the earlier selected generation so retry does not replay CPU work. Unsaved work
since the last periodic acknowledgement is still lost on abrupt process death.

## Validation and interpretation

Native checks cover canonical byte equality, the original decoder, sparse/dense
pages, deletions, memory reset, huge addresses, quantum bindings/core, legacy
memory, stale history, failed storage, bounded compaction and three actual
SIGKILL points before head publication. The C43 serializer and C44 resident
tests remain independent regressions; UBSan exercises the new journal path.

Android 15 tests deliberately kill the disposable test process with committed
journal state plus newer unsaved CPU progress. A second process must recover the
exact independently generated RVG. A second case truncates the selected tail and
recovers the preceding generation. Export, failed-write retry, explicit original
import, Activity pause and the genuine Android GKI path are also checked. Test
packages compile for ARM64/x86_64; execution on the user's phone belongs to C46.

The C42 benchmark compares four equal durable boundaries, followed by cold
recovery and a canonical export, with the same CPU work and original disk. It
reports initialization, individual save times, recovery, export, bytes and total
time separately. It is not comparable directly to C44's four-block/one-save
benchmark. A short session can be slower overall because of initial validation
and recovery even when subsequent checkpoints write far fewer bytes and finish
faster. Full large-checkpoint timings are recorded in CI evidence and the PR.

## Pending

The existing phone transport limit remains 256 MiB and the explicit host test
limit remains 1 GiB. Initial decoding and first journal preparation still require
work in C46. The independent FMLA instruction and completed Android boot remain
outside this checkpoint cycle. Benchmark outputs are not promoted as continuation
sources; the original verified C42 source remains intact.
