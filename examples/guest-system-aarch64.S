// Own bare-metal witness, not Linux or Android. Fixed load address 0x40080000.
// Exercises a real page-table walk, timer IRQ, ERET to EL0, SVC and a denied
// write. QEMU is an independent test oracle only; it is not used by REVERSE.
.section .text
.global _start
_start:
    msr daifset, #15
    mov x0, #0x41000000
    mov sp, x0
    adr x0, .Lvectors
    msr vbar_el1, x0
    mov x20, #0x09000000
    mov x21, #0x08000000
    mov x22, #0x08010000
    mov x23, #0
    mov x24, #0
    // A 39-bit TTBR0/TTBR1 with 1 GiB block mappings.
    mov x0, #0x40010000
    mov x1, #0x401
    str x1, [x0]                 // Device space, EL1 only.
    mov x1, #0x705
    movk x1, #0x4000, lsl #16
    str x1, [x0, #8]             // RAM, EL1 RW only; executable at EL1.
    mov x1, #0x7c5
    movk x1, #0x4000, lsl #16
    str x1, [x0, #16]            // EL0 read-only code/data alias; write must abort.
    msr ttbr0_el1, x0
    msr ttbr1_el1, x0
    mov x1, #0xff00
    msr mair_el1, x1
    mov x1, #0x19
    movk x1, #0x8019, lsl #16
    msr tcr_el1, x1
    dsb sy
    // Establish the documented v8 baseline, rather than inherit firmware's
    // write/execute policy from a different machine's reset path.
    mov x1, #0x801
    movk x1, #0x30d0, lsl #16
    msr sctlr_el1, x1
    isb
    // Enable the physical timer's PPI in a single-group GICv2.
    mov w0, #1
    str w0, [x21]
    str w0, [x22]
    mov w0, #255
    str w0, [x22, #4]
    mov w0, #0x40000000
    str w0, [x21, #0x100]
    mov x0, #1000
    msr cntp_tval_el0, x0
    mov x0, #1
    msr cntp_ctl_el0, x0
.Lwait_irq:
    // Mask IRQ while checking the completion flag. WFI still wakes for a
    // pending IRQ; unmask afterwards. This also works if the timer expired
    // before WFI on a differently paced, independent execution engine.
    msr daifset, #2
    cbnz x23, .Lirq_done
    wfi
    msr daifclr, #2
    b .Lwait_irq
.Lirq_done:
    msr daifclr, #2
    // Enter EL0. The user cannot write PL011 or read/write system registers.
    mov x0, #0x1000
    movk x0, #0x4100, lsl #16
    msr sp_el0, x0
    adr x0, .Luser
    mov x1, #0x40000000
    add x0, x0, x1              // Execute EL0 through its read-only alias.
    msr elr_el1, x0
    msr spsr_el1, xzr
    eret
.Luser:
    mov x0, #7
    svc #42
    cmp x0, #42
    b.ne .Luser_fail
    mov x1, #0x9000
    movk x1, #0x8000, lsl #16
    str x0, [x1]                // Must trap before any physical byte changes.
    svc #43
.Luser_fail:
    svc #44
    b .Luser_fail
.Lkernel_done:
    cmp x23, #1
    b.ne .Lfail
    cmp x24, #1
    b.ne .Lfail
    mov x1, #0x9000
    movk x1, #0x4000, lsl #16
    ldr x0, [x1]
    cbnz x0, .Lfail
    mov x0, #42
    str x0, [x1, #8]
    adr x1, .Lmessage
.Lprint:
    ldrb w0, [x1], #1
    cbz w0, .Loff
    strb w0, [x20]
    b .Lprint
.Loff:
    mov x0, #8
    movk x0, #0x8400, lsl #16
    hvc #0
    b .Loff
.Lfail:
    mov w0, #70
    strb w0, [x20]
    b .Loff
.Lsync:
    mrs x0, esr_el1
    lsr x1, x0, #26
    cmp x1, #0x15
    b.eq .Lsvc
    cmp x1, #0x24
    b.ne .Lfail
    and x0, x0, #0x7f
    cmp x0, #0x4d
    b.ne .Lfail
    mrs x0, far_el1
    mov x1, #0x9000
    movk x1, #0x8000, lsl #16
    cmp x0, x1
    b.ne .Lfail
    mov x24, #1
    mrs x0, elr_el1
    add x0, x0, #4
    msr elr_el1, x0
    eret
.Lsvc:
    and x0, x0, #0xffff
    cmp x0, #42
    b.ne .Lreturn_kernel
    mov x0, #42
    eret
.Lreturn_kernel:
    cmp x0, #43
    b.ne .Lfail
    adr x0, .Lkernel_done
    msr elr_el1, x0
    mov x0, #0x3c5
    msr spsr_el1, x0
    eret
.Lirq:
    ldr w0, [x22, #0xc]
    cmp w0, #30
    b.ne .Lfail
    msr cntp_ctl_el0, xzr
    str w0, [x22, #0x10]
    mov x23, #1
    eret
.balign 2048
.Lvectors:
    b .Lfail
    .balign 128
    b .Lirq
    .balign 128
    b .Lfail
    .balign 128
    b .Lfail
    .balign 128
    b .Lsync
    .balign 128
    b .Lirq
    .balign 128
    b .Lfail
    .balign 128
    b .Lfail
    .balign 128
    b .Lsync
    .balign 128
    b .Lirq
    .balign 128
    b .Lfail
    .balign 128
    b .Lfail
    .balign 128
    b .Lfail
    .balign 128
    b .Lfail
    .balign 128
    b .Lfail
    .balign 128
    b .Lfail
    .balign 128
.Lmessage:
    .asciz "REVERSE: EL1 MMU IRQ EL0 SVC FAULT OK\n"
