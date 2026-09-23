#ifndef LINEARRESAMPLER_H
#define LINEARRESAMPLER_H

#include <QVector>
#include "AntiAliasFilter.h"

/**
 * @brief Streaming resampler with an anti-alias FIR before linear interpolation.
 *
 * Text modes benefit from exact internal timing.  MadModem uses 8000 Hz for
 * RTTY and BPSK31 because common baud/symbol rates map to convenient sample
 * counts while still preserving all information in the narrow audio passband.
 */
class LinearResampler
{
public:
    void configure(double outputSampleRate);
    void reset();
    QVector<double> process(const QVector<float> &input, int inputSampleRate);

    double outputSampleRate() const;
    qint64 delayNanoseconds() const { return m_inputSampleRate > 0 ? static_cast<qint64>(m_antiAlias.delaySamples()) * 1000000000LL / m_inputSampleRate : 0; }

private:
    AntiAliasFilter m_antiAlias;
    double m_outputSampleRate = 8000.0;
    int m_inputSampleRate = 0;
    double m_stepInputSamples = 6.0;
    double m_absoluteInputIndex = 0.0;
    double m_nextOutputInputIndex = 0.0;
    double m_previousSample = 0.0;
    bool m_havePreviousSample = false;
};

#endif // LINEARRESAMPLER_H
