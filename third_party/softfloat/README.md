# Berkeley SoftFloat 3e, ARM-VFPv2 specialization

Upstream: https://github.com/ucb-bar/berkeley-softfloat-3
Pinned revision: `0a5ad290c6b287f6605bb99a35ad0842c1e61aa5`.
API reference: https://www.jhauser.us/arithmetic/SoftFloat-3/doc/SoftFloat.html

This directory contains the unmodified C sources needed for binary16 format
conversion and binary32/binary64 operations. `UPSTREAM.json` lists every selected
upstream file and its SHA-256. Both CMake and the standalone build script verify
those hashes. The original redistribution terms are in `COPYING.txt` and in each
source file; Android includes the same notice in `assets/licenses/softfloat.txt`.

`platform.h`, `CMakeLists.txt` and this README are REVERSE integration files.
C11 `_Thread_local` / C++ `thread_local` isolate rounding mode and exception flags
between threads. No host float arithmetic or fast-math is used. The C library is
arithmetic support, not another CPU interpreter or another memory/quantum engine.
ARM architectural register handling, FPCR/FPSR, flushing and instruction decoding
are implemented in REVERSE and checked separately against QEMU.
