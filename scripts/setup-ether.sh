#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SHA="e2778d962af22f6f1a0794058dd4685fc2573966"
TARGET="${ROOT}/deps/mundo-x"
if [[ ! -d "${TARGET}/.git" ]]; then
  mkdir -p "${TARGET}"
  git -C "${TARGET}" init
  git -C "${TARGET}" remote add origin https://github.com/guadalupejuan001-code/mundo-X.git
fi
git -C "${TARGET}" fetch --no-tags --depth 1 origin "${SHA}"
git -C "${TARGET}" checkout --detach "${SHA}"
ACTUAL="$(git -C "${TARGET}" rev-parse HEAD)"
[[ "${ACTUAL}" == "${SHA}" ]] || { echo "Incorrect Mundo X revision" >&2; exit 1; }
echo "REVERSE_PC_UPSTREAM_PIN_PASS ${ACTUAL}"
