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
    // Deterministic low-amplitude ripple avoids testing a perfectly flat and
    // unrealistic FFT while keeping the expected percentile reproducible.
    for (std::size_t i = 0; i < line.size(); ++i)
        line[i] += 1.35 * std::sin(0.173 * static_cast<double>(i)) +
                   0.45 * std::cos(0.071 * static_cast<double>(i));
    return line;
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

    WaterfallLeveler fullLeveler;
    const auto full = noiseLine(kBins, -82.0);
    const WaterfallLevelResult fullResult = fullLeveler.update(full, 0.050);

    WaterfallLeveler partialLeveler;
    std::vector<double> partial(kBins, -126.0);
    const auto active = noiseLine(344U, -82.0);
    std::copy(active.begin(), active.end(), partial.begin() + 202);
    const WaterfallLevelResult partialResult = partialLeveler.update(partial, 0.050);

    const double occupancyDelta = std::abs(fullResult.floorDb -
                                           partialResult.floorDb);
    const bool occupancyOk = partialResult.partialBand &&
                             partialResult.validBegin < 220U &&
                             partialResult.validEnd > 525U &&
                             occupancyDelta < 1.5;
    printResult("partial-band-does-not-pump", occupancyOk,
                fullResult.floorDb, partialResult.floorDb);
    allOk = occupancyOk && allOk;

    WaterfallLeveler carrierLeveler;
    const WaterfallLevelResult carrierBase = carrierLeveler.update(full, 0.050);
    std::vector<double> withCarrier = full;
    for (std::size_t i = 379U; i <= 385U; ++i)
        withCarrier[i] = -31.0 + 0.2 * static_cast<double>(i - 379U);
    WaterfallLevelResult carrierResult = carrierBase;
    for (int i = 0; i < 20; ++i)
        carrierResult = carrierLeveler.update(withCarrier, 0.050);
    const double carrierDelta = std::abs(carrierResult.floorDb -
                                         carrierBase.floorDb);
    const bool carrierOk = carrierDelta < 0.8 &&
                           std::abs((carrierResult.ceilingDb -
                                     carrierResult.floorDb) - 48.0) < 1.0e-9;
    printResult("strong-carrier-does-not-pump", carrierOk,
                carrierBase.floorDb, carrierResult.floorDb);
    allOk = carrierOk && allOk;

    WaterfallLeveler slewLeveler;
    const WaterfallLevelResult beforeSlew =
        slewLeveler.update(noiseLine(kBins, -92.0), 0.050);
    const WaterfallLevelResult afterSlew =
        slewLeveler.update(noiseLine(kBins, -57.0), 0.100);
    const double slewStep = afterSlew.floorDb - beforeSlew.floorDb;
    const bool slewOk = slewStep >= -1.0e-9 && slewStep <= 0.161;
    printResult("floor-slew-bounded", slewOk, beforeSlew.floorDb,
                afterSlew.floorDb);
    allOk = slewOk && allOk;

    WaterfallLeveler silentEdgeLeveler;
    std::vector<double> sloped = noiseLine(kBins, -83.0);
    for (std::size_t i = 0; i < sloped.size(); ++i)
        sloped[i] += 7.0 * static_cast<double>(i) /
                     static_cast<double>(sloped.size() - 1U);
    const WaterfallLevelResult slopedResult =
        silentEdgeLeveler.update(sloped, 0.050);
    const bool slopeOk = !slopedResult.partialBand;
    printResult("broad-slope-is-not-passband-edge", slopeOk,
                static_cast<double>(slopedResult.validBegin),
                static_cast<double>(slopedResult.validEnd));
    allOk = slopeOk && allOk;

    WaterfallLeveler persistentLeveler;
    WaterfallLevelResult persistentResult;
    for (int i = 0; i < 8; ++i)
        persistentResult = persistentLeveler.update(partial, 0.050);
    const double persistentBase = persistentResult.floorDb;
    std::vector<double> ambiguous = partial;
    // A broad keyed/QRM region temporarily obscures the passband profile.  It
    // must not clear the persistent mask or pull the floor into the signal.
    for (std::size_t i = 250U; i < 500U; ++i)
        ambiguous[i] = -39.0 + 0.6 * std::sin(0.11 * static_cast<double>(i));
    for (int i = 0; i < 80; ++i)
        persistentResult = persistentLeveler.update(ambiguous, 0.050);
    const bool persistenceOk = persistentResult.partialBand &&
                               std::abs(persistentResult.floorDb - persistentBase) < 0.35;
    printResult("persistent-passband-resists-broad-signal", persistenceOk,
                persistentBase, persistentResult.floorDb);
    allOk = persistenceOk && allOk;

    return allOk ? 0 : 1;
}
