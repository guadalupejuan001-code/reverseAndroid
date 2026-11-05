// Small, self-checking program for XI -> ARM64. Independently assembled in CI.
.text
.global _start
_start:
    sub sp, sp, #32
    fmov s0, #1.0
    fmov s1, #2.0
    fadd s2, s0, s1
    fmov w0, s2
    mov w1, #0x40400000
    cmp w0, w1
    b.ne .Lfailed
    fmov d0, #4.0
    fsqrt d1, d0
    fmov x0, d1
    mov x1, #0x4000000000000000
    cmp x0, x1
    b.ne .Lfailed
    movi v2.16b, #42
    add v2.16b, v2.16b, v2.16b
    str q2, [sp]
    ldr q3, [sp]
    umov w0, v3.b[15]
    cmp w0, #84
    b.ne .Lfailed
    adr x1, .Lok
    mov x2, #27
    mov x19, #0
    b .Lprint
.Lfailed:
    adr x1, .Lbad
    mov x2, #24
    mov x19, #1
.Lprint:
    mov x0, #1
    mov x8, #64
    svc #0
    mov x0, x19
    mov x8, #93
    svc #0
.Lok:
    .ascii "REVERSE: FP32 FP64 SIMD OK\n"
.Lbad:
    .ascii "REVERSE: FP SIMD FAILED\n"
