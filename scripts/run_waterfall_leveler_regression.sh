#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-${MADMODEM_WATERFALL_TEST_BUILD_DIR:-/tmp/madmodem-waterfall-leveler-regression}}"
CXX_BIN="${CXX:-g++}"
mkdir -p "${BUILD_DIR}"

"${CXX_BIN}" -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror \
    -I"${ROOT_DIR}" \
    "${ROOT_DIR}/tests/WaterfallLevelerRegression.cpp" \
    "${ROOT_DIR}/dsp/WaterfallLeveler.cpp" \
    -o "${BUILD_DIR}/WaterfallLevelerRegression"

"${BUILD_DIR}/WaterfallLevelerRegression"
