// Independent AArch64 assembler/execution witness. No host FP calculations.
// Every record: complete 128-bit destination, FPSR, and (for scalar) NZCV.
.text
.global _start
_start:
    sub sp, sp, #8, lsl #12
    mov x20, sp
    mov x21, sp
    adr x22, .Lsingle
    mov x23, #16
.macro save reg
    str \reg, [x21], #16
    mrs x10, fpsr
    str x10, [x21], #8
.endm
.macro scalar instruction
    msr fpsr, xzr
    \instruction s2, s0, s1
    save q2
.endm
.Lsloop:
    ldr s0, [x22], #4
    ldr s1, [x22]
    scalar fadd
    scalar fsub
    scalar fmul
    scalar fdiv
    scalar fmax
    scalar fmin
    scalar fmaxnm
    scalar fminnm
    scalar fnmul
    msr fpsr, xzr
    fsqrt s2, s0
    save q2
    msr fpsr, xzr
    fmadd s2, s0, s1, s0
    save q2
    fmsub s2, s0, s1, s0
    save q2
    fnmadd s2, s0, s1, s0
    save q2
    fnmsub s2, s0, s1, s0
    save q2
    fcmp s0, s1
    mrs x10, nzcv
    str x10, [x21], #8
    fcmpe s0, #0.0
    mrs x10, nzcv
    str x10, [x21], #8
    fcvtzs w10, s0
    str x10, [x21], #8
    fcvtzu x10, s0
    str x10, [x21], #8
    fcvtas x10, s0
    str x10, [x21], #8
    frintn s2, s0
    save q2
    frintx s2, s0
    save q2
    fcvt d2, s0
    save q2
    fcvt h2, s0
    save q2
    fcvt s2, h2
    save q2
    subs x23, x23, #1
    b.ne .Lsloop
    adr x22, .Ldouble
    mov x23, #16
.macro dscalar instruction
    msr fpsr, xzr
    \instruction d2, d0, d1
    save q2
.endm
.Ldloop:
    ldr d0, [x22], #8
    ldr d1, [x22]
    dscalar fadd
    dscalar fsub
    dscalar fmul
    dscalar fdiv
    dscalar fmax
    dscalar fminnm
    msr fpsr, xzr
    fsqrt d2, d0
    save q2
    fmadd d2, d0, d1, d0
    save q2
    fmsub d2, d0, d1, d0
    save q2
    fnmadd d2, d0, d1, d0
    save q2
    fnmsub d2, d0, d1, d0
    save q2
    fcvtzs x10, d0
    str x10, [x21], #8
    fcvtzu w10, d0
    str x10, [x21], #8
    fcvtas w10, d0
    str x10, [x21], #8
    frintm d2, d0
    save q2
    frintz d2, d0
    save q2
    fcvt s2, d0
    save q2
    fcvt h2, d0
    save q2
    fcvt d2, h2
    save q2
    subs x23, x23, #1
    b.ne .Ldloop
    // All four rounding modes, DN and FZ, exact halfway, underflow and overflow.
    mov x24, #0
