.section .text
.global _start
_start:
  ldr x22, =0x40090000
  mov x9, #17
  mov x4, #34
  stp x9, x4, [x22]
  ldxp x0, x1, [x22]
  cmp x0, x9
  b.ne fail
  cmp x1, x4
  b.ne fail
  add x9, x9, #1
  add x4, x4, #2
  stlxp w10, x9, x4, [x22]
  cbnz w10, fail
  ldp x0, x1, [x22]
  cmp x0, x9
  b.ne fail
  cmp x1, x4
  b.ne fail
  stxp w10, x9, x4, [x22]
  cbz w10, fail
  ldaxp x0, x1, [x22]
  clrex
  stlxp w10, x9, x4, [x22]
  cbz w10, fail
  mov w9, #51
  mov w4, #68
  stp w9, w4, [x22]
  ldaxp w0, w1, [x22]
  cmp w0, w9
  b.ne fail
  cmp w1, w4
  b.ne fail
  add w9, w9, #1
  add w4, w4, #1
  stxp w10, w9, w4, [x22]
  cbnz w10, fail
  ldp w0, w1, [x22]
  cmp w0, w9
  b.ne fail
  cmp w1, w4
  b.ne fail
  adr x1, success
  b print
fail:
  adr x1, failure
print:
  ldr x2, =0x09000000
1:
  ldrb w0, [x1], #1
  cbz w0, done
  str w0, [x2]
  b 1b
done:
  mov x0, #8
  movk x0, #0x8400, lsl #16
  hvc #0
  b done
success:
  .asciz "REVERSE: EXCLUSIVE PAIR OK\n"
failure:
  .asciz "REVERSE: EXCLUSIVE PAIR FAIL\n"
