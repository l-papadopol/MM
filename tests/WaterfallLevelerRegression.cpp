#include "dsp/WaterfallLeveler.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<double> noiseLine(std::size_t count, double floorDb)
{
    std::vector<double> line(count, floorDb);
    for (std::size_t i = 0; i < line.size(); ++i)
        line[i] += 1.35 * std::sin(0.173 * static_cast<double>(i)) +
                   0.45 * std::cos(0.071 * static_cast<double>(i));
    return line;
}

double median(std::vector<double> values)
{
    if (values.empty()) return 0.0;
    const std::size_t middle = values.size() / 2U;
    std::nth_element(values.begin(),
                     values.begin() + static_cast<std::ptrdiff_t>(middle),
                     values.end());
    return values[middle];
}

double flattenedAt(const WaterfallLevelResult &result,
                   const std::vector<double> &line,
                   std::size_t index)
{
    return line[index] - result.baselineDb[index];
}

void printResult(const std::string &name, bool ok, double valueA,
                 double valueB = 0.0)
{
    std::cout << "WFREG\t" << name << "\t" << (ok ? "OK" : "FAIL")
              << "\tA=" << std::fixed << std::setprecision(2) << valueA
              << "\tB=" << valueB << '\n';
}

} // namespace

int main()
{
    constexpr std::size_t kBins = 768U;
    bool allOk = true;

    // TS-790-style V/U/SHF audio can have a much quieter noise floor at the
    // selected spectrum edges than in the centre.  A strong signal must never
    // change the *displayed width* by making a relative passband detector turn
    // those quiet-but-real bins into an exact black mask.
    WaterfallLeveler quietEdgeLeveler;
    std::vector<double> quietEdges = noiseLine(kBins, -86.0);
    for (std::size_t i = 0; i < quietEdges.size(); ++i) {
        const double edgeDistance = static_cast<double>(
            std::min(i, quietEdges.size() - 1U - i));
        if (edgeDistance < 150.0)
            quietEdges[i] -= 24.0 * (1.0 - edgeDistance / 150.0);
    }
    const WaterfallLevelResult quietBefore =
        quietEdgeLeveler.update(quietEdges, 0.050);

    std::vector<double> quietEdgesWithCarrier = quietEdges;
    for (std::size_t i = 378U; i <= 386U; ++i)
        quietEdgesWithCarrier[i] += 55.0 -
            2.0 * std::abs(static_cast<double>(i) - 382.0);
    const WaterfallLevelResult quietAfter =
        quietEdgeLeveler.update(quietEdgesWithCarrier, 0.050);

    std::vector<double> leftBefore;
    std::vector<double> rightBefore;
    std::vector<double> leftAfter;
    std::vector<double> rightAfter;
    for (std::size_t i = 0; i < 80U; ++i) {
        leftBefore.push_back(flattenedAt(quietBefore, quietEdges, i));
        leftAfter.push_back(flattenedAt(quietAfter, quietEdgesWithCarrier, i));
    }
    for (std::size_t i = kBins - 80U; i < kBins; ++i) {
        rightBefore.push_back(flattenedAt(quietBefore, quietEdges, i));
        rightAfter.push_back(flattenedAt(quietAfter, quietEdgesWithCarrier, i));
    }
    const double leftBeforeMedian = median(leftBefore);
    const double rightBeforeMedian = median(rightBefore);
    const double leftAfterMedian = median(leftAfter);
    const double rightAfterMedian = median(rightAfter);
    const double edgeChange = std::max(std::abs(leftAfterMedian - leftBeforeMedian),
                                       std::abs(rightAfterMedian - rightBeforeMedian));
    const bool fullWidthOk = !quietBefore.partialBand && !quietAfter.partialBand &&
                             quietBefore.validBegin == 0U &&
                             quietAfter.validBegin == 0U &&
                             quietBefore.validEnd == kBins - 1U &&
                             quietAfter.validEnd == kBins - 1U &&
                             leftBeforeMedian > 0.30 && rightBeforeMedian > 0.30 &&
                             leftAfterMedian > 0.30 && rightAfterMedian > 0.30 &&
                             edgeChange < 0.50;
    printResult("quiet-edges-strong-signal-keeps-full-width", fullWidthOk,
                edgeChange, flattenedAt(quietAfter, quietEdgesWithCarrier, 382U));
    allOk = fullWidthOk && allOk;

    // A whole-receiver AGC step must disappear in the very next row.  This is
    // the key behaviour inherited from WSJT-X flat4 and intentionally replaces
    // MadModem's former slow temporal floor slew.
    WaterfallLeveler stepLeveler;
    std::vector<double> before = noiseLine(kBins, -89.0);
    for (std::size_t i = 0; i < before.size(); ++i)
        before[i] += 8.0 * static_cast<double>(i) /
                     static_cast<double>(before.size() - 1U);
    before[318U] += 24.0;
    before[319U] += 31.0;
    const WaterfallLevelResult beforeResult = stepLeveler.update(before, 0.050);
    std::vector<double> after = before;
    for (double &value : after) value += 17.0;
    const WaterfallLevelResult afterResult = stepLeveler.update(after, 0.050);
    double maximumFlattenedDelta = 0.0;
    for (std::size_t i = 0; i < kBins; ++i) {
        maximumFlattenedDelta = std::max(maximumFlattenedDelta,
            std::abs(flattenedAt(beforeResult, before, i) -
                     flattenedAt(afterResult, after, i)));
    }
    const double baselineStep = afterResult.floorDb - beforeResult.floorDb;
    const bool stepOk = std::abs(baselineStep - 17.0) < 0.15 &&
                        maximumFlattenedDelta < 0.15;
    printResult("receiver-agc-step-cancelled-next-row", stepOk,
                baselineStep, maximumFlattenedDelta);
    allOk = stepOk && allOk;

    WaterfallLeveler slopeLeveler;
    std::vector<double> sloped = noiseLine(kBins, -91.0);
    for (std::size_t i = 0; i < sloped.size(); ++i) {
        const double x = static_cast<double>(i) /
                         static_cast<double>(sloped.size() - 1U);
        sloped[i] += 16.0 * x - 4.0 * x * x;
    }
    const WaterfallLevelResult slopeResult = slopeLeveler.update(sloped, 0.050);
    std::vector<double> segmentMedians;
    for (std::size_t segment = 0; segment < 10U; ++segment) {
        const std::size_t begin = (segment * kBins) / 10U;
        const std::size_t end = ((segment + 1U) * kBins) / 10U;
        std::vector<double> flattened;
        for (std::size_t i = begin; i < end; ++i)
            flattened.push_back(flattenedAt(slopeResult, sloped, i));
        segmentMedians.push_back(median(flattened));
    }
    const auto minmax = std::minmax_element(segmentMedians.begin(), segmentMedians.end());
    const double residualSlope = *minmax.second - *minmax.first;
    const bool slopeOk = !slopeResult.partialBand && residualSlope < 1.25;
    printResult("broad-receiver-slope-flattened", slopeOk,
                residualSlope, slopeResult.floorDb);
    allOk = slopeOk && allOk;

    WaterfallLeveler carrierLeveler;
    std::vector<double> withCarrier = noiseLine(kBins, -84.0);
    for (std::size_t i = 379U; i <= 385U; ++i)
        withCarrier[i] += 42.0 - 2.0 * std::abs(static_cast<double>(i) - 382.0);
    const WaterfallLevelResult carrierResult = carrierLeveler.update(withCarrier, 0.050);
    const double carrierProminence = flattenedAt(carrierResult, withCarrier, 382U);
    std::vector<double> nearbyNoise;
    for (std::size_t i = 340U; i < 370U; ++i)
        nearbyNoise.push_back(flattenedAt(carrierResult, withCarrier, i));
    const double nearbyMedian = median(nearbyNoise);
    const bool carrierOk = carrierProminence > 35.0 && nearbyMedian < 4.0;
    printResult("narrow-carrier-does-not-lift-baseline", carrierOk,
                carrierProminence, nearbyMedian);
    allOk = carrierOk && allOk;

    // A steep sound-card/receiver response at the outer audio frequencies can
    // make the fourth-order global fit undershoot locally.  The display then
    // paints both edges brighter than the centre even though the noise is only
    // shaped, not stronger.  The local lower-residual stabilization must remove
    // that false edge emphasis without touching narrow signals.
    WaterfallLeveler edgeLeveler;
    std::vector<double> edgeShaped = noiseLine(kBins, -84.0);
    for (std::size_t i = 0; i < edgeShaped.size(); ++i) {
        const double x = 2.0 * static_cast<double>(i) /
                         static_cast<double>(edgeShaped.size() - 1U) - 1.0;
        edgeShaped[i] += 18.0 * std::pow(std::abs(x), 12.0);
    }
    const WaterfallLevelResult edgeResult = edgeLeveler.update(edgeShaped, 0.050);
    std::vector<double> leftEdge;
    std::vector<double> centre;
    std::vector<double> rightEdge;
    for (std::size_t i = 0; i < 70U; ++i)
        leftEdge.push_back(flattenedAt(edgeResult, edgeShaped, i));
    for (std::size_t i = 330U; i < 438U; ++i)
        centre.push_back(flattenedAt(edgeResult, edgeShaped, i));
    for (std::size_t i = kBins - 70U; i < kBins; ++i)
        rightEdge.push_back(flattenedAt(edgeResult, edgeShaped, i));
    const double leftMedian = median(leftEdge);
    const double centreMedian = median(centre);
    const double rightMedian = median(rightEdge);
    const double edgeSpread = std::max({leftMedian, centreMedian, rightMedian}) -
                              std::min({leftMedian, centreMedian, rightMedian});
    const bool edgeOk = edgeSpread < 0.90;
    printResult("steep-passband-edges-not-artificially-hot", edgeOk,
                edgeSpread, std::max(leftMedian, rightMedian) - centreMedian);
    allOk = edgeOk && allOk;

    WaterfallLeveler asymmetricEdgeLeveler;
    std::vector<double> asymmetric = noiseLine(kBins, -86.0);
    for (std::size_t i = 0; i < asymmetric.size(); ++i) {
        const double leftDistance = static_cast<double>(i);
        const double rightDistance = static_cast<double>(asymmetric.size() - 1U - i);
        asymmetric[i] += 22.0 * std::exp(-leftDistance / 18.0) +
                         15.0 * std::exp(-rightDistance / 10.0);
    }
    asymmetric[18U] += 38.0;
    const WaterfallLevelResult asymmetricResult =
        asymmetricEdgeLeveler.update(asymmetric, 0.050);
    std::vector<double> leftOuter;
    std::vector<double> middle;
    std::vector<double> rightOuter;
    for (std::size_t i = 0; i < 32U; ++i)
        leftOuter.push_back(flattenedAt(asymmetricResult, asymmetric, i));
    for (std::size_t i = 340U; i < 428U; ++i)
        middle.push_back(flattenedAt(asymmetricResult, asymmetric, i));
    for (std::size_t i = kBins - 32U; i < kBins; ++i)
        rightOuter.push_back(flattenedAt(asymmetricResult, asymmetric, i));
    const double leftOuterMedian = median(leftOuter);
    const double middleMedian = median(middle);
    const double rightOuterMedian = median(rightOuter);
    const double asymmetricSpread = std::max({leftOuterMedian, middleMedian,
                                               rightOuterMedian}) -
                                    std::min({leftOuterMedian, middleMedian,
                                               rightOuterMedian});
    const double edgeCarrierProminence =
        flattenedAt(asymmetricResult, asymmetric, 18U) - leftOuterMedian;
    const bool asymmetricOk = asymmetricSpread < 1.10 &&
                              edgeCarrierProminence > 30.0;
    printResult("asymmetric-one-sided-edges-anchored", asymmetricOk,
                asymmetricSpread, edgeCarrierProminence);
    allOk = asymmetricOk && allOk;

    return allOk ? 0 : 1;
}
