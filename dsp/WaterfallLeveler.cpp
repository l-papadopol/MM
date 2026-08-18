#include "WaterfallLeveler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {

double clampValue(double value, double low, double high)
{
    return std::max(low, std::min(high, value));
}

double quantile(std::vector<double> values, double q)
{
    if (values.empty()) return -130.0;
    q = clampValue(q, 0.0, 1.0);
    const std::size_t index = static_cast<std::size_t>(
        std::llround(q * static_cast<double>(values.size() - 1U)));
    std::nth_element(values.begin(),
                     values.begin() + static_cast<std::ptrdiff_t>(index),
                     values.end());
    return values[index];
}

bool solveFiveByFive(std::array<std::array<double, 6>, 5> matrix,
                     std::array<double, 5> &solution)
{
    for (std::size_t column = 0; column < 5U; ++column) {
        std::size_t pivot = column;
        double pivotAbs = std::abs(matrix[pivot][column]);
        for (std::size_t row = column + 1U; row < 5U; ++row) {
            const double candidate = std::abs(matrix[row][column]);
            if (candidate > pivotAbs) {
                pivot = row;
                pivotAbs = candidate;
            }
        }
        if (pivotAbs < 1.0e-10 || !std::isfinite(pivotAbs)) return false;
        if (pivot != column) std::swap(matrix[pivot], matrix[column]);

        const double divisor = matrix[column][column];
        for (std::size_t j = column; j < 6U; ++j)
            matrix[column][j] /= divisor;

        for (std::size_t row = 0; row < 5U; ++row) {
            if (row == column) continue;
            const double factor = matrix[row][column];
            for (std::size_t j = column; j < 6U; ++j)
                matrix[row][j] -= factor * matrix[column][j];
        }
    }

    for (std::size_t i = 0; i < 5U; ++i) {
        solution[i] = matrix[i][5U];
        if (!std::isfinite(solution[i])) return false;
    }
    return true;
}

void stabilizeLocalLowerEnvelope(const std::vector<double> &dbLine,
                                 std::size_t begin,
                                 std::size_t end,
                                 std::vector<double> &baseline)
{
    if (dbLine.empty() || baseline.size() != dbLine.size() || end <= begin)
        return;

    const std::size_t width = end - begin + 1U;
    const std::size_t segmentCount = std::max<std::size_t>(8U,
        std::min<std::size_t>(24U, width / 32U));
    if (segmentCount < 2U) return;

    std::vector<double> centers;
    std::vector<double> corrections;
    centers.reserve(segmentCount);
    corrections.reserve(segmentCount);

    for (std::size_t segment = 0; segment < segmentCount; ++segment) {
        const std::size_t segmentBegin = begin + (segment * width) / segmentCount;
        std::size_t segmentEnd = begin + ((segment + 1U) * width) / segmentCount;
        if (segmentEnd > begin) --segmentEnd;
        segmentEnd = std::max(segmentBegin, std::min(segmentEnd, end));

        std::vector<double> residuals;
        residuals.reserve(segmentEnd - segmentBegin + 1U);
        for (std::size_t i = segmentBegin; i <= segmentEnd; ++i)
            residuals.push_back(dbLine[i] - baseline[i]);

        // The global fourth-order WSJT-X fit can undershoot near a steep
        // receiver/sound-card roll-off.  That turns the outer noise floor into
        // bright yellow bands even though no extra RF noise is present.  Align
        // the local lower residual, not the median or peak, so narrow signals
        // remain untouched.
        const double localFloor = quantile(residuals, 0.12);
        centers.push_back(0.5 * static_cast<double>(segmentBegin + segmentEnd));
        corrections.push_back(clampValue(localFloor, -5.0, 10.0));
    }

    if (corrections.size() >= 3U) {
        std::vector<double> smoothed = corrections;
        for (std::size_t i = 1U; i + 1U < corrections.size(); ++i)
            smoothed[i] = 0.25 * corrections[i - 1U] +
                          0.50 * corrections[i] +
                          0.25 * corrections[i + 1U];
        corrections.swap(smoothed);
    }

    std::size_t right = 1U;
    for (std::size_t i = begin; i <= end; ++i) {
        const double x = static_cast<double>(i);
        while (right + 1U < centers.size() && x > centers[right]) ++right;
        const std::size_t left = right > 0U ? right - 1U : 0U;
        double correction = corrections[left];
        if (right < centers.size() && centers[right] > centers[left]) {
            const double fraction = clampValue(
                (x - centers[left]) / (centers[right] - centers[left]), 0.0, 1.0);
            correction = corrections[left] +
                         fraction * (corrections[right] - corrections[left]);
        }
        baseline[i] += correction;
    }
}


