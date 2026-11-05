#!/usr/bin/env bash
# Build-only transport: bring the owner's unchanged BBY Q XI programs into
# the REVERSE OS APK assets. Android runtime does NOT fetch from the internet.
# No AI or language-model algorithm is implemented by this shell script.
set -euo pipefail
bbyq="${1:?Expected checked-out BBY-Q path}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
target="$repo_root/reverse-os-android/app/src/main/assets/bbyq"
expected="7dfa6c31247cf609128db78908313a6ce41c0b80"
actual="$(git -C "$bbyq" rev-parse HEAD)"
if [[ "$actual" != "$expected" ]]; then
    echo "Unexpected BBY-Q head $actual; expected $expected" >&2
    exit 1
fi
for program in \
    e006/birth e006/seed-42 e006/feed-xor e006/learn \
    e006/validate-feed-xor e008/admit e008/open-shadow \
    e008/train-xnor e008/train-and e008/train-or \
    e008/admit-feed-xnor e008/admit-feed-and e008/admit-feed-or \
    e008/bank-store e008/audit-organism \
    language/birth language/ground language/relate language/calibrate \
    language/readline language/ask language/reflect language/speak language/audit; do
    source="$bbyq/ai/xi/$program.xi"
    output="$target/$program.xi"
    test -s "$source"
    mkdir -p "$(dirname "$output")"
    cp "$source" "$output"
done
printf '%s\n' "$expected" > "$target/source-sha.txt"
test -s "$target/curriculum-v1.txt"
echo "BBYQ_XI_BUNDLE_READY owner_sha=$actual programs=$(find "$target" -name '*.xi' | wc -l)"
