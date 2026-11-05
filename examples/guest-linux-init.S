/* Minimal AArch64 Linux userspace witness for REVERSE's real kernel probe.
 * Not an Android runtime, shell, graphics driver or replacement for XI/Ether.
 * Linux syscall ABI only: no libc or linked runtime.
 */
.text
.p2align 2
.global _start
.type _start, %function
_start:
    mov x0, #1                 // stdout inherited from /dev/console
    adr x1, message
    mov x2, #message_end - message
    mov x8, #64                // __NR_write
    svc #0
    cmp x0, #message_end - message
    b.ne failed
sleep_forever:
    adr x0, sleep_interval
    mov x1, xzr
    mov x8, #101               // __NR_nanosleep
    svc #0
    b sleep_forever
failed:
    mov x0, #77
    mov x8, #93                // __NR_exit
    svc #0
1:  b 1b
.size _start, . - _start

.section .rodata
message:
    .ascii "REVERSE_LINUX_INIT_OK\n"
message_end:
.p2align 3
sleep_interval:
    .quad 0                   // seconds
    .quad 100000000           // nanoseconds
