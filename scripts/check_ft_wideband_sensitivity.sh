#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CPP="$ROOT/modems/ft8/Ft8RxDecoder.cpp"
HDR="$ROOT/modems/ft8/Ft8RxDecoder.h"
DOC="$ROOT/docs/FT8_FT4_SENSITIVITY_REGRESSION_FIX_0_5_78.md"

need() {
    local pattern="$1" file="$2" label="$3"
    if ! grep -Fq -- "$pattern" "$file"; then
        echo "FAIL: $label" >&2
        exit 1
    fi
}

need 'std::array<int, 103> *decodedTonesOut' "$HDR" 'FT4 decoded tones are not returned by the decoder'
need 'makeFt4ReferenceWaveformRx(decodedTones' "$CPP" 'FT4 exact GFSK SIC is missing'
need 'const bool sumProductAllowed = m_offlineAnalysisActive.load();' "$CPP" 'live SPA safety gate is missing'
need 'm_offlineAnalysisActive.load() &&' "$CPP" 'offline coherent A/B safety gate is missing'
need 'bucket rescue decoded/candidates' "$ROOT/mainwindow.cpp" 'wideband recovery telemetry is missing'
need 'Source-level error in the FT8 baseline port' "$DOC" 'regression diagnosis documentation is missing'

if grep -Fq 'rescuedCandidate.bucketRescue = true' "$CPP"; then
    echo 'FAIL: active bucket-rescue tail still displaces validated live candidates' >&2
    exit 1
fi
if grep -Fq 'subtractFt4DecodedSignal(QVector<double> &samples, const Candidate &candidate) const' "$HDR"; then
    echo 'FAIL: obsolete observed-tone FT4 SIC signature remains' >&2
    exit 1
fi

python3 "$ROOT/tests/ft8/check_wideband_algorithms.py"
echo 'FT sensitivity regression source audit: PASS'
