#include "DspEngine.h"

#include <QtMath>

#include <algorithm>

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

DspEngine::DspEngine(QObject *parent)
    : QObject(parent)
{
}

// -----------------------------------------------------------------------------
// Public slots
// -----------------------------------------------------------------------------

void DspEngine::processAudioBlock(const AudioBlock &block)
{
    if (block.samples.isEmpty() || block.sampleRate <= 0) {
        return;
    }

    m_fifo.reserve(m_fifo.size() + block.samples.size());

    for (float sample : block.samples) {
        m_fifo.append(sample);
    }

    while (m_fifo.size() >= m_fftSize) {
        QVector<float> window;
        window.reserve(m_fftSize);

        for (int i = 0; i < m_fftSize; ++i) {
            window.append(m_fifo[i]);
        }

        analyzeWindow(window, block.sampleRate);

        m_fifo.remove(0, qMin(m_hopSize, m_fifo.size()));
    }
}

void DspEngine::reset()
{
    m_fifo.clear();
    m_smoothedWaterfall.clear();
    m_displayFloorDb = -110.0;
    m_displayCeilingDb = -62.0;
    m_levelWarmupWindows = 0;
}

// -----------------------------------------------------------------------------
// Analysis
// -----------------------------------------------------------------------------

void DspEngine::analyzeWindow(const QVector<float> &window, int sampleRate)
{
    ensureWindowTable();

    QVector<double> real(m_fftSize);
    QVector<double> imag(m_fftSize);

    for (int i = 0; i < m_fftSize; ++i) {
        real[i] = static_cast<double>(window[i]) * m_windowTable[i];
        imag[i] = 0.0;
    }

    fft(real, imag);

    const int maxBin = (m_fftSize / 2) - 2;
    QVector<double> magnitudes(maxBin + 2);

    for (int bin = 0; bin <= maxBin + 1; ++bin) {
        magnitudes[bin] = qSqrt(real[bin] * real[bin] + imag[bin] * imag[bin]) /
                          static_cast<double>(m_fftSize);
    }

    QVector<double> dbLine;
    dbLine.resize(m_columns);

    QVector<quint8> waterfallLine;
    waterfallLine.resize(m_columns);

    if (m_smoothedWaterfall.size() != m_columns) {
        m_smoothedWaterfall.fill(0.0, m_columns);
    }

    double bestDb = -200.0;
    double bestFrequency = 0.0;

    const double log10 = qLn(10.0);

    for (int x = 0; x < m_columns; ++x) {
        const double ratio = static_cast<double>(x) / static_cast<double>(m_columns - 1);
        const double freq = m_minHz + ratio * (m_maxHz - m_minHz);
        const double binPosition = (freq * static_cast<double>(m_fftSize)) /
                                   static_cast<double>(sampleRate);

        const int bin0 = qBound(1, static_cast<int>(qFloor(binPosition)), maxBin);
        const int bin1 = qBound(1, bin0 + 1, maxBin + 1);
        const double frac = qBound(0.0, binPosition - static_cast<double>(bin0), 1.0);
        const double mag = ((1.0 - frac) * magnitudes[bin0]) + (frac * magnitudes[bin1]);
        const double db = 20.0 * qLn(qMax(mag, 1.0e-12)) / log10;
        dbLine[x] = db;

        if (db > bestDb) {
            bestDb = db;
            bestFrequency = freq;
        }
    }

    /*
     * WSJT-X/MSHV-style display levelling: do not map every FFT line through a
     * fixed -100..-18 dB ramp.  Real receiver audio has very different noise
     * floors on different PCs and sound cards; a fixed ramp makes a whole band
     * go green/yellow and weak FT traces disappear.  Instead estimate the
     * visible-band floor/peak from robust percentiles, track them slowly, and
     * map signal excess above the local floor into the palette.  Decoder input
     * is unchanged; this is display-only.
     */
    QVector<double> sortedDb = dbLine;
    std::sort(sortedDb.begin(), sortedDb.end());
    const int n = sortedDb.size();
    const double p30 = sortedDb[qBound(0, static_cast<int>(qRound(0.30 * (n - 1))), n - 1)];
    const double p96 = sortedDb[qBound(0, static_cast<int>(qRound(0.96 * (n - 1))), n - 1)];

    const double targetFloor = qBound(-130.0, p30 + 2.0, -25.0);
    const double targetCeiling = qBound(targetFloor + 26.0,
                                        qMax(p96 + 5.0, targetFloor + 38.0),
                                        targetFloor + 62.0);

    if (m_levelWarmupWindows < 8) {
        const double alpha = 0.45;
        m_displayFloorDb = ((1.0 - alpha) * m_displayFloorDb) + (alpha * targetFloor);
        m_displayCeilingDb = ((1.0 - alpha) * m_displayCeilingDb) + (alpha * targetCeiling);
        ++m_levelWarmupWindows;
    } else {
        const double floorAlpha = (targetFloor < m_displayFloorDb) ? 0.14 : 0.045;
        const double ceilingAlpha = (targetCeiling > m_displayCeilingDb) ? 0.18 : 0.06;
        m_displayFloorDb = ((1.0 - floorAlpha) * m_displayFloorDb) + (floorAlpha * targetFloor);
        m_displayCeilingDb = ((1.0 - ceilingAlpha) * m_displayCeilingDb) + (ceilingAlpha * targetCeiling);
    }

    if (m_displayCeilingDb < m_displayFloorDb + 24.0) {
        m_displayCeilingDb = m_displayFloorDb + 24.0;
    }

    for (int x = 0; x < m_columns; ++x) {
        const double normalized = (dbLine[x] - m_displayFloorDb) /
                                  (m_displayCeilingDb - m_displayFloorDb);
        const double clamped = qBound(0.0, normalized, 1.0);

        /*
         * v1.59: make the visual transfer curve more radio-waterfall-like and
         * less linear.  Keep a small black point so receiver noise stays dark,
         * then use a logarithmic expansion so weak/medium traces jump out
         * before the palette reaches the hot colours.
         */
        const double blackPoint = 0.035;
        const double lifted = qBound(0.0, (clamped - blackPoint) / (1.0 - blackPoint), 1.0);
        const double shaped = qLn(1.0 + lifted * 18.0) / qLn(19.0);
        const double rawValue = qBound(0.0, shaped * 255.0, 255.0);

        const double previous = m_smoothedWaterfall[x];
        const double alpha = (rawValue > previous) ? 0.82 : 0.48;
        const double smoothedValue = (alpha * rawValue) + ((1.0 - alpha) * previous);
        m_smoothedWaterfall[x] = smoothedValue;
        const int value = qBound(0, static_cast<int>(qRound(smoothedValue)), 255);

        waterfallLine[x] = static_cast<quint8>(value);
    }

    emit waterfallLineReady(waterfallLine, m_minHz, m_maxHz);

    if (bestDb > -92.0) {
        emit dominantFrequencyChanged(bestFrequency, bestDb);
    } else {
        emit dominantFrequencyChanged(0.0, bestDb);
    }
}

