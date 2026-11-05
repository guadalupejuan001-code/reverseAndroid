# C50: SurfaceFlinger vector float-to-integer conversion

C48 executed UCVTF and saved a clean checkpoint at 28,109,215,990 instructions,
SHA-256 `5193c20e52dd2bc6612a39e478be2aab698866f2a5a8f8a5289a14cdbd450b5a`.
The next missing instruction occurs 4,628 instructions later: `0x6ea1b800`,
FCVTZU V0.4S,V0.4S, PC `0x6fc27b30d4`. The actual source float lanes are 0.5,
20.314241409301758, 40.128482818603516 and 59.942726135253906. Correct truncation
produces the unsigned integers 0, 20, 40 and 59. The diagnostic tail hash is
`ed006047d64c776b9263044a506d1fb0ffd0dcef3a33e4fa550385e45044189a`.
It is not a resumption source. Independently replaying the 4,628 instructions
from the clean C48 checkpoint has already recreated those bytes as a clean
completed budget, retaining the original source admission gates.

Implement allocated FCVTZS/FCVTZU vector 2S, 4S and 2D forms through the existing
ArmFloat scalar conversion, with fixed rounding toward zero, active lanes only,
cumulative FPSR, original FP access trap, source-before-destination aliasing and
upper clearing. Preserve range saturation, NaN/Infinity and subnormal/FZ behavior.
Reserved 1D and unsupported FP16 forms remain rejected atomically. The original
scalar conversions and all other opcode handlers remain unchanged.

Release tests pass 8,453 cases / 306,819 checks, including the actual source
vector, all register encodings, all FPCR rounding/FZ/DN modes, signedness, NaNs,
Infinity, finite range limits, subnormals, inactive lanes, cumulative QC and
checkpoint replay. UBSan and the independent native AArch64 oracle must also
pass. The oracle executes actual encodings using integer extrema, IEEE special
values and random bits. See the conversion mappings in the
[official Arm Neon reference](https://arm-software.github.io/acle/neon_intrinsics/advsimd.html).

The compact RVG/MXIM representation, original decoder, XI/Ether authority,
logical RAM, disk, counter seed/cadence, security and every-instruction timer/IRQ
checks retain their existing semantics. C49's opt-in million-instruction host
chunk reduces repeated COW index copying; it does not skip architectural steps.

CI first compares the pinned C48 binary and the new CPU before the observed
instruction, reconstructing the same bytes as a clean budget. It then executes
one FCVTZU and checks the actual result, probes a million instructions, and
continues in bounded billion-instruction stages. Upload each verified boundary
before proceeding; failed tails remain diagnostic. Full boot still needs live
system_server lineage, framework evidence, live PID1 sys.boot_completed=1 and
enforcing SELinux. Scanout and human visual review remain separate evidence.
