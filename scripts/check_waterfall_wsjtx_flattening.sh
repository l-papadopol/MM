#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LEVELER="$ROOT_DIR/dsp/WaterfallLeveler.cpp"
DSP="$ROOT_DIR/dsp/DspEngine.cpp"
WIDGET="$ROOT_DIR/widgets/WaterfallWidget.cpp"

require() {
    local pattern="$1" file="$2" message="$3"
    if ! grep -Fq -- "$pattern" "$file"; then
        echo "FAIL: $message" >&2
        exit 1
    fi
}
reject() {
    local pattern="$1" file="$2" message="$3"
    if grep -Fq -- "$pattern" "$file"; then
        echo "FAIL: $message" >&2
        exit 1
    fi
}

require "constexpr std::size_t kSegments = 10U" "$LEVELER" "ten WSJT-X lower-envelope segments missing"
require "quantile(values, 0.10)" "$LEVELER" "10th-percentile lower envelope missing"
require "std::array<double, 5> coefficients" "$LEVELER" "fourth-order polynomial fit missing"
require "stabilizeLocalLowerEnvelope" "$LEVELER" "steep-edge lower-residual stabilization missing"
require "stabilizeOneSidedEdgeAnchors" "$LEVELER" "one-sided physical-edge anchors missing"
require "residualBias" "$LEVELER" "sharp one-sided residual-bias correction missing"
require "quantile(residuals, 0.12)" "$LEVELER" "local lower-residual percentile missing"
require "result.baselineDb = lowerEnvelopePolynomial" "$LEVELER" "per-row baseline output missing"
require "const double flattenedDb = dbLine[x] - baselineDb" "$DSP" "baseline subtraction missing"
require "const double rawValue = 10.0 * flattenedDb" "$DSP" "fixed WSJT-X-style colour zero/gain input missing"
require "qPow(10.0, 0.015 * plotGain)" "$WIDGET" "WSJT-X exponential waterfall gain law missing"
require "const std::size_t validBegin = 0U" "$LEVELER" "full-band waterfall leveling invariant missing"
require "result.partialBand = false" "$LEVELER" "signal-dependent partial-band masking still possible"
reject "highReference" "$LEVELER" "relative passband detector still active"
reject "baseline[i] = dbLine[i]" "$LEVELER" "hard black outside-passband mask still active"
reject "m_floorCandidates" "$LEVELER" "legacy temporal floor history still active"
reject "maximumStep" "$LEVELER" "legacy temporal floor slew still active"

echo "WSJT-X-style waterfall flattening source audit: PASS"
