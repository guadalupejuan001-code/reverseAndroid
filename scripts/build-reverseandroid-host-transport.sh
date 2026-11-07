#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/verify-reverseandroid-import.py
python3 scripts/prepare-ether-compat.py deps/mundo-x build/ether-compat
python3 scripts/build-softfloat.py
python3 scripts/prepare-host-transport.py build/host-transport-runtime
mkdir -p build/host-transport-native
flags=(-std=c++17 -pthread -DREVERSE_PC_HAVE_BOUNDED_PRODUCT_IMPORT=1
  -DREVERSE_PC_NUMERIC_BYTE_STORES=1 -DREVERSE_PC_COMPACT_INDEX_LOOKASIDE=1
  -Ibuild/host-transport-runtime/src -Ibuild/ether-compat -Ideps/mundo-x/native/include -Isrc -Itools)
for item in 'guest_checkpoint_view_test decoder' 'guest_snapshot_transport_test host-policy' 'reverseandroid_ic_ivau_test ic-ivau' 'reverseandroid_clz_test clz' 'minimal_guest_system_test system'; do
  read -r source name <<< "$item"
  g++ "${flags[@]}" -O2 "tests/$source.cpp" build/softfloat/libreverse_softfloat.a -o "build/host-transport-native/test-$name"
  if [[ "$name" == decoder ]]; then
    "build/host-transport-native/test-$name" build/host-transport-native/decoder-data > "build/host-transport-native/$name.json"
  else
    "build/host-transport-native/test-$name" > "build/host-transport-native/$name.json"
  fi
done
g++ "${flags[@]}" -O3 -DREVERSE_PC_COMPACT_RESTORE_VIEW=1 -DREVERSE_PC_OPENSSL_DISK_SHA=1 \
  build/host-transport-runtime/src/guest_reverseandroid_epoch_host.cpp build/softfloat/libreverse_softfloat.a -lcrypto \
  -o build/host-transport-native/epoch-clz-ic-host
g++ "${flags[@]}" -O2 -DREVERSE_PC_COMPACT_RESTORE_VIEW=1 -DREVERSE_PC_OPENSSL_DISK_SHA=1 \
  tools/guest_checkpoint_frame.cpp build/softfloat/libreverse_softfloat.a -lcrypto -o build/host-transport-native/frame
cp build/host-transport-runtime/host-transport-manifest.json build/host-transport-native/host-transport-manifest.json
sha256sum build/host-transport-native/epoch-clz-ic-host build/host-transport-native/frame > build/host-transport-native/binaries.sha256
python3 scripts/verify-reverseandroid-import.py
