#pragma once

#include <cstddef>
#include <deque>
#include <vector>

/**
 * Stable display-only waterfall levelling.
 *
 * The leveler keeps a persistent estimate of the actually populated audio
 * passband and a slow history of its low noise quantile.  Silent FFT regions,
 * a strong carrier, or one ambiguous row cannot move the colour reference.
 */
struct WaterfallLevelResult
{
    double floorDb = -110.0;
    double ceilingDb = -62.0;
    std::size_t validBegin = 0;
    std::size_t validEnd = 0; // inclusive
    bool partialBand = false;
};

class WaterfallLeveler
{
public:
    void reset();
    WaterfallLevelResult update(const std::vector<double> &dbLine,
                                double elapsedSeconds);

private:
    double m_floorDb = -110.0;
    bool m_initialized = false;

    std::deque<double> m_floorCandidates;
    std::size_t m_validBegin = 0;
    std::size_t m_validEnd = 0;
    std::size_t m_candidateBegin = 0;
    std::size_t m_candidateEnd = 0;
    std::size_t m_lastBinCount = 0;
    int m_partialConfirm = 0;
    int m_fullConfirm = 0;
    bool m_haveValidBand = false;
};
