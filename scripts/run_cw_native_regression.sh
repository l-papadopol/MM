#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-/tmp/madmodem-cw-native-regression}"
mkdir -p "$build_dir"
compiler="${CXX:-g++}"
"$compiler" -std=c++17 -O2 -Wall -Wextra -Werror -I"$root" \
  "$root/tests/CwNativeRegression.cpp" \
  "$root/modems/cw/skimmer/CwSkimmerEngine.cpp" \
  "$root/modems/cw/skimmer/SelectedToneCwTracker.cpp" \
  "$root/modems/cw/skimmer/CwBayesianDecoder.cpp" \
  -o "$build_dir/MadModemCwNativeRegression"
"$build_dir/MadModemCwNativeRegression"
