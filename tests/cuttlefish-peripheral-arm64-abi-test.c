/* SPDX-License-Identifier: Apache-2.0 */
#include <stdio.h>
#include "../native/cuttlefish-peripheral-linux-abi.h"
/* Unmodified upstream AArch64 kernel header; it overrides host generic flags.
 * The fixture is a primary-source reference, not generated from peer code. */
#include "fixtures/linux-v6.12-arm64-fcntl.h"
_Static_assert(RA_ARM64_O_RDWR == O_RDWR,"AArch64 O_RDWR");
_Static_assert(RA_ARM64_O_CREAT == O_CREAT,"AArch64 O_CREAT");
_Static_assert(RA_ARM64_O_EXCL == O_EXCL,"AArch64 O_EXCL");
_Static_assert(RA_ARM64_O_NOCTTY == O_NOCTTY,"AArch64 O_NOCTTY");
_Static_assert(RA_ARM64_O_NONBLOCK == O_NONBLOCK,"AArch64 O_NONBLOCK");
_Static_assert(RA_ARM64_O_DIRECTORY == O_DIRECTORY,"AArch64 O_DIRECTORY differs from x86");
_Static_assert(RA_ARM64_O_NOFOLLOW == O_NOFOLLOW,"AArch64 O_NOFOLLOW differs from x86");
_Static_assert(RA_ARM64_O_CLOEXEC == O_CLOEXEC,"AArch64 O_CLOEXEC");
int main(void) {
  printf("{\"official_AArch64_UAPI_flags_match\":true,\"O_DIRECTORY\":%d,\"O_NOFOLLOW\":%d}\n",O_DIRECTORY,O_NOFOLLOW);
  return 0;
}