void stabilizeOneSidedEdgeAnchors(const std::vector<double> &dbLine,
                                  std::size_t begin,
                                  std::size_t end,
                                  std::vector<double> &baseline)
{
    if (dbLine.empty() || baseline.size() != dbLine.size() || end <= begin)
        return;

    const std::size_t width = end - begin + 1U;
    const std::size_t edgeSpan = std::max<std::size_t>(24U,
        std::min<std::size_t>(128U, width / 7U));
    const std::size_t window = std::max<std::size_t>(12U,
        std::min<std::size_t>(48U, width / 32U));
    if (edgeSpan < 8U || width < 2U * window) return;

    auto residualFloor = [&](std::size_t centre) {
        const std::size_t half = window / 2U;
        const std::size_t localBegin = centre > begin + half ? centre - half : begin;
        const std::size_t localEnd = std::min(end, localBegin + window);
        std::vector<double> residuals;
        residuals.reserve(localEnd - localBegin + 1U);
        for (std::size_t i = localBegin; i <= localEnd; ++i)
            residuals.push_back(dbLine[i] - baseline[i]);
        return clampValue(quantile(residuals, 0.10), -8.0, 18.0);
    };

    auto applySide = [&](bool left) {
        std::array<std::size_t, 5> positions {};
        for (std::size_t k = 0; k < positions.size(); ++k) {
            const std::size_t distance = (k * edgeSpan) / (positions.size() - 1U);
            positions[k] = left ? begin + distance : end - distance;
        }

        std::array<double, 5> correction {};
        for (std::size_t k = 0; k < correction.size(); ++k)
            correction[k] = residualFloor(positions[k]);

        const double interiorReference = correction.back();
        for (double &value : correction)
            value = clampValue(value - interiorReference, -8.0, 18.0);

        std::array<double, 5> smoothed = correction;
        for (std::size_t i = 1U; i + 1U < correction.size(); ++i)
            smoothed[i] = 0.25 * correction[i - 1U] +
                          0.50 * correction[i] +
                          0.25 * correction[i + 1U];
        correction = smoothed;

        for (std::size_t distance = 0; distance <= edgeSpan; ++distance) {
            const double coordinate = static_cast<double>(distance) *
                                      static_cast<double>(correction.size() - 1U) /
                                      static_cast<double>(edgeSpan);
            const std::size_t lower = std::min<std::size_t>(
                correction.size() - 2U, static_cast<std::size_t>(coordinate));
            const double fraction = coordinate - static_cast<double>(lower);
            const double value = correction[lower] +
                                 fraction * (correction[lower + 1U] - correction[lower]);
            const std::size_t index = left ? begin + distance : end - distance;
            baseline[index] += value;
        }

        // If a very sharp one-sided receiver bend remains after the polynomial
        // and anchor interpolation, align the outer lower floor with the inner
        // edge floor.  This correction is gated by a real residual bias, so a
        // normal broad slope is left untouched, and uses a lower quartile so a
        // narrow carrier cannot become part of the baseline.
        const std::size_t outerCount = std::min<std::size_t>(32U, edgeSpan / 3U + 1U);
        std::vector<double> outerResiduals;
        outerResiduals.reserve(outerCount);
        for (std::size_t distance = 0; distance < outerCount; ++distance) {
            const std::size_t index = left ? begin + distance : end - distance;
            outerResiduals.push_back(dbLine[index] - baseline[index]);
        }

        const std::size_t centreHalf = std::max<std::size_t>(16U, width / 16U);
        const std::size_t centre = begin + width / 2U;
        const std::size_t centreBegin = centre > centreHalf ? centre - centreHalf : begin;
        const std::size_t centreEnd = std::min(end, centre + centreHalf);
        std::vector<double> centreResiduals;
        centreResiduals.reserve(centreEnd - centreBegin + 1U);
        for (std::size_t i = centreBegin; i <= centreEnd; ++i)
            centreResiduals.push_back(dbLine[i] - baseline[i]);

        const double outerFloor = quantile(outerResiduals, 0.50);
        const double centreFloor = quantile(centreResiduals, 0.50);
        const double residualBias = clampValue(outerFloor - centreFloor, -10.0, 14.0);
        if (std::abs(residualBias) > 3.25) {
            // The residual estimate is taken from the outermost bins.  Fade it
            // only through the sharp transition region; extending the same
            // correction across the whole edge span can darken an otherwise
            // flat shoulder after a symmetric passband roll-off.
            const std::size_t biasSpan = std::min<std::size_t>(
                edgeSpan, std::max<std::size_t>(48U, 2U * outerCount));
            for (std::size_t distance = 0; distance <= biasSpan; ++distance) {
                const double x = static_cast<double>(distance) /
                                 static_cast<double>(biasSpan);
                const double smoothStep = x * x * (3.0 - 2.0 * x);
                const double weight = 1.0 - smoothStep;
                const std::size_t index = left ? begin + distance : end - distance;
                baseline[index] += 1.05 * residualBias * weight;
            }
        }
    };

    applySide(true);
    applySide(false);
}

