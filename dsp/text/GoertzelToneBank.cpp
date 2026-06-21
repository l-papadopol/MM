#include "GoertzelToneBank.h"

#include <QtMath>

namespace {
constexpr double kTwoPi = 6.283185307179586476925286766559;

int saneToneCount(int count)
{
    return qBound(2, count, 64);
}

} // namespace

GoertzelToneBank::GoertzelToneBank()
{
    configure(m_sampleRate, m_firstToneHz, m_toneSpacingHz, m_toneCount, m_symbolSamples);
}

void GoertzelToneBank::configure(double sampleRate, double firstToneHz, double toneSpacingHz,
                                 int toneCount, int symbolSamples)
{
    m_sampleRate = qMax(1000.0, sampleRate);
    m_toneCount = saneToneCount(toneCount);
    m_toneSpacingHz = qMax(1.0, toneSpacingHz);
    m_firstToneHz = qBound(5.0, firstToneHz, m_sampleRate * 0.48);
    m_symbolSamples = qMax(16, symbolSamples);

    m_coefficients.resize(m_toneCount);
    for (int i = 0; i < m_toneCount; ++i) {
        const double hz = qBound(5.0,
                                 m_firstToneHz + static_cast<double>(i) * m_toneSpacingHz,
                                 m_sampleRate * 0.48);
        const double omega = kTwoPi * hz / m_sampleRate;
        m_coefficients[i] = 2.0 * qCos(omega);
    }
    rebuildWindow(m_symbolSamples);
}

void GoertzelToneBank::reset()
{
    // Stateless between symbols.  Kept for API symmetry with other DSP helpers.
}

GoertzelToneBank::Result GoertzelToneBank::analyse(const QVector<double> &samples,
                                                   bool estimateOffset) const
{
    Result result;
    if (samples.isEmpty() || m_toneCount <= 0) {
        return result;
    }

    result.energies.resize(m_toneCount);
    for (int tone = 0; tone < m_toneCount; ++tone) {
        const double coeff = m_coefficients.value(tone, 0.0);
        double q0 = 0.0;
        double q1 = 0.0;
        double q2 = 0.0;
        const int n = samples.size();
        for (int i = 0; i < n; ++i) {
            const double w = m_window.isEmpty() ? 1.0 : m_window.at(qMin(i, m_window.size() - 1));
            q0 = (coeff * q1) - q2 + (samples.at(i) * w);
            q2 = q1;
            q1 = q0;
        }
        const double power = (q1 * q1) + (q2 * q2) - (coeff * q1 * q2);
        result.energies[tone] = power;
        if (power > result.bestPower) {
            result.secondPower = result.bestPower;
            result.bestPower = power;
            result.bestIndex = tone;
        } else if (power > result.secondPower) {
            result.secondPower = power;
        }
    }

    result.confidence = result.bestPower / qMax(1.0e-18, result.secondPower + result.bestPower * 0.04);

    if (estimateOffset && result.bestIndex >= 0 && result.bestPower > 1.0e-16) {
        const double bestHz = m_firstToneHz + static_cast<double>(result.bestIndex) * m_toneSpacingHz;
        const double probe = qBound(1.0, m_toneSpacingHz * 0.30, 20.0);
        const double left = goertzelPower(samples, bestHz - probe);
        const double right = goertzelPower(samples, bestHz + probe);
        const double denom = qMax(1.0e-18, left + right + result.bestPower * 0.15);
        result.offsetHz = qBound(-m_toneSpacingHz * 0.45,
                                 ((right - left) / denom) * m_toneSpacingHz * 0.75,
                                 m_toneSpacingHz * 0.45);
    }

    return result;
}

double GoertzelToneBank::powerAt(const QVector<double> &samples, double frequencyHz) const
{
    return goertzelPower(samples, frequencyHz);
}

int GoertzelToneBank::toneCount() const
{
    return m_toneCount;
}

double GoertzelToneBank::firstToneHz() const
{
    return m_firstToneHz;
}

double GoertzelToneBank::toneSpacingHz() const
{
    return m_toneSpacingHz;
}

void GoertzelToneBank::rebuildWindow(int symbolSamples)
{
    const int n = qMax(16, symbolSamples);
    m_window.resize(n);
    if (n == 1) {
        m_window[0] = 1.0;
        return;
    }
    for (int i = 0; i < n; ++i) {
        m_window[i] = 0.5 - 0.5 * qCos(kTwoPi * static_cast<double>(i) / static_cast<double>(n - 1));
    }
}

double GoertzelToneBank::goertzelPower(const QVector<double> &samples, double frequencyHz) const
{
    if (samples.isEmpty()) {
        return 0.0;
    }
    const double hz = qBound(5.0, frequencyHz, m_sampleRate * 0.48);
    const double coeff = 2.0 * qCos(kTwoPi * hz / m_sampleRate);
    double q0 = 0.0;
    double q1 = 0.0;
    double q2 = 0.0;
    const int n = samples.size();
    for (int i = 0; i < n; ++i) {
        const double w = m_window.isEmpty() ? 1.0 : m_window.at(qMin(i, m_window.size() - 1));
        q0 = (coeff * q1) - q2 + (samples.at(i) * w);
        q2 = q1;
        q1 = q0;
    }
    return (q1 * q1) + (q2 * q2) - (coeff * q1 * q2);
}
