#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/verify-reverseandroid-import.py
test -f build/ether-compat/mundo_x/quantum_runtime.hpp
test -f build/softfloat/libreverse_softfloat.a
test ! -e build/gpu-resource-trace-src
test ! -e build/gpu-resource-control-src
python3 scripts/prepare-gpu-resource-trace.py build/gpu-resource-trace-src > build/gpu-resource-trace-prepare.json
python3 scripts/prepare-ic-ivau-extension.py build/gpu-resource-control-src > build/gpu-resource-control-prepare.json
mkdir -p build/gpu-resource-trace-tests
flags=(-std=c++17 -pthread -DREVERSE_PC_HAVE_BOUNDED_PRODUCT_IMPORT=1
  -DREVERSE_PC_NUMERIC_BYTE_STORES=1 -DREVERSE_PC_COMPACT_INDEX_LOOKASIDE=1
  -Ibuild/ether-compat -Ideps/mundo-x/native/include -Isrc -Itools)
for variant in control trace; do
  g++ -I"build/gpu-resource-$variant-src" "${flags[@]}" -O2 \
    tests/reverseandroid_gpu_resource_trace_test.cpp build/softfloat/libreverse_softfloat.a \
    -o "build/gpu-resource-trace-tests/test-$variant"
  "build/gpu-resource-trace-tests/test-$variant" "build/gpu-resource-trace-tests/$variant.rvg" \
    > "build/gpu-resource-trace-tests/$variant.json" 2> "build/gpu-resource-trace-tests/$variant.log"
done
cmp build/gpu-resource-trace-tests/control.rvg build/gpu-resource-trace-tests/trace.rvg
cmp build/gpu-resource-trace-tests/control.json build/gpu-resource-trace-tests/trace.json
python3 scripts/verify-gpu-resource-fixture-trace.py build/gpu-resource-trace-tests
g++ -Ibuild/gpu-resource-trace-src "${flags[@]}" -O3 \
  -DREVERSE_PC_COMPACT_RESTORE_VIEW=1 -DREVERSE_PC_OPENSSL_DISK_SHA=1 \
  build/gpu-resource-trace-src/guest_reverseandroid_gpu_trace.cpp \
  build/softfloat/libreverse_softfloat.a -lcrypto -o build/gpu-resource-trace-tests/epoch-gpu-trace
g++ -Ibuild/gpu-resource-control-src "${flags[@]}" -O2 \
  tests/minimal_guest_pci_test.cpp build/softfloat/libreverse_softfloat.a \
  -o build/gpu-resource-trace-tests/test-original-pci
build/gpu-resource-trace-tests/test-original-pci > build/gpu-resource-trace-tests/original-pci.json
python3 scripts/verify-reverseandroid-import.py
