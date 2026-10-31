#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ $# -lt 2 || $# -gt 4 ]]; then
  echo "Usage: $0 WAV TONE_HZ [INITIAL_WPM] [INTERFERER_HZ]" >&2
  exit 2
fi
build_dir="${TMPDIR:-/tmp}/madmodem-cw-recorded-wav-probe"
mkdir -p "$build_dir"
compiler="${CXX:-g++}"
"$compiler" -std=c++17 -O2 -Wall -Wextra -Werror -I"$root" \
  "$root/tests/CwRecordedWavProbe.cpp" \
  "$root/modems/cw/skimmer/CwCarrierDiscriminator.cpp" \
  "$root/modems/cw/skimmer/CwMorseBeamDecoder.cpp" \
  "$root/modems/cw/skimmer/CwRelativeTimingDecoder.cpp" \
  "$root/modems/cw/skimmer/CwRelativeTimingTask.cpp" \
  "$root/modems/cw/skimmer/SelectedToneCwTracker.cpp" \
  -pthread -o "$build_dir/CwRecordedWavProbe"
"$build_dir/CwRecordedWavProbe" "$@"
