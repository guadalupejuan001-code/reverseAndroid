#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/verify-reverseandroid-import.py
python3 scripts/prepare-ether-compat.py deps/mundo-x build/ether-compat
python3 scripts/build-softfloat.py
python3 scripts/prepare-clz-extension.py build/clz-src
mkdir -p build/reverseandroid-clz
flags=(-std=c++17 -pthread -DREVERSE_PC_HAVE_BOUNDED_PRODUCT_IMPORT=1
  -DREVERSE_PC_NUMERIC_BYTE_STORES=1 -DREVERSE_PC_COMPACT_INDEX_LOOKASIDE=1
  -Ibuild/ether-compat -Ideps/mundo-x/native/include -Isrc -Itools)
g++ "${flags[@]}" -O2 tests/reverseandroid_clz_test.cpp build/softfloat/libreverse_softfloat.a -o build/reverseandroid-clz/test-baseline
build/reverseandroid-clz/test-baseline --expect-unsupported > build/reverseandroid-clz/baseline-refusal.json
g++ -Ibuild/clz-src "${flags[@]}" -O2 tests/reverseandroid_clz_test.cpp build/softfloat/libreverse_softfloat.a -o build/reverseandroid-clz/test-clz
build/reverseandroid-clz/test-clz > build/reverseandroid-clz/clz-test.json
if [[ "${1:-}" != --tests-only ]]; then
  g++ -Ibuild/clz-src "${flags[@]}" -O3 -DREVERSE_PC_COMPACT_RESTORE_VIEW=1 -DREVERSE_PC_OPENSSL_DISK_SHA=1 \
    tools/guest_reverseandroid_epoch.cpp build/softfloat/libreverse_softfloat.a -lcrypto -o build/reverseandroid-clz/epoch-clz
  sha256sum build/reverseandroid-clz/epoch-clz > build/reverseandroid-clz/epoch-clz.sha256
fi
cp build/clz-src/extension-manifest.json build/reverseandroid-clz/extension-manifest.json
python3 scripts/verify-reverseandroid-import.py
