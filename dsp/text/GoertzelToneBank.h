#ifndef GOERTZELTONEBANK_H
#define GOERTZELTONEBANK_H

#include <QVector>

/**
 * @brief Small Goertzel/Hann tone-bank for symbol-rate MFSK style receivers.
 *
 * This class is intentionally dependency-free and GPL-friendly: it follows the
 * classic tone-bank pattern used by programs such as fldigi for MFSK family
 * receivers, but it is a clean MadModem implementation.  It avoids per-sample
 * trig in the detector inner loop, precomputes the Hann window for the current
 * symbol length and evaluates all requested tones with stable double-precision
 * Goertzel recurrences.
 */
class GoertzelToneBank
{
public:
    struct Result
    {
        int bestIndex = -1;
        double bestPower = 0.0;
        double secondPower = 0.0;
        double confidence = 0.0;
        double offsetHz = 0.0;
        QVector<double> energies;
    };

    GoertzelToneBank();

    void configure(double sampleRate, double firstToneHz, double toneSpacingHz,
                   int toneCount, int symbolSamples);
    void reset();

    Result analyse(const QVector<double> &samples, bool estimateOffset = true) const;
    double powerAt(const QVector<double> &samples, double frequencyHz) const;

    int toneCount() const;
    double firstToneHz() const;
    double toneSpacingHz() const;

private:
    void rebuildWindow(int symbolSamples);
    double goertzelPower(const QVector<double> &samples, double frequencyHz) const;

private:
    double m_sampleRate = 8000.0;
    double m_firstToneHz = 500.0;
    double m_toneSpacingHz = 15.625;
    int m_toneCount = 16;
    int m_symbolSamples = 512;

    QVector<double> m_coefficients;
    QVector<double> m_window;
};

#endif // GOERTZELTONEBANK_H
