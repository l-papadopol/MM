#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

for file in \
  modems/cw/skimmer/CwCarrierDiscriminator.h \
  modems/cw/skimmer/CwCarrierDiscriminator.cpp \
  modems/cw/skimmer/CwMorseBeamDecoder.h \
  modems/cw/skimmer/CwMorseBeamDecoder.cpp \
  modems/cw/skimmer/CwRelativeTimingDecoder.h \
  modems/cw/skimmer/CwRelativeTimingDecoder.cpp \
  modems/cw/skimmer/CwRelativeTimingTask.h \
  modems/cw/skimmer/CwRelativeTimingTask.cpp; do
  test -f "$root/$file" || { echo "FAIL missing $file" >&2; exit 1; }
done

for old in CwBayesianDecoder CwCausalSemiMarkovDecoder CwGeometricEdgeWorker; do
  if find "$root/modems/cw" -type f -name "${old}.*" | grep -q .; then
    echo "FAIL obsolete decoder source remains: $old" >&2
    exit 1
  fi
  if grep -Fq "$old" "$root/CMakeLists.txt"; then
    echo "FAIL obsolete decoder remains in CMake: $old" >&2
    exit 1
  fi
done

grep -Fq 'std::thread m_worker' "$root/modems/cw/skimmer/CwRelativeTimingTask.h"
grep -Fq 'Threads::Threads' "$root/CMakeLists.txt"
grep -Fq 'CwMorseBeamDecoder m_beamDecoder' \
  "$root/modems/cw/skimmer/CwRelativeTimingDecoder.h"
grep -Fq 'm_btnShowRuntimeLog->setVisible(ft8Mode || msk144Mode || q65Mode || cwMode)' \
  "$root/mainwindow.cpp"
grep -Fq 'runtime_log_cw_tooltip' "$root/translations/ui_en.ini"

"$root/scripts/run_cw_native_regression.sh"
echo "CW relative-timing clean-restart audit: PASS"
