.text
.global _start
_start:
    adr x20, .Loutput
    mov x0, #-1
    and x0, x0, #0x00ff00ff00ff00ff
    str x0, [x20, #0]
    mov w0, #0
    orr w0, w0, #0x80000001
    str x0, [x20, #8]
    mov x0, #0
    mov w1, #0xffff
    add x0, x0, w1, sxth #2
    str x0, [x20, #16]
    mov x0, #-1
    adds xzr, x0, #1
    adc x0, xzr, xzr
    str x0, [x20, #24]
    mov x0, #0
    subs xzr, x0, #1
    sbc x0, xzr, xzr
    str x0, [x20, #32]
    mov x1, #7
    mov x2, #9
    cmp xzr, xzr
    csneg x0, x1, x2, ne
    str x0, [x20, #40]
    cset w0, eq
    str x0, [x20, #48]
    ccmp x1, #7, #0, eq
    csel x0, x1, x2, eq
    str x0, [x20, #56]
    mov w1, #0x1234
    mov w2, #0xabcd
    extr w0, w1, w2, #4
    str x0, [x20, #64]
    mov x1, #0x8000
    sbfx x0, x1, #8, #8
    str x0, [x20, #72]
    mov x0, #-1
    bfi x0, xzr, #16, #8
    str x0, [x20, #80]
    mov x1, #3
    mov x2, #65
    lsl x0, x1, x2
    str x0, [x20, #88]
    mov x1, #-8
    mov x2, #2
    asr x0, x1, x2
    str x0, [x20, #96]
    mov w0, #0x80
    strb w0, [x20, #512]
    mov x1, #512
    ldrsb x0, [x20, x1]
    str x0, [x20, #104]
    mov w0, #0x80000000
    str w0, [x20, #516]
    ldrsw x0, [x20, #516]
    str x0, [x20, #112]
    mov x1, #0x1234
    clz x0, x1
    str x0, [x20, #120]
    mov x1, #-8
    cls x0, x1
    str x0, [x20, #128]
    mov w1, #1
    rbit w0, w1
    str x0, [x20, #136]
    mov x1, #0x1234
    rev x0, x1
    str x0, [x20, #144]
    mov w1, #-3
    mov w2, #7
    mov x3, #5
    smaddl x0, w1, w2, x3
    str x0, [x20, #152]
    mov w1, #-1
    mov w2, #2
    mov x3, #3
    umaddl x0, w1, w2, x3
    str x0, [x20, #160]
    mov x1, #-2
    mov x2, #3
    smulh x0, x1, x2
    str x0, [x20, #168]
    mov x1, #-1
    mov x2, #-1
    umulh x0, x1, x2
    str x0, [x20, #176]
    add x3, x20, #512
    mov x4, x3
    ldpsw x0, x1, [x3]
    stp x0, x1, [x20, #184]
    ldpsw x0, x1, [x3], #8
    stp x0, x1, [x20, #200]
    sub x0, x3, x4
    str x0, [x20, #216]
    ldpsw x0, x1, [x3, #-8]!
    stp x0, x1, [x20, #224]
    sub x0, x3, x4
    str x0, [x20, #240]
    mov x0, #1
    mov x1, x20
    mov x2, #248
    mov x8, #64
    svc #0
    mov x0, #0
    mov x8, #93
    svc #0
.section .data
.balign 16
.Loutput:
    .zero 1024