.Lmode:
    lsl x10, x24, #22
    msr fpcr, x10
    msr fpsr, xzr
    adr x22, .Lsingle
    ldr s0, [x22, #32]
    ldr s1, [x22, #36]
    fadd s2, s0, s1
    save q2
    fmul s2, s1, s1
    save q2
    fdiv s2, s0, s1
    save q2
    ldr s0, [x22, #24]
    ldr s1, [x22, #28]
    fmul s2, s0, s1
    save q2
    ldr s0, [x22, #40]
    ldr s1, [x22, #44]
    fmul s2, s0, s1
    save q2
    fmaxnm s2, s0, s1
    save q2
    mov x10, #0x7fffffffffffffff
    scvtf s2, x10
    save q2
    ucvtf d2, x10
    save q2
    add x24, x24, #1
    cmp x24, #16
    b.ne .Lmode
    msr fpcr, xzr
    msr fpsr, xzr
    // FMA NaN operand priority and invalid 0 * infinity + quiet/signaling NaN.
    adr x22, .Lnantriples
    mov x23, #6
.Lnanloop:
    ldr s0, [x22], #4
    ldr s1, [x22], #4
    ldr s3, [x22], #4
    msr fpsr, xzr
    fmadd s2, s0, s1, s3
    save q2
    fmsub s2, s0, s1, s3
    save q2
    fnmadd s2, s0, s1, s3
    save q2
    fnmsub s2, s0, s1, s3
    save q2
    subs x23, x23, #1
    b.ne .Lnanloop
    mov x10, #0x2000000
    msr fpcr, x10
    adr x22, .Lsingle
    ldr s0, [x22, #56]
    fcvt h2, s0
    save q2
    msr fpcr, xzr
    adr x22, .Lvectors
    ldp q0, q1, [x22]
.macro vector instruction, suffix
    \instruction v2.\suffix, v0.\suffix, v1.\suffix
    save q2
.endm
    vector and, 16b
    vector bic, 16b
    vector orr, 16b
    vector orn, 16b
    vector eor, 16b
    vector bsl, 16b
    vector bit, 16b
    vector bif, 16b
    vector add, 16b
    vector sub, 8h
    vector mul, 4s
    vector add, 2d
    vector cmeq, 16b
    vector cmgt, 8h
    vector cmge, 4s
    vector cmhi, 4s
    vector cmhs, 2d
    not v2.16b, v0.16b
    save q2
    rev64 v2.16b, v0.16b
    save q2
    rev64 v2.8h, v0.8h
    save q2
    addv b2, v0.16b
    save q2
    addv s2, v0.4s
    save q2
    mov w10, #0x1234
    dup v2.4s, w10
    save q2
    dup v2.2d, v1.d[1]
    save q2
    ins v2.s[2], w10
    save q2
    ins v2.d[1], v1.d[0]
    save q2
    umov w10, v2.b[3]
    str x10, [x21], #8
    smov x10, v0.s[2]
    str x10, [x21], #8
    fmov x10, v2.d[1]
    str x10, [x21], #8
    fmov v2.d[1], x10
    save q2
    movi v2.16b, #0x42
    save q2
    movi v2.4s, #0x42, lsl #16
    save q2
    movi v2.8h, #0x81, lsl #8
    save q2
    movi v2.4s, #0x81, msl #16
    save q2
    movi v2.2d, #0xff00ff00ff00ff00
    save q2
    mvni v2.4s, #0x42
    save q2
    orr v2.8h, #0x20, lsl #8
    save q2
    bic v2.4s, #0x11, lsl #16
    save q2
    fmov v2.4s, #1.5
    save q2
    fmov v2.2d, #-2.0
    save q2
    ext v2.16b, v0.16b, v1.16b, #7
    save q2
    zip1 v2.4s, v0.4s, v1.4s
    save q2
    zip2 v2.8h, v0.8h, v1.8h
    save q2
    uzp1 v2.16b, v0.16b, v1.16b
    save q2
    uzp2 v2.8h, v0.8h, v1.8h
    save q2
    trn1 v2.4s, v0.4s, v1.4s
    save q2
    trn2 v2.2d, v0.2d, v1.2d
    save q2
    tbl v2.16b, {v0.16b, v1.16b}, v1.16b
    save q2
    tbx v2.8b, {v0.16b}, v1.8b
    save q2
    ldp q0, q1, [x22, #32]
    vector fadd, 4s
    vector fsub, 4s
    vector fmul, 4s
    vector fdiv, 4s
    vector fmla, 4s
    vector fmls, 4s
    vector fmax, 4s
    vector fmin, 4s
    vector fmaxnm, 4s
    vector fminnm, 4s
    vector fcmeq, 4s
    vector fcmge, 4s
    vector fcmgt, 4s
    vector facge, 4s
    vector facgt, 4s
    fsqrt v2.4s, v0.4s
    save q2
    fabs v2.4s, v0.4s
    save q2
    fneg v2.2d, v1.2d
    save q2
    // 64-bit vector writes must clear the high 64 bits.
    orr v2.8b, v0.8b, v1.8b
    save q2
    add v2.2s, v0.2s, v1.2s
    save q2
    fadd v2.2s, v0.2s, v1.2s
    save q2
    mov x0, #1
    mov x1, x20
    sub x2, x21, x20
    mov x8, #64
    svc #0
    mov x0, #0
    mov x8, #93
    svc #0
.p2align 4
.Lsingle:
    .word 0, 0x80000000, 0x3f800000, 0xc0200000
    .word 0x40200000, 0x3fc00000, 0x00800000, 0x3f000000
    .word 0x3f800000, 0x33800000, 0x7f7fffff, 0x40000000
    .word 0x7f800000, 0xff800000, 0x7fc12345, 0x7f812345, 0x80000001
.Ldouble:
    .quad 0, 0x8000000000000000, 0x3ff0000000000000, 0xc004000000000000
    .quad 0x4004000000000000, 0x3ff8000000000000, 0x0010000000000000, 0x3fe0000000000000
    .quad 0x3ff0000000000000, 0x3ca0000000000000, 0x7fefffffffffffff, 0x4000000000000000
    .quad 0x7ff0000000000000, 0xfff0000000000000, 0x7ff8123456789abc, 0x7ff0123456789abc, 0x8000000000000001
.p2align 4
.Lvectors:
    .quad 0x807f0f0403020100, 0x80123456fedcba98, 0xfffe000300100002, 0x0102030405060708
    .word 0x3f800000, 0xc0000000, 0x7fc12345, 0x00000001
    .word 0x3f000000, 0x40000000, 0x7f812345, 0x00800000
.Lnantriples:
    .word 0x7fc12345, 0x40000000, 0x7fc54321
    .word 0x3f800000, 0x7fc12345, 0x7fc54321
    .word 0x7f812345, 0x40000000, 0x7fc54321
    .word 0x7f812345, 0x7f854321, 0x7f811111
    .word 0x7f800000, 0, 0x7fc12345
    .word 0, 0x7f800000, 0x7f812345
