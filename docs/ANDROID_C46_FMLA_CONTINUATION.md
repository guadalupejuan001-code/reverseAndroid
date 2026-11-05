# C46: repair indexed FMLA and continue the authentic Android machine

C42 stage 11 stopped at instruction 25,697,785,171, PC `0x738df9df3c`,
opcode `0x4fc01062`: FMLA V2.2D,V3.2D,V0.D[0]. The last clean source is
C42 stage 10 (25,200,742,731 instructions), SHA-256
`96c73b59100d80030c53ef9b84324e3dde7fa26c8cb062ebfd5d171b96022706`.
The architectural-stop candidate is diagnostic evidence, not a resume source.

## CPU change

Decode the allocated FP32/FP64 scalar and vector FMLA/FMLS indexed-element
forms. Select H/L and the full five-bit source register correctly, copy all
operands before a destination alias can change them, and use the existing
ArmFloat fused operation. FP access traps, rounding, cumulative FPSR and upper
clearing stay architectural. Reserved 1D/D:L forms and unsupported FP16 remain
rejected atomically. No instruction is skipped and no estimated result is used.

The existing XI/Ether memory remains the authority. This repair does not change
the RVG/MXIM format, decoder, compact state, disk, guest clock, SELinux or odsign.
The C43/C44/C45 checkpoint changes remain included. This continuation keeps the
existing authenticated BootCycleJournal publication contract.

## Gates

- Independently specified integer-exact and IEEE fused/NaN cases; all indexed
  lanes, high source register bits, destination aliases, checkpoint restore,
  access trap and rejection/rollback.
- UBSan, existing FP and C41 conversion regressions.
- A native AArch64 runner executes the actual encodings as an independent
  oracle across all supported FPCR rounding, FZ and DN combinations.
- The original C42 source is authenticated and round-tripped. Old and repaired
  CPU outputs must be identical through a million instructions before the
  observed stop. The first continuation then crosses that stop by one real
  instruction and saves a new verified boundary.

## Continuation and completion

Every stage uses only a clean source with a validated contract and records its
exact predecessor. Each newly verified checkpoint is uploaded before a following
stage starts; failed stages preserve diagnostics without promoting their tails.
Only temporary, previously uploaded stage directories may be pruned.

The existing observer independently checks live Zygote/system_server identity,
parentage, odsign and SELinux. Completed boot additionally requires the existing
framework proof, including `sys.boot_completed=1` in its live PID1 property
mapping. A scanout is retained separately for visual review. A live launcher,
system_server alone, elapsed time or an instruction count is never full boot.

Source references: Arm Neon ACLE `vfmaq_laneq_f64` and Arm A64 FMLA/FMLS
indexed-element pseudocode. The arithmetic uses the already pinned SoftFloat;
it does not introduce another emulator or memory model.
