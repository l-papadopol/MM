#pragma once

#include <cstddef>
#include <vector>

/**
 * Display-only waterfall normalization modelled on the WSJT-X Wide Graph
 * "Flatten" path.
 *
 * WSJT-X does not run a slow full-band AGC over successive waterfall rows.
 * Each row is converted to dB, a lower-envelope polynomial is estimated from
 * ten frequency segments, and that baseline is subtracted before fixed
 * gain/zero colour mapping.  Consequently a receiver AGC step changes the
 * absolute input level without making the whole waterfall stay hot for many
 * seconds.
 */
struct WaterfallLevelResult
{
    // Representative baseline values retained for diagnostics and tests.
    double floorDb = -110.0;
    double ceilingDb = -86.0;

    std::size_t validBegin = 0;
    std::size_t validEnd = 0; // inclusive
    bool partialBand = false;

    // Per-bin lower-envelope fit in dB.  DspEngine subtracts this from the
    // current dB spectrum before applying the fixed WSJT-X-like colour gain.
    std::vector<double> baselineDb;
};

class WaterfallLeveler
{
public:
    void reset();
    WaterfallLevelResult update(const std::vector<double> &dbLine,
                                double elapsedSeconds);

private:
    std::size_t m_validBegin = 0;
    std::size_t m_validEnd = 0;
    std::size_t m_candidateBegin = 0;
    std::size_t m_candidateEnd = 0;
    std::size_t m_lastBinCount = 0;
    int m_partialConfirm = 0;
    int m_fullConfirm = 0;
    bool m_haveValidBand = false;
};
