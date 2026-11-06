#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_mode=${1:?instrument or use}
task_build=build/reverseandroid/pgo-eval
task_profile="$(pwd)/$task_build/profile"
mkdir -p "$task_build" "$task_profile"
python3 scripts/verify-reverseandroid-import.py
flags=(-std=c++17 -pthread -DREVERSE_PC_HAVE_BOUNDED_PRODUCT_IMPORT=1
  -DREVERSE_PC_NUMERIC_BYTE_STORES=1 -DREVERSE_PC_COMPACT_INDEX_LOOKASIDE=1
  -DREVERSE_PC_COMPACT_RESTORE_VIEW=1 -DREVERSE_PC_OPENSSL_DISK_SHA=1
  -Ibuild/ether-compat -Ideps/mundo-x/native/include -Isrc -Itools -O3)
case "$task_mode" in
  instrument) flags+=("-fprofile-generate=$task_profile" -fprofile-update=atomic) ;;
  use)
    python3 - "$task_profile" <<'PY'
import pathlib,sys
assert list(pathlib.Path(sys.argv[1]).rglob('*.gcda')), 'Missing actual execution profile'
PY
    flags+=("-fprofile-use=$task_profile" -Werror=missing-profile) ;;
  *) exit 2 ;;
esac
g++ "${flags[@]}" tools/guest_reverseandroid_epoch.cpp build/softfloat/libreverse_softfloat.a -lcrypto -o "$task_build/epoch-pgo"
if [ "$task_mode" = instrument ]; then cp "$task_build/epoch-pgo" "$task_build/epoch-pgo-instrumented"; fi
sha256sum "$task_build/epoch-pgo"
