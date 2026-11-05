#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/verify-reverseandroid-import.py
python3 scripts/prepare-ether-compat.py deps/mundo-x build/ether-compat
python3 scripts/build-softfloat.py
mkdir -p build/reverseandroid
flags=(-std=c++17 -pthread -DREVERSE_PC_HAVE_BOUNDED_PRODUCT_IMPORT=1
  -DREVERSE_PC_NUMERIC_BYTE_STORES=1 -DREVERSE_PC_COMPACT_INDEX_LOOKASIDE=1
  -Ibuild/ether-compat -Ideps/mundo-x/native/include -Isrc -Itools)
g++ "${flags[@]}" -O3 -DREVERSE_PC_COMPACT_RESTORE_VIEW=1 -DREVERSE_PC_OPENSSL_DISK_SHA=1 \
  tools/guest_c27_profile_resume.cpp build/softfloat/libreverse_softfloat.a -lcrypto \
  -o build/reverseandroid/native
build/reverseandroid/native --pins > build/reverseandroid/native-pins.json
sha256sum build/reverseandroid/native > build/reverseandroid/binary-sha256.txt
for test in saturating_narrow high_narrow fp; do
  g++ "${flags[@]}" -O2 "tests/minimal_guest_${test}_test.cpp" \
    build/softfloat/libreverse_softfloat.a -o "build/reverseandroid/test-${test}"
  "build/reverseandroid/test-${test}" > "build/reverseandroid/${test}.json"
done
g++ "${flags[@]}" -O2 tests/guest_checkpoint_view_test.cpp \
  build/softfloat/libreverse_softfloat.a -o build/reverseandroid/test-decoder
build/reverseandroid/test-decoder build/reverseandroid/decoder-data > build/reverseandroid/original-decoder.json
node --test tests/android_c27_profile_resume.test.mjs tests/android_c42_zygote_progress.test.mjs \
  tests/android_c46_continue.test.mjs > build/reverseandroid/admission-tests.tap
echo 'Original C68 engine, narrowing instructions, decoder and continuation admission passed.'
