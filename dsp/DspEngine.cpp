#include "DspEngine.h"

#include <QtMath>

#include <algorithm>
#include <utility>
#include <vector>

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
    m_waterfallLeveler.reset();
}

void DspEngine::configureSignalPeakMetric(bool enabled, int lowHz, int highHz)
{
    m_signalPeakMetricEnabled = enabled;
    m_signalPeakLowHz = qMax(0, qMin(lowHz, highHz));
    m_signalPeakHighHz = qMax(m_signalPeakLowHz, qMax(lowHz, highHz));
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
     * Display-only WSJT-X-style flattening.
     *
     * WSJT-X Wide Graph does not let a slow temporal AGC chase the absolute
     * receiver level.  Its default Flatten path converts each row to dB,
     * estimates a lower-envelope polynomial from the lowest 10% of each of
     * ten frequency segments, then subtracts that baseline before applying a
     * fixed gain/zero colour mapping.  This reacts immediately to receiver
     * AGC steps without leaving the whole passband orange for many seconds.
     *
     * MadModem keeps its persistent passband detector only to exclude truly
     * silent monitor-device bins from the fit.  Decoder audio is untouched.
     */
    std::vector<double> levelInput;
    levelInput.reserve(static_cast<std::size_t>(dbLine.size()));
    for (double value : dbLine) levelInput.push_back(value);
    const double lineSeconds = static_cast<double>(m_hopSize) /
                               static_cast<double>(sampleRate);
    const WaterfallLevelResult levels =
        m_waterfallLeveler.update(levelInput, lineSeconds);
    const bool haveBaseline = levels.baselineDb.size() ==
                              static_cast<std::size_t>(dbLine.size());

    for (int x = 0; x < m_columns; ++x) {
        const double baselineDb = haveBaseline
            ? levels.baselineDb[static_cast<std::size_t>(x)]
            : levels.floorDb;
        const double flattenedDb = dbLine[x] - baselineDb;

        // Equivalent to the WSJT-X Wide Graph default transfer:
        //     y1 = 10 * gain * flattened_dB + zero
        // The user colour-scale control supplies the gain in WaterfallWidget;
        // zero remains 0 so the lower envelope maps to black/dark blue.
        const double rawValue = 10.0 * flattenedDb;
        const int value = qBound(0, static_cast<int>(qRound(rawValue)), 254);

        waterfallLine[x] = static_cast<quint8>(value);
    }

    emit waterfallLineReady(waterfallLine, m_minHz, m_maxHz);

    // QSO signal peak metric. Reuse the FFT that was already required for the
    // waterfall instead of running Welch/FFT work in MainWindow. The metric is
    // intentionally small: average power in the selected signal band versus
    // equal-width adjacent noise bands, with a guard gap around the signal.
    if (m_signalPeakMetricEnabled && m_signalPeakHighHz > m_signalPeakLowHz) {
        const int nyquist = sampleRate / 2;
        const int lowHz = qBound(20, m_signalPeakLowHz, qMax(20, nyquist - 20));
        const int highHz = qBound(lowHz + 10, m_signalPeakHighHz, qMax(lowHz + 10, nyquist - 10));
        const int widthHz = qMax(20, highHz - lowHz);
        const int guardHz = qMax(30, widthHz / 3);

        auto meanBandPower = [&](int bandLowHz, int bandHighHz, bool *ok) -> double {
            if (ok != nullptr) *ok = false;
            bandLowHz = qBound(1, bandLowHz, nyquist - 1);
            bandHighHz = qBound(bandLowHz + 1, bandHighHz, nyquist);
            const int firstBin = qBound(1, static_cast<int>(qCeil(
                static_cast<double>(bandLowHz) * m_fftSize / sampleRate)), maxBin);
            const int lastBin = qBound(firstBin, static_cast<int>(qFloor(
                static_cast<double>(bandHighHz) * m_fftSize / sampleRate)), maxBin);
            if (lastBin < firstBin) return 0.0;
            double sum = 0.0;
            int count = 0;
            for (int bin = firstBin; bin <= lastBin; ++bin) {
                const double mag = magnitudes.at(bin);
                sum += mag * mag;
                ++count;
            }
            if (count <= 0 || !(sum > 0.0) || !qIsFinite(sum)) return 0.0;
            if (ok != nullptr) *ok = true;
            return sum / static_cast<double>(count);
        };

        bool signalOk = false;
        const double signalPower = meanBandPower(lowHz, highHz, &signalOk);
        QVector<double> noisePowers;
        bool noiseOk = false;
        const int leftHigh = lowHz - guardHz;
        const int leftLow = leftHigh - widthHz;
        if (leftLow >= 20) {
            const double p = meanBandPower(leftLow, leftHigh, &noiseOk);
            if (noiseOk) noisePowers.append(p);
        }
        const int rightLow = highHz + guardHz;
        const int rightHigh = rightLow + widthHz;
        if (rightHigh <= nyquist - 10) {
            const double p = meanBandPower(rightLow, rightHigh, &noiseOk);
            if (noiseOk) noisePowers.append(p);
        }

        if (signalOk && !noisePowers.isEmpty()) {
            double noisePower = 0.0;
            for (double p : std::as_const(noisePowers)) noisePower += p;
            noisePower /= static_cast<double>(noisePowers.size());
            if (noisePower > 0.0 && qIsFinite(noisePower)) {
                emit signalPeakMetricReady(10.0 * qLn(qMax(1.0e-12, signalPower / noisePower)) / qLn(10.0), true);
            } else {
                emit signalPeakMetricReady(-99.0, false);
            }
        } else {
            emit signalPeakMetricReady(-99.0, false);
        }
    }

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
