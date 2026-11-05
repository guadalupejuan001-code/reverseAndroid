# C53: scalar SIMD integer comparisons

The verified C52 probe completed one million additional Android instructions at count 28,110,243,610 (RVG SHA-256 `7704ff024431aa0500c7fb7e4afd01647d842a06dcf98c0680823ac2728418e9`). The next long cycle advanced 24,639,807 instructions before SurfaceFlinger thread 778 reached unsupported `0x7ee13402` at PC `0x6fc5de3cd8`: CMHI D2,D0,D1. Both actual low source lanes are 256, so D2 must become zero and its upper half must clear. D2 is already zero in this observed input; all other architectural state must remain unchanged except normal PC/count/clock advancement.

The failed tail is diagnostic only. A separate clean replay of precisely 24,639,807 instructions from the verified C52 point reproduced its complete original bytes, SHA-256 `62fd322bda96f1d807f8bb783cefa55c0b1b437f8f47b1ec8e6a22413cacd9ed`, with a completed instruction budget and every observation/security gate passed. This is the continuation source.

Add the allocated 64-bit scalar register comparison family CMGT/CMHI, CMGE/CMHS, CMTST/CMEQ, plus the associated CMGT/CMGE/CMEQ/CMLE/CMLT comparisons with zero. Signed ordering uses an exact sign-bit bias. Comparison outputs are all ones or all zeros, upper bits clear, aliasing preserves both inputs, and NZCV/FPCR/FPSR remain unchanged. The original FP access trap applies. Unallocated B/H/S scalar forms reject atomically. Existing vector comparisons and all FP arithmetic retain their code.

Release and UBSan each passed 46,354 cases / 1,693,542 checks. These cover all source register pairs and destination aliases, all register numbers for zero forms, signed/unsigned limits, bit masks, FPCR modes, original snapshot replay, inactive high lanes and atomic rollback. Native AArch64 CI executes the actual instruction words and independently checks outputs and FPSR before genuine continuation.

CI starts from already durable C50 stage 2, with the hardware-validated and SHA-pinned C51 native reference. It replays 25,662,798 instructions with the reference and candidate, checks complete bytes against the clean pre-CMHI SHA above, then executes exactly one comparison and validates the observed zero result. A million-instruction probe and bounded longer stages follow, with verified artifacts uploaded before each continuation.

The original compact RVG/MXIM representation and decoders, XI/Ether authority, disk and authentication, logical RAM, counter seed and 64-instruction cadence, IRQ checks and SELinux are preserved. C52's independently verified faster host checksum and metadata observations remain opt-in and are enabled by this workflow. No guest instruction is skipped or replaced by a second execution engine.

Full boot still requires live system_server with Zygote lineage, framework evidence, live PID1 sys.boot_completed=1 and enforcing SELinux. Real display output remains independently checked.
