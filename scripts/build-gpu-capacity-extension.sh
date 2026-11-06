#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/verify-reverseandroid-import.py
test "$(git -C deps/qssd rev-parse HEAD)" = c9dc0ebf3370a4468e14df92fcebb2a90954c448
git -C deps/qssd diff --exit-code -- include/qssd/state_bus.hpp tests/state_bus.cpp
python3 scripts/prepare-gpu-capacity-extension.py build/gpu-capacity-src > build/gpu-capacity-prepare.json
python3 scripts/prepare-ic-ivau-extension.py build/gpu-capacity-control-src > build/gpu-capacity-control-prepare.json
mkdir -p build/gpu-capacity
flags=(-std=c++17 -pthread -DREVERSE_PC_HAVE_BOUNDED_PRODUCT_IMPORT=1
  -DREVERSE_PC_NUMERIC_BYTE_STORES=1 -DREVERSE_PC_COMPACT_INDEX_LOOKASIDE=1
  -Ibuild/ether-compat -Ideps/mundo-x/native/include -Ideps/qssd/include -Iextensions -Isrc -Itools)
g++ -Ibuild/gpu-capacity-control-src "${flags[@]}" -O2 tests/reverseandroid_gpu_capacity_test.cpp \
  build/softfloat/libreverse_softfloat.a -o build/gpu-capacity/test-control
build/gpu-capacity/test-control build/gpu-capacity/control.rvg > build/gpu-capacity/control.json
g++ -Ibuild/gpu-capacity-src "${flags[@]}" -O2 -DREVERSE_GPU_CAPACITY_CANDIDATE=1 \
  tests/reverseandroid_gpu_capacity_test.cpp build/softfloat/libreverse_softfloat.a -o build/gpu-capacity/test-candidate
build/gpu-capacity/test-candidate build/gpu-capacity/candidate.rvg > build/gpu-capacity/candidate.json
build/gpu-capacity/test-control --reject-extended build/gpu-capacity/candidate.rvg > build/gpu-capacity/old-decoder-refusal.json
for variant in control candidate; do
  src=build/gpu-capacity-control-src
  if [[ "$variant" == candidate ]]; then src=build/gpu-capacity-src; fi
  g++ -I"$src" "${flags[@]}" -O2 tests/reverseandroid_gpu_resource_trace_test.cpp \
    build/softfloat/libreverse_softfloat.a -o "build/gpu-capacity/legacy-$variant"
  "build/gpu-capacity/legacy-$variant" "build/gpu-capacity/legacy-$variant.rvg" > "build/gpu-capacity/legacy-$variant.json"
done
cmp build/gpu-capacity/legacy-control.rvg build/gpu-capacity/legacy-candidate.rvg
cmp build/gpu-capacity/legacy-control.json build/gpu-capacity/legacy-candidate.json
g++ -Ibuild/gpu-capacity-src "${flags[@]}" -O2 tests/reverseandroid_gpu_state_bridge_test.cpp \
  build/softfloat/libreverse_softfloat.a -o build/gpu-capacity/test-qssd-bridge
build/gpu-capacity/test-qssd-bridge > build/gpu-capacity/qssd-bridge.json
g++ -std=c++17 -O2 -pthread -Ideps/qssd/include deps/qssd/tests/state_bus.cpp -o build/gpu-capacity/test-original-qssd
build/gpu-capacity/test-original-qssd > build/gpu-capacity/original-qssd.json
g++ -Ibuild/gpu-capacity-src "${flags[@]}" -O2 tests/minimal_guest_pci_test.cpp \
  build/softfloat/libreverse_softfloat.a -o build/gpu-capacity/test-original-pci
build/gpu-capacity/test-original-pci > build/gpu-capacity/original-pci.json
for variant in control candidate; do
  src=build/gpu-capacity-control-src
  name=epoch-control
  if [[ "$variant" == candidate ]]; then src=build/gpu-capacity-src; name=epoch; fi
  python3 scripts/prepare-gpu-resource-trace.py "$src" --prepared > "build/gpu-capacity/$variant-trace-prepare.json"
  g++ -I"$src" "${flags[@]}" -O3 \
    -DREVERSE_PC_COMPACT_RESTORE_VIEW=1 -DREVERSE_PC_OPENSSL_DISK_SHA=1 \
    "$src/guest_reverseandroid_gpu_trace.cpp" build/softfloat/libreverse_softfloat.a -lcrypto -o "build/gpu-capacity/$name"
done
g++ -Ibuild/gpu-capacity-src "${flags[@]}" -O3 \
  -DREVERSE_PC_COMPACT_RESTORE_VIEW=1 -DREVERSE_PC_OPENSSL_DISK_SHA=1 \
  tools/guest_checkpoint_frame.cpp build/softfloat/libreverse_softfloat.a -lcrypto -o build/gpu-capacity/frame
cp build/gpu-capacity-src/gpu-capacity-manifest.json build/gpu-capacity/
python3 scripts/verify-reverseandroid-import.py
