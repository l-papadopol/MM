#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

for file in \
  modems/cw/skimmer/CwMorseBeamDecoder.h \
  modems/cw/skimmer/CwMorseBeamDecoder.cpp \
  modems/cw/skimmer/CwRelativeTimingDecoder.h \
  modems/cw/skimmer/CwRelativeTimingDecoder.cpp \
  modems/cw/skimmer/SelectedToneCwTracker.cpp; do
  test -f "$root/$file" || { echo "FAIL missing $file" >&2; exit 1; }
done

grep -Fq 'CwMorseBeamDecoder.cpp' "$root/CMakeLists.txt"
grep -Fq 'maxReplayEvents' "$root/modems/cw/skimmer/CwMorseBeamDecoder.cpp"
grep -Fq 'collectStablePrefix' "$root/modems/cw/skimmer/CwMorseBeamDecoder.cpp"
grep -Fq 'timingConfidence' "$root/modems/cw/skimmer/CwMorseBeamDecoder.cpp"
grep -Fq 'carrierSessionProbability' "$root/modems/cw/skimmer/SelectedToneCwTracker.cpp"
grep -Fq 'learnedWordSec' "$root/modems/cw/skimmer/SelectedToneCwTracker.cpp"
! grep -Fq 'carrierQualifiedUntilSec' "$root/modems/cw/skimmer/SelectedToneCwTracker.cpp"
! grep -Fq '12.0 * ditSec' "$root/modems/cw/skimmer/SelectedToneCwTracker.cpp"
grep -Fq 'adaptive-beam-replay-first-dash' "$root/tests/CwNativeRegression.cpp"
grep -Fq 'live-30-long-word-gap' "$root/tests/CwNativeRegression.cpp"

"$root/scripts/run_cw_native_regression.sh"
echo "CW adaptive-beam live-path audit: PASS"
