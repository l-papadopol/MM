#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

for file in \
  modems/cw/skimmer/CwMorseBeamDecoder.h \
  modems/cw/skimmer/CwMorseBeamDecoder.cpp \
  modems/cw/skimmer/CwRelativeTimingDecoder.h \
  modems/cw/skimmer/CwRelativeTimingDecoder.cpp \
  modems/cw/skimmer/CwRelativeTimingTask.h \
  modems/cw/skimmer/CwRelativeTimingTask.cpp \
  modems/cw/skimmer/SelectedToneCwTracker.cpp \
  tests/CwNativeRegression.cpp; do
  test -f "$root/$file" || { echo "FAIL missing $file" >&2; exit 1; }
done

grep -Fq 'void beginEpoch(const CwMorseTimingSnapshot& continuityTiming' \
  "$root/modems/cw/skimmer/CwMorseBeamDecoder.h"
grep -Fq 'm_beamDecoder.beginEpoch(continuity, 0.30)' \
  "$root/modems/cw/skimmer/CwRelativeTimingDecoder.cpp"
grep -Fq 'const bool largeScaleChange = geometricScale <= 0.76' \
  "$root/modems/cw/skimmer/CwRelativeTimingDecoder.cpp"
grep -Fq 'timingTask.beginEpoch(acquisitionShortMs, acquisitionLongMs' \
  "$root/modems/cw/skimmer/SelectedToneCwTracker.cpp"
grep -Fq 'model=" << (result.temporalModel == 0 ? "LOCAL" : "CONT")' \
  "$root/modems/cw/skimmer/SelectedToneCwTracker.cpp"
grep -Fq 'same-lane-abrupt-speed-change' \
  "$root/tests/CwNativeRegression.cpp"
grep -Fq 'live-same-lane-two-operators' \
  "$root/tests/CwNativeRegression.cpp"

"$root/scripts/run_cw_native_regression.sh"
echo "CW temporal-epoch live-path audit: PASS"