std::vector<double> lowerEnvelopePolynomial(const std::vector<double> &dbLine,
                                            std::size_t begin,
                                            std::size_t end)
{
    std::vector<double> baseline(dbLine.size(), -110.0);
    if (dbLine.empty()) return baseline;
    begin = std::min(begin, dbLine.size() - 1U);
    end = std::max(begin, std::min(end, dbLine.size() - 1U));
    const std::size_t width = end - begin + 1U;

    std::array<std::array<double, 6>, 5> normal {};
    std::size_t pointCount = 0U;
    constexpr std::size_t kSegments = 10U;

    for (std::size_t segment = 0; segment < kSegments; ++segment) {
        const std::size_t segmentBegin = begin + (segment * width) / kSegments;
        std::size_t segmentEnd = begin + ((segment + 1U) * width) / kSegments;
        if (segmentEnd > begin) --segmentEnd;
        segmentEnd = std::max(segmentBegin, std::min(segmentEnd, end));

        std::vector<double> values(dbLine.begin() + static_cast<std::ptrdiff_t>(segmentBegin),
                                   dbLine.begin() + static_cast<std::ptrdiff_t>(segmentEnd + 1U));
        // This is the exact lower-envelope principle used by WSJT-X flat4:
        // retain the lowest ten percent from each of ten frequency segments.
        const double threshold = quantile(values, 0.10);

        for (std::size_t i = segmentBegin; i <= segmentEnd; ++i) {
            if (dbLine[i] > threshold) continue;
            const double x = width > 1U
                ? (2.0 * static_cast<double>(i - begin) /
                   static_cast<double>(width - 1U)) - 1.0
                : 0.0;
            std::array<double, 9> powers {};
            powers[0] = 1.0;
            for (std::size_t p = 1U; p < powers.size(); ++p)
                powers[p] = powers[p - 1U] * x;
            for (std::size_t row = 0; row < 5U; ++row) {
                for (std::size_t column = 0; column < 5U; ++column)
                    normal[row][column] += powers[row + column];
                normal[row][5U] += dbLine[i] * powers[row];
            }
            ++pointCount;
        }
    }

    std::array<double, 5> coefficients {};
    const bool fitted = pointCount >= 20U && solveFiveByFive(normal, coefficients);
    const std::vector<double> valid(dbLine.begin() + static_cast<std::ptrdiff_t>(begin),
                                    dbLine.begin() + static_cast<std::ptrdiff_t>(end + 1U));
    const double fallback = quantile(valid, 0.10);

    for (std::size_t i = begin; i <= end; ++i) {
        if (!fitted) {
            baseline[i] = fallback;
            continue;
        }
        const double x = width > 1U
            ? (2.0 * static_cast<double>(i - begin) /
               static_cast<double>(width - 1U)) - 1.0
            : 0.0;
        double value = coefficients[4U];
        for (int p = 3; p >= 0; --p)
            value = value * x + coefficients[static_cast<std::size_t>(p)];
        // A pathological edge fit must not make the display flash.  The wide
        // clamp is only a numerical guard and does not act as a temporal AGC.
        baseline[i] = clampValue(value, fallback - 24.0, fallback + 24.0);
    }

    // Preserve the source-derived global flat4 shape, then remove only the
    // residual lower-floor error that appears at steep passband edges.
    stabilizeLocalLowerEnvelope(dbLine, begin, end, baseline);
    // A global polynomial and centre-to-centre local interpolation are least
    // constrained at the two physical ends of the selected spectrum. Add
    // explicit one-sided lower-envelope anchors so the last few percent cannot
    // become artificially hot when the receiver response bends sharply.
    stabilizeOneSidedEdgeAnchors(dbLine, begin, end, baseline);

    // MadModem 0.5.79 deliberately keeps the complete selected spectrum in
    // the colour-mapped history.  Older builds dynamically classified quiet
    // outer bins as an invalid passband and then forced those bins to exactly
    // zero after flattening.  A strong carrier could move that relative
    // threshold and make the visible waterfall appear to narrow/widen.
    // Full-band lower-envelope fitting avoids that signal-dependent geometry;
    // genuinely quiet receiver edges remain dark according to their local
    // noise statistics rather than becoming a hard black mask.
    return baseline;
}

} // namespace

void WaterfallLeveler::reset()
{
    // Per-row full-band normalization has no temporal state.
}

WaterfallLevelResult WaterfallLeveler::update(
    const std::vector<double> &dbLine, double elapsedSeconds)
{
    (void)elapsedSeconds; // flattening is intentionally per-row.
    WaterfallLevelResult result;
    if (dbLine.empty()) return result;

    // 0.5.79 invariant: display geometry never depends on instantaneous signal
    // amplitude.  Always level the complete selected spectrum.  The former
    // relative passband detector used the row's 90th percentile as a reference;
    // on radios with a steep/quiet VHF-UHF audio edge (for example TS-790) a
    // strong signal raised that reference enough to reclassify edge noise as
    // "outside" the passband.  Those bins were then forced to flattened value
    // zero, producing the apparent horizontal accordion/narrowing effect.
    const std::size_t validBegin = 0U;
    const std::size_t validEnd = dbLine.size() - 1U;

    result.baselineDb = lowerEnvelopePolynomial(dbLine, validBegin, validEnd);
    std::vector<double> representative(result.baselineDb.begin(),
                                       result.baselineDb.end());
    result.floorDb = quantile(representative, 0.50);
    result.ceilingDb = result.floorDb + 24.0;
    result.validBegin = validBegin;
    result.validEnd = validEnd;
    result.partialBand = false;
    return result;
}
