/* SPDX-License-Identifier: Apache-2.0 */
#ifndef RA_CUTTLEFISH_PERIPHERAL_LINUX_ABI_H
#define RA_CUTTLEFISH_PERIPHERAL_LINUX_ABI_H
/* Linux AArch64 UAPI deliberately preserves ARM32 open flag assignments.
 * O_DIRECTORY and O_NOFOLLOW differ from x86/asm-generic. Independently checked
 * against torvalds/linux v6.12 arch/arm64/include/uapi/asm/fcntl.h. */
#define RA_ARM64_O_RDWR 00000002
#define RA_ARM64_O_CREAT 00000100
#define RA_ARM64_O_EXCL 00000200
#define RA_ARM64_O_NOCTTY 00000400
#define RA_ARM64_O_NONBLOCK 00004000
#define RA_ARM64_O_DIRECTORY 00040000
#define RA_ARM64_O_NOFOLLOW 00100000
#define RA_ARM64_O_CLOEXEC 02000000
#endif
