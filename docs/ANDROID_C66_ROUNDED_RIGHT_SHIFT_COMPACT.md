BootAnimation thread 1962 stops at PC 0x7b9af6a898 on 0x6f182485, URSHR V5.8H,V4.8H,#8. The last clean C65 checkpoint is at 51,183,780,033 instructions. The original unchanged C65 CPU independently replays 591,279,703 instructions to the clean pre-instruction boundary; the failed tail is diagnostic only.

Implement 32 scalar/vector forms of SRSHR, URSHR, SRSRA and URSRA. Vectors support the allocated B/H/S/D arrangements; scalar forms support D. Source aliases are captured before writes. Quotient plus discarded rounding bit avoids host overflow and shifts by 64; lane writes perform exact modular accumulation. Scalar and 64-bit vector writes clear unwritten upper bits. Unallocated forms reject before FP access, and zero immh remains with the original modified-immediate decoder.

Tests compare a separate signed 128-bit mathematical bias/floor/modulus reference across every legal immediate, source and destination register, boundary values, aliases and randomized FP modes/status. Physical ARM executes the actual encodings. Original snapshot decoding, FP traps and reserved-form rollback are also verified.

The actual Android source V4 is zero and old V5 is nonzero. The instruction must remove V5 from the original sparse FPU representation, reducing its RVG section by exactly 16 bytes. The one-instruction proof verifies every other register, all non-FPU state bytes, original section lengths, PC/count/phase and both SHA-256 values with the original decoder unchanged.

XI/Ether authority, RVG/MXIM encoding, disk, 2 GiB logical RAM, original HMAC seed, 64 instructions per tick, interrupt checks and enforcing SELinux remain unchanged. The separate UART input prototype is not part of this CPU repair. Android boot is accepted only by the existing C46 system_server, live boot property and actual display checks.

Measured local release and UBSan runs each passed 1,094,657 cases and 73,344,101 checks. Physical ARM CI must pass at least 1,027,329 actual instruction comparisons before long continuation.
