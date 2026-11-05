# C51: SurfaceFlinger fixed-point FP conversion

C50 crossed its real FCVTZU instruction, with the exact expected output and an unchanged original RVG/MXIM decoder. Its clean post-conversion checkpoint is at 28,109,220,619 instructions, SHA-256 `ec801f66fefbe9db6a6524018bf803260ba68ddd8c0013631957c4db6110203d`.

The next authentic probe stopped 22,990 instructions later, at PC `0x6fc27b3354`, instruction `0x4f38fc00`: FCVTZS V0.4S,V0.4S,#8. Its actual source lanes are -0.125, 0.375, -0.375, 0.125. Exact conversion scales by 256 and truncates, yielding signed lanes -32, 96, -96, 32.

The added SIMD scalar/vector FCVTZS/FCVTZU fixed-point handler supports allocated S/D forms, every fractional-bit count, signed and unsigned saturation, active lanes, aliasing, upper clearing and the original FP access trap. Unsupported FP16, narrow 1D and reserved encodings are rejected atomically. Exact exponent scaling applies FZ to the original input and preserves NaNs; it introduces no intermediate floating multiply or spurious FP overflow/inexact flags. The existing integer conversion supplies the final result and cumulative flags.

All original RVG/MXIM bytes and decoders, XI/Ether state authority, logical RAM, full disk authentication, virtual clock seed and 64-instruction cadence, IRQ checks and SELinux semantics remain unchanged. Only the new method and instruction handler are runtime changes.

Local Release and UBSan each passed 18,754 cases / 683,998 checks, including all allocated fractional-bit counts, integer boundaries, FPCR modes, NaNs/infinities/subnormals, aliasing, all register pairs, original snapshot replay and rollback. The separate AArch64 oracle executes actual instruction words and checks results plus FPSR on native hardware before genuine continuation is admitted.

The workflow authenticates clean C50 stage 2, independently replays its 22,990-instruction prefix with pinned C50 and C51, compares all checkpoint bytes to SHA-256 `5222443f91bc5447939bcaa07bf151fd8364168ca72dc09f12eea9fed6e4263a`, then crosses exactly one instruction and validates the actual output. A failed ISA tail is never a continuation source. Subsequent bounded stages retain every verified checkpoint.

Completed Android boot requires live system_server with verified Zygote lineage, framework evidence, live PID1 sys.boot_completed=1 and SELinux. Display pixels are checked independently. These milestones are still pending at publication.
