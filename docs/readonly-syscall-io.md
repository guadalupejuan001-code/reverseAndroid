# Read-only syscall context

Source13's independently observed SystemServer main frame reached `read()`. Source15's original kernel stack subsequently reached `readlinkat` and `proc_self_get_link`. The supplemental observer can identify the pathname or descriptor involved without sending a guest command.

For the explicitly selected PID, the probe uses the original saved syscall argument from `pt_regs.orig_x0`. File-table capacity, slot and kernel pointer checks precede BTF-derived file position, inode mode/size and mounted path reads. The observer does not read the file's contents or advance its offset. For pathname syscalls it reads at most4096 bytes, stopping at the first NUL and respecting AArch64 pointer tagging. A `/proc/self/fd/N` pathname can be linked to that process's validated descriptor. Directory descriptors retain signed32-bit semantics.

Closed slots, invalid bounds, unavailable mappings and undecodable contexts remain explicit. An observed saved frame is recorded as context and does not identify an unobserved private Java method. The full original snapshot digest and all870 source files remain unchanged after inspection.

Tests cover position/identity without content reads, closed or out-of-range descriptors, original arguments versus return values, tagged paths, negative directory descriptors, unterminated paths and short memory reads.
