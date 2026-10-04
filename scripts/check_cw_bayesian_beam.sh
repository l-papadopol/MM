#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

for file in \
  modems/cw/skimmer/CwMorseBeamDecoder.h \
  modems/cw/skimmer/CwMorseBeamDecoder.cpp \
  modems/cw/skimmer/CwCarrierDiscriminator.h \
  modems/cw/skimmer/CwCarrierDiscriminator.cpp \
  modems/cw/skimmer/CwRelativeTimingDecoder.h \
  modems/cw/skimmer/CwRelativeTimingDecoder.cpp \
  modems/cw/skimmer/SelectedToneCwTracker.cpp; do
  test -f "$root/$file" || { echo "FAIL missing $file" >&2; exit 1; }
done

grep -Fq 'CwMorseBeamDecoder.cpp' "$root/CMakeLists.txt"
grep -Fq 'DurationPosterior' "$root/modems/cw/skimmer/CwMorseBeamDecoder.h"
grep -Fq 'logPredictive' "$root/modems/cw/skimmer/CwMorseBeamDecoder.cpp"
grep -Fq 'credibleMass' "$root/modems/cw/skimmer/CwMorseBeamDecoder.cpp"
grep -Fq 'bestPosterior' "$root/modems/cw/skimmer/CwMorseBeamDecoder.cpp"
grep -Fq 'meanMarkProbability' "$root/modems/cw/skimmer/CwCarrierDiscriminator.cpp"
grep -Fq 'stateProbability' "$root/modems/cw/skimmer/CwRelativeTimingDecoder.cpp"
grep -Fq ' bayes=' "$root/modems/cw/skimmer/SelectedToneCwTracker.cpp"
grep -Fq 'bayesian-posterior-text' "$root/tests/CwNativeRegression.cpp"
grep -Fq 'live-30-long-word-gap' "$root/tests/CwNativeRegression.cpp"

"$root/scripts/run_cw_native_regression.sh"
echo "CW Bayesian-beam live-path audit: PASS"
