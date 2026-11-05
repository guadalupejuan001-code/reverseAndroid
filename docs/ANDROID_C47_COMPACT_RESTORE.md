# C47: restore original compact state without whole-text copies

C46 executed the missing indexed FMLA and reached a clean boundary at
27,247,785,172 instructions. PID 567 has become `zygote64`. Its checkpoint is
1,066,302,863 bytes, SHA-256
`064ba023cffbb1bf7889e053ecea97435a9975d1781bd4c74bb0524b179f59c8`.
The next stage failed while saving: `Guest snapshot exceeds configured host
transport capacity`. It had only 7,438,961 bytes of headroom under 1 GiB.
The failed process is not a continuation source; C47 starts from clean stage 4.

## Preserve the representation and original decoder

The original XI/Ether QuantumRuntime and InverseMemory still own the state.
RVG/MXIM bytes, revisions, core, bindings, random state and subsequent execution
are unchanged. The new view reader consumes the same grammar directly from
the input buffer, avoiding complete copies of the memory text. Restored data
are owned by the runtime; the buffer can be released before CPU execution.
The original decoder and adaptive memory decoder remain unchanged as oracles.
The frame observer compares streamed serialization rather than allocating an
additional complete snapshot. This is a transport optimization, not a change
to the original model or a claim of increased representational compression.

## Explicit finite host bound

Only binaries built with `REVERSE_PC_COMPACT_RESTORE_VIEW=1` select the new
reader and a 2 GiB host checkpoint bound. Logical guest RAM remains 2 GiB; the
ordinary 256 MiB default and original 1 GiB decoder limit remain unchanged.
The larger host bound is recorded consistently in native pins, provenance,
result and continuation contract, outside serialized guest state. Existing
contracts retain their own 1 GiB bound. Opening a larger contract requires an
explicit host option. Failed processes, ISA stops, mismatched disk/clock/RAM,
unclean boundaries and source lineage failures remain rejected.

## Measurements and gates

On authentic C46 stage 3 (1,015,614,783 bytes), isolated restoration of the
original runtime memory component used 4,504,864 KiB peak RSS with the original
reader and 1,685,152 KiB with the view reader: a 62.6% reduction. Restore time
was 8.395 s versus 5.711 s. Both restored 395,604,870 overrides and reproduced
memory SHA-256 `3d7435fe6e0f16b179947d78b8f685e02915cab1d2c53f7819c22e34aa94885f`.
This measures memory transport restoration, not a full Android or phone peak.

Local release and UBSan gates cover 13 scenarios and 731 checks each, including
non-Clifford core state, bindings, huge addresses, sparse/dense pages, discarded
input buffers, transactional failure and 400 checksum-repaired corruptions.
Host gates cover 213 cases. The scanout protocol fixture executes no Android.

CI repeats restoration measurements on the authentic stage 4, then executes
one million real Android instructions using the independently pinned C46
binary and the new reader. Full output bytes must match. Only then may genuine
continuation start. Each clean stage is uploaded before proceeding; the first
250 million instruction budget exercises the larger output path. Later stages
use bounded billion-instruction budgets and stop on verified completed boot.

Completed boot still requires live `system_server` with verified Zygote
parentage, the framework evidence, live PID1 `sys.boot_completed=1`, and SELinux
enforcement. Scanout export and visual review remain separate evidence. A
renamed Zygote, an instruction count or elapsed time does not establish boot.
