#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/verify-reverseandroid-import.py
test -f build/softfloat/libreverse_softfloat.a
test -f build/ether-compat/mundo_x/quantum_runtime.hpp
flags=(-std=c++17 -pthread -DREVERSE_PC_HAVE_BOUNDED_PRODUCT_IMPORT=1
  -DREVERSE_PC_NUMERIC_BYTE_STORES=1 -DREVERSE_PC_COMPACT_INDEX_LOOKASIDE=1
  -DREVERSE_PC_COMPACT_RESTORE_VIEW=1 -DREVERSE_PC_OPENSSL_DISK_SHA=1
  -Ibuild/ether-compat -Ideps/mundo-x/native/include -Isrc -Itools)
g++ "${flags[@]}" -O2 tools/guest_reverseandroid_power_cycle.cpp build/softfloat/libreverse_softfloat.a -lcrypto -o build/reverseandroid/power-cycle
g++ "${flags[@]}" -O3 tools/guest_reverseandroid_epoch.cpp build/softfloat/libreverse_softfloat.a -lcrypto -o build/reverseandroid/epoch
g++ "${flags[@]}" -O2 tests/reverseandroid_power_cycle_test.cpp build/softfloat/libreverse_softfloat.a -lcrypto -o build/reverseandroid/test-power-cycle
build/reverseandroid/test-power-cycle > build/reverseandroid/power-cycle-test.json
node --test tests/reverseandroid_lights_profile.test.mjs > build/reverseandroid/lights-profile-tests.tap
g++ "${flags[@]}" -O2 tests/reverseandroid_console_event_test.cpp build/softfloat/libreverse_softfloat.a -lcrypto -o build/reverseandroid/test-console-event
build/reverseandroid/test-console-event > build/reverseandroid/console-event-test.json
node --test tests/reverseandroid_epoch_input.test.mjs > build/reverseandroid/epoch-input-tests.tap
sha256sum build/reverseandroid/power-cycle build/reverseandroid/epoch > build/reverseandroid/epoch-binaries-sha256.txt
