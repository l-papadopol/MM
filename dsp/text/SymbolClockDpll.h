#ifndef SYMBOLCLOCKDPLL_H
#define SYMBOLCLOCKDPLL_H

/**
 * @brief Lightweight fractional symbol clock for CPU text decoders.
 *
 * The class keeps exact fractional timing.  Small positive/negative corrections
 * can be applied by modem-specific edge/error detectors without changing the
 * nominal baud-derived samples-per-symbol.
 */
class SymbolClockDpll
{
public:
    void configure(double samplesPerSymbol);
    void reset(double initialDelaySymbols = 1.0);
    bool tick();
    void nudge(double samples);
    double samplesPerSymbol() const;
    double countdown() const;

private:
    double m_samplesPerSymbol = 256.0;
    double m_countdown = 256.0;
};

#endif // SYMBOLCLOCKDPLL_H
