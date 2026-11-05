// Compile: clang --target=aarch64-linux-gnu -fuse-ld=lld -nostdlib -static \
//   -Wl,--build-id=none -o guest-aarch64.elf examples/guest-aarch64.S
.text
.global _start
_start:
  mov x0, #6
  mov x1, #7
  mul x2, x0, x1
  adr x3, guest_result
  str x2, [x3]
  mov x0, #1
  adr x1, message
  mov x2, #message_end-message
  mov x8, #64
  svc #0
  mov x0, #0
  mov x8, #93
  svc #0
message:
  .ascii "REVERSE ARM64: 6*7 = 42\n"
message_end:
.bss
.balign 8
.global guest_result
guest_result:
  .skip 8
