// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QVector>
#include <QtGlobal>
#include <algorithm>
#include <complex>
#include <vector>
#include <cmath>
namespace MadModemAudio {
inline int previousPowerOfTwo(int value)
{
    int p = 1;
    while ((p << 1) > 0 && (p << 1) <= value) {
        p <<= 1;
    }
    return p;
}

inline void fftRadix2Local(std::vector<std::complex<double>> &a)
{
    const int n = static_cast<int>(a.size());
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(a[static_cast<size_t>(i)], a[static_cast<size_t>(j)]);
        }
    }

    constexpr double kTwoPi = 6.283185307179586476925286766559;
    for (int len = 2; len <= n; len <<= 1) {
        const double angle = -kTwoPi / static_cast<double>(len);
        const std::complex<double> wlen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            const int half = len >> 1;
            for (int j = 0; j < half; ++j) {
                const std::complex<double> u = a[static_cast<size_t>(i + j)];
                const std::complex<double> v = a[static_cast<size_t>(i + j + half)] * w;
                a[static_cast<size_t>(i + j)] = u + v;
                a[static_cast<size_t>(i + j + half)] = u - v;
                w *= wlen;
            }
        }
    }
}

inline double trimmedMean(QVector<double> values, double lowTrimFraction, double highTrimFraction)
{
    if (values.isEmpty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const int n = values.size();
    int first = qBound(0, static_cast<int>(std::floor(static_cast<double>(n) * lowTrimFraction)), n - 1);
    int lastExclusive = n - qBound(0, static_cast<int>(std::floor(static_cast<double>(n) * highTrimFraction)), n - 1);
    if (lastExclusive <= first) {
        first = 0;
        lastExclusive = n;
    }
    double sum = 0.0;
    int count = 0;
    for (int i = first; i < lastExclusive; ++i) {
        sum += values.at(i);
        ++count;
    }
    return count > 0 ? sum / static_cast<double>(count) : 0.0;
}

/**
 * @brief Robust Welch PSD band power for Radio Telescope tiles.
 *
 * This replaces the early Goertzel-bank prototype.  Each audio block is split
 * into Hann-windowed, 50%-overlapped FFT segments.  The one-sided PSD is
 * integrated inside the selected audio slice.  A trimmed mean over frequency
 * bins suppresses narrow carriers/spurs so the result follows broadband noise
 * more than one accidental whistle.  The caller still averages consecutive
 * block results during the tile dwell interval.
 */
inline double radioTelescopeWelchBandPower(const QVector<float> &samples, int sampleRate, int lowHz, int highHz)
{
    const int n = samples.size();
    const int fs = sampleRate;
    if (n < 128 || fs <= 0) {
        return 0.0;
    }

    lowHz = qBound(1, lowHz, static_cast<int>(0.48 * static_cast<double>(fs)));
    highHz = qBound(lowHz + 1, highHz, static_cast<int>(0.48 * static_cast<double>(fs)));
    if (highHz <= lowHz) {
        return 0.0;
    }

    int segmentSize = previousPowerOfTwo(qMin(n, 4096));
    if (segmentSize < 256) {
        segmentSize = previousPowerOfTwo(n);
    }
    if (segmentSize < 128) {
        return 0.0;
    }
    const int hop = qMax(1, segmentSize / 2);
    const double df = static_cast<double>(fs) / static_cast<double>(segmentSize);
    const int firstBin = qBound(1, static_cast<int>(std::ceil(static_cast<double>(lowHz) / df)), (segmentSize / 2) - 1);
    const int lastBin = qBound(firstBin, static_cast<int>(std::floor(static_cast<double>(highHz) / df)), segmentSize / 2);
    if (lastBin < firstBin) {
        return 0.0;
    }

    double globalMean = 0.0;
    for (float sample : samples) {
        globalMean += static_cast<double>(sample);
    }
    globalMean /= static_cast<double>(n);

    QVector<double> window(segmentSize);
    double windowPower = 0.0;
    constexpr double kTwoPi = 6.283185307179586476925286766559;
    for (int i = 0; i < segmentSize; ++i) {
        const double w = 0.5 - 0.5 * std::cos((kTwoPi * static_cast<double>(i)) /
                                               qMax(1.0, static_cast<double>(segmentSize - 1)));
        window[i] = w;
        windowPower += w * w;
    }
    if (windowPower <= 0.0) {
        return 0.0;
    }

    QVector<double> segmentBandPowers;
    std::vector<std::complex<double>> fft(static_cast<size_t>(segmentSize));

    for (int start = 0; start + segmentSize <= n; start += hop) {
        for (int i = 0; i < segmentSize; ++i) {
            const double x = (static_cast<double>(samples.at(start + i)) - globalMean) * window.at(i);
            fft[static_cast<size_t>(i)] = std::complex<double>(x, 0.0);
        }
        fftRadix2Local(fft);

        QVector<double> binPowers;
        binPowers.reserve(lastBin - firstBin + 1);
        for (int bin = firstBin; bin <= lastBin; ++bin) {
            const std::complex<double> v = fft[static_cast<size_t>(bin)];
            double psd = std::norm(v) / (static_cast<double>(fs) * windowPower);
            if (bin > 0 && bin < segmentSize / 2) {
                psd *= 2.0; // one-sided PSD
            }
            binPowers.append(qMax(0.0, psd * df)); // integrated power represented by this FFT bin
        }

        if (!binPowers.isEmpty()) {
            // Reject a little low-end leakage and more high-end narrow carriers.
            const double perBinPower = trimmedMean(binPowers, 0.05, 0.20);
            segmentBandPowers.append(perBinPower * static_cast<double>(binPowers.size()));
        }
    }

    if (segmentBandPowers.isEmpty()) {
        return 0.0;
    }
    // Robust temporal average inside this block; the dwell accumulator then
    // averages many block-level estimates.
    return trimmedMean(segmentBandPowers, 0.10, 0.10);
}

}