void DspEngine::ensureWindowTable()
{
    if (m_windowTable.size() == m_fftSize) {
        return;
    }

    m_windowTable.resize(m_fftSize);

    for (int i = 0; i < m_fftSize; ++i) {
        const double phase = (2.0 * M_PI * i) /
                             static_cast<double>(m_fftSize - 1);

        /*
         * Four-term Blackman-Harris window.  It has a wider main lobe than
         * Hann, but far lower sidelobes, so strong WEFAX/SSTV tones leak much
         * less into nearby waterfall bins and weak signals are easier to see.
         */
        m_windowTable[i] =
            0.35875 -
            (0.48829 * qCos(phase)) +
            (0.14128 * qCos(2.0 * phase)) -
            (0.01168 * qCos(3.0 * phase));
    }
}

void DspEngine::fft(QVector<double> &real, QVector<double> &imag)
{
    const int n = real.size();

    int j = 0;
    for (int i = 1; i < n; ++i) {
        int bit = n >> 1;

        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }

        j ^= bit;

        if (i < j) {
            qSwap(real[i], real[j]);
            qSwap(imag[i], imag[j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1) {
        const double angle = -2.0 * M_PI / static_cast<double>(len);
        const double wLenReal = qCos(angle);
        const double wLenImag = qSin(angle);

        for (int i = 0; i < n; i += len) {
            double wReal = 1.0;
            double wImag = 0.0;

            for (int k = 0; k < len / 2; ++k) {
                const int even = i + k;
                const int odd = i + k + len / 2;

                const double oddReal = real[odd] * wReal - imag[odd] * wImag;
                const double oddImag = real[odd] * wImag + imag[odd] * wReal;

                const double evenReal = real[even];
                const double evenImag = imag[even];

                real[even] = evenReal + oddReal;
                imag[even] = evenImag + oddImag;

                real[odd] = evenReal - oddReal;
                imag[odd] = evenImag - oddImag;

                const double nextWReal = wReal * wLenReal - wImag * wLenImag;
                const double nextWImag = wReal * wLenImag + wImag * wLenReal;

                wReal = nextWReal;
                wImag = nextWImag;
            }
        }
    }
}
