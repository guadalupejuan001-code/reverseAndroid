#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/verify-reverseandroid-import.py
python3 scripts/prepare-ether-compat.py deps/mundo-x build/ether-compat
python3 scripts/build-softfloat.py
python3 scripts/prepare-ic-ivau-extension.py build/ic-ivau-src
mkdir -p build/reverseandroid-ic-ivau
flags=(-std=c++17 -pthread -DREVERSE_PC_HAVE_BOUNDED_PRODUCT_IMPORT=1
  -DREVERSE_PC_NUMERIC_BYTE_STORES=1 -DREVERSE_PC_COMPACT_INDEX_LOOKASIDE=1
  -Ibuild/ether-compat -Ideps/mundo-x/native/include -Isrc -Itools)
g++ "${flags[@]}" -O2 tests/reverseandroid_ic_ivau_test.cpp build/softfloat/libreverse_softfloat.a -o build/reverseandroid-ic-ivau/test-baseline
build/reverseandroid-ic-ivau/test-baseline --baseline > build/reverseandroid-ic-ivau/baseline-IC-refusal.json
for item in 'reverseandroid_ic_ivau_test ic-ivau' 'minimal_guest_el0_cache_test prior-el0-cache' 'reverseandroid_clz_test retained-clz' 'minimal_guest_system_test prior-system'; do
  read -r source name <<< "$item"
  g++ -Ibuild/ic-ivau-src "${flags[@]}" -O2 "tests/$source.cpp" build/softfloat/libreverse_softfloat.a -o "build/reverseandroid-ic-ivau/test-$name"
  "build/reverseandroid-ic-ivau/test-$name" > "build/reverseandroid-ic-ivau/$name-test.json"
done
if [[ "${1:-}" != --tests-only ]]; then
  g++ -Ibuild/ic-ivau-src "${flags[@]}" -O3 -DREVERSE_PC_COMPACT_RESTORE_VIEW=1 -DREVERSE_PC_OPENSSL_DISK_SHA=1 \
    tools/guest_reverseandroid_epoch.cpp build/softfloat/libreverse_softfloat.a -lcrypto -o build/reverseandroid-ic-ivau/epoch-clz-ic
  sha256sum build/reverseandroid-ic-ivau/epoch-clz-ic > build/reverseandroid-ic-ivau/epoch-clz-ic.sha256
fi
cp build/ic-ivau-src/extension-manifest.json build/reverseandroid-ic-ivau/extension-manifest.json
python3 scripts/verify-reverseandroid-import.py
