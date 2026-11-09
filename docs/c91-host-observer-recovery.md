# C91: continue the preserved C90R stage 5 with a bounded host observer

## Actual source and failure

C90R admitted and preserved five checkpoints. Its last complete source is
**165447727998 instructions**, SHA-256
`7c00c86bb9af645fe7a6f416f63c0145351234ec20d749c2bf5b3dd30f5dd802`.
The [stage 5 authority](evidence/c90r/stage5-original-CI-authority.json) pins all
four transport pieces, original admission and producer metadata. Stages 2–4
also retain their own locks, proofs and authenticated cumulative consoles.

Stage 6's original native process finished one billion instructions with exit
code zero and its original snapshot roundtrip. The subsequent kernel observer
failed with `Oversized RVG memory record`. The original C46 admission did not
complete and the full failed RVG was not uploaded. That tail is diagnostic
evidence only; C91 recovers stage 5. The exact
[failure evidence](evidence/c90r/stage6-host-observer-failure.json) includes the
process/report/contract bindings and original observer error.

## Reproduced host defect

On Node 24.22.1, numeric `Buffer.indexOf` uses signed 32-bit positions in the
affected large-buffer path. In the actual regression, a newline at position
2147500024 produced the negative result -2147467272. The original bounded line
reader treated that result as end-of-file and then rejected an apparently huge
remaining record. A starting position above the signed boundary can also be
clamped backwards.

The valid MXIM page record remains at most 9240 bytes: its tag, a decimal 64-bit
page number, separators, a 512-byte bitmap in hexadecimal and up to 4096 values
in hexadecimal. The existing default record bound of **16384 bytes** is
sufficient. It is retained, as is the maximum explicit bound of **65536 bytes**.

The selected reader searches a zero-copy view of at most `maxLineBytes + 1`
bytes and decodes Latin-1 using local positions. It continues to reject genuinely
oversized records, preserves high bits for the existing validators, and preserves
empty records, final-newline and unterminated-final-record behavior. It does not
change RVG/MXIM serialization or decode the entire memory section into one string.

The regression uses a real **2147532805-byte** Buffer and visits **131067** full
records plus the final suffix. It reproduces the original signed-index failure,
verifies the complete corrected traversal and truncated EOF, and rejects a
genuinely oversized record beyond 2 GiB. Differential cases compare normal
records, limits and failures directly with the original reader.

## Explicit, authenticated observer profile

The original `tools/rvg_memory_lines.mjs` and all **870 imported files** remain
unchanged. The corrected implementation is in
`extensions/host-memory-lines/rvg_memory_lines.mjs` and is copied only into a
separately derived runtime.

`prepare-host-observer.py` first authenticates the complete validated host
transport profile. It then verifies that exactly **one of 148 derived tool
files** changes: `rvg_memory_lines.mjs`. All other derived programs remain byte
identical. The selected native and frame exporter retain their validated hashes;
no guest executable is recompiled or replaced.

The [selected manifest](evidence/c91/selected-host-observer-manifest.json) has
SHA-256 `0580564f9cff84a7aa7edd155cc08f203fd897de0a62b0481f9ce0f201d5d647`.
Its reader has SHA-256
`9235a0743c701e909acc159b8cf3aff1a8ff765268ed644f55bddbf1804939b8`.
The [C91 plan](../state/experiments/c91-plan.json) pins both. The native remains
`a31e3fd923f63071ebadeb6a78ee5aadf77111c88186e428927220f199710318`;
host file capacity remains 3840 MiB and logical guest RAM remains 2 GiB.

## Admission before promotion

C91 must recover the complete authenticated stage 5 source, its proof and its
3964476-byte accumulated console. It preserves all eight historical UART events
and supplies zero new UART bytes. The baseline and corrected source preflights
must agree, and the corrected source kernel and required-peripheral observations
must reproduce the admitted original observations.

Before execution, the source selection is checked against both current repository
pointers read at one observed remote `main` commit. That head and the exact JSON
hashes are recorded in the resume proof. An old workflow rerun cannot silently
select stage 5 after the repository has promoted a later source. The check runs
once, so publishing admitted checkpoints during the active campaign remains
possible.

Its first one-billion-instruction stage must reproduce the entire native RVG
SHA-256 reported by the failed tail:
`7e0135bad3a7c61f12e48d6f97b4c60a89c1848b9e03c464895a5b861bbd612b`,
at **166447727998 instructions**. This is a determinism requirement on a fresh
execution from stage 5, not permission to use the failed tail as a source. Full
original admission, kernel observation, enforcing SELinux and fresh live
peripheral checks must then pass. Promotion requires all checkpoint pieces to
be preserved successfully.

Later bounded stages keep the same native/profile, exact parent, full console
and historical event ledger. They proceed only after the previous checkpoint
has been admitted and completely uploaded. Genuine Android boot remains a
separate gate; ending a bounded campaign does not establish boot completion.

## Android observations at the preserved source

SystemServer is alive at PID 7354 with Zygote parent 5629. Its leader accumulated
additional CPU time between C90R stages 1 and 5, while filesystem/journal waiters
also increased. The journal thread was itself advancing. Those samples show
execution and queued work; they do not establish a deadlock or a unique cause
of the pending Android boot. The original counts, stacks and metadata hashes are
preserved in the [read-only comparison](evidence/c90r/stages1-5-readonly-progress.json).

The authenticated [stage 5 frame review](evidence/c90r/stage5-authentic-frame-review.json)
still shows the Android wordmark at 800×600. `sys.boot_completed=1`, usable UI and
a meaningful response to a real gesture remain unverified. The host reader fix
does not alter any of those acceptance conditions.

## Actual C91 validation and preserved continuation

[C91 run 42596365209](https://github.com/guadalupejuan001-code/reverseAndroid/actions/runs/42596365209),
at commit `a035f386b468c71ad11c32aee88252dbab0b91db`, passed source recovery,
both preflights and the exact original source kernel/peripheral observation
comparisons. Stage 1 then completed with native exit code zero, reproduced the
full expected RVG SHA-256 `7e0135bad3a7c61f12e48d6f97b4c60a89c1848b9e03c464895a5b861bbd612b`,
and reached **166447727998 instructions**. The original kernel observation and
full C46 admission now pass. Fresh live peripherals and enforcing SELinux also
pass, and the native console delta equals the earlier diagnostic execution's
delta byte for byte.

All four checkpoint pieces and the metadata were preserved successfully before
stage 2 began. The [stage 1 authority](evidence/c91/stage1-original-CI-authority.json)
records the real producer, artifact IDs and ZIP hashes, process/report bindings,
proof and authenticated cumulative console. The metadata ZIP was checked locally
by full SHA-256 and CRC. No fresh complete local RVG download or local native
replay is claimed. Canonical authority comes from this fully admitted and
preserved C91 execution; the old failed tail remains diagnostic evidence.

The [original-engine validation for the same commit](https://github.com/guadalupejuan001-code/reverseAndroid/actions/runs/42596365265)
also completed successfully. SystemServer remains alive at PID 7354. The
[authenticated stage 1 frame](evidence/c91/stage1-authentic-frame-review.json)
is byte identical to the visually inspected C90R stage 5 frame: Android wordmark,
800×600. Genuine Android boot, usable UI and gesture response remain pending.

The new [stage 1 recovery lock](../state/experiments/c91-stage-1-artifacts.json)
selects its own pieces, exact proof, complete console and adopted observer
profile. The initial C91 recipe remains fixed to C90R stage 5; its live source
lease will reject an obsolete rerun after promotion. A subsequent campaign must
bind the newest fully admitted source and its console instead of replaying the
initial recipe.
