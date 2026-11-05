# C65: non-saturating indexed integer multiplication

Android Zygote PID 567 reached `MUL V16.4S,V16.4S,V5.S[0]` (`0x4f858210`) in `/apex/com.android.tethering/lib64/libcrypto_httpengine.so` at PC `0x70abf98158`, instruction 49,182,780,032. The original C64 CPU independently replays a clean 72,646,108-instruction prefix from the verified 49,110,133,924 boundary. The clean prefix SHA-256 is `b7db828deb43d0edfa0e071d1b43a0ce793cb0491396d088de62447b29e756c0`.

Implement the 36 allocated, non-saturating H/S by-element forms:

- MUL, MLA and MLS with 64/128-bit vector destinations.
- SMULL/UMULL, SMLAL/UMLAL and SMLSL/UMLSL, including upper-half `2` forms.

The existing Vector128 state owns every operand and result. Capture the source vector, selected scalar and old destination before aliased writes. Halfword selection uses H:L:M and V0–V15; word selection uses H:L and V0–V31. Widening operations select the original lower/upper source half. Unsigned two's-complement arithmetic gives modular signed products without signed C++ overflow. Reject byte/doubleword sizes before FP/SIMD access; retain the original access trap, FPCR/FPSR and NZCV behavior.

The exact observed operand is 2,508. The destination words change from `[4979,3006,6528,5528]` to `[12487332,7539048,16372224,13864224]`. The whole-file crossing verifier permits only those 16 destination bytes, PC, instruction count, counter subtick 3→4 and the checksum to differ. All other original bytes, including the other 31 vectors and memory/device state, must match.

Local release and UBSan each pass 787,969 cases and 52,796,067 checks. Tests use independent signed 128-bit mathematical integers reduced modulo the output width, all legal register/index combinations, aliases, overflow/sign edges, upper-half selection, original snapshot round trips and access/undefined-instruction rollback. On physical ARM the same test executes 184,321 real allocated encodings, including the exact observed Android instruction and operands and compares the hardware vectors and status with both the mathematical oracle and emulator.

The original XI/Ether/InfiniteCore authority, RVG/MXIM codec and decoder, 2 GiB logical RAM, disk, timing seed, 64-instruction tick, instruction-by-instruction IRQ checks and SELinux policy remain unchanged. No serializer, timing or device source changes are included.

CI waits for independently produced C64 stage 12 (47,610,133,924 instructions), pinned by a separate original-C64 local replay to SHA-256 `4dda154c7b16e6b1da0c022b5cb3c5437541c47f6f3d7af5b1d9236e62d2c018`. It then compares original and repaired prefix checkpoint bytes, verifies the exact one-instruction crossing, runs a million-instruction probe and continues bounded genuine Android cycles. Source admission and completed-boot gates are unchanged. Completed Android boot still requires live system_server lineage, framework evidence, live PID 1 `sys.boot_completed=1`, enforcing SELinux and review of the original GPU scanout.

Instruction references: [Arm Instruction Set Reference Guide](https://documentation-service.arm.com/static/6245c734b059dc5ff9a8bdab), [primary Go AArch64 encoding table](https://go.googlesource.com/arch/+/edaf650c5f69cf0d12eb548780b42a7ba0d982c1/arm64/arm64asm/inst.json). The physical ARM oracle independently executes the emitted words.
