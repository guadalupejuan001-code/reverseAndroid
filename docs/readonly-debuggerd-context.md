# Read-only signal-context inspection

SystemServer7354 remains alive in the original C89 stage2. Its main thread waits on futex `0x76a6dcdaf4`; an independently read child backlink identifies TID7816 in the same thread group. PID7825 subsequently executes `crash_dump64`. Those observations alone do not identify a fatal signal or a requested dump.

The supplemental probe can request `--debuggerd-context-pid=7354`. Only the selected thread receives bounded reads of a possible debugger-thread prefix, a 128-byte siginfo, and a 456-byte AArch64 ucontext prefix. Both TIDs must match independent observations. Pointers, signal range, EL0 mode, executable PC and writable SP mappings must validate. Otherwise the candidate remains unverified. Raw bytes and hashes accompany any decoded candidate.

The private prefix layout is **not locked to this guest build**, so decoding remains a candidate and never changes the original C46, live-process or boot observations. No guest instruction or UART input is executed. The original snapshot digest is verified before and after the supplemental observer.

Layout references: [AOSP debuggerd handler](https://android.googlesource.com/platform/system/core/+/refs/heads/main/debuggerd/handler/debuggerd_handler.cpp), blob `88278ca667e84f432125c791298bd70ad1e4deca`; [AArch64 bionic ucontext](https://android.googlesource.com/platform/bionic/+/bcfe3cf/libc/include/sys/ucontext.h). These explain the tested layout hypothesis, not the exact private implementation of the guest image.

Validation includes rejected unrelated futexes, unreadable or kernel pointers, invalid signals, privileged contexts and invalid code/stack mappings. All 870 imported original files remain unchanged.
