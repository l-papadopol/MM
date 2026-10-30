// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace MadModemFt {
// Centered cos^2 FIR, evaluated with sliding DC/cosine/sine sums. This is
// convolution, not a forward/backward IIR approximation. The window has L+1
// taps, zero endpoints and total mass L/2. Work is O(samples + L).
// Only the valid input interval is reconstructed; outputs outside it are zero.
inline void raisedCosineEnvelope(std::vector<std::complex<double>> &samples,
                                 int length, bool correctEdges,
                                 int validBegin = 0, int validEnd = -1)
{
    if (samples.empty() || length < 2 || (length & 1)) return;
    const int count = static_cast<int>(samples.size());
    validBegin = std::max(0, std::min(count, validBegin));
    validEnd = validEnd < 0 ? count : std::max(validBegin, std::min(count, validEnd));
    if (validBegin == validEnd) { std::fill(samples.begin(), samples.end(), std::complex<double>{}); return; }
    const auto input = samples;
    std::vector<double> cosine(length), sine(length);
    constexpr double twoPi = 6.283185307179586476925286766559;
    for (int k = 0; k < length; ++k) {
        cosine[k] = std::cos(twoPi * k / length);
        sine[k] = std::sin(twoPi * k / length);
    }
    std::complex<double> dc{}, re{}, im{};
    double massRe = 0, massIm = 0;
    int mass = 0;
    auto update = [&](int k, double sign) {
        if (k < validBegin || k >= validEnd) return;
        const double c = cosine[k % length], s = sine[k % length];
        dc += sign * input[k]; re += (sign * c) * input[k]; im += (sign * s) * input[k];
        mass += sign > 0 ? 1 : -1; massRe += sign * c; massIm += sign * s;
    };
    const int half = length / 2;
    for (int k = 0; k <= std::min(half, count-1); ++k) update(k, 1);
    for (int n = 0; n < count; ++n) {
        const double c = cosine[n % length], s = sine[n % length];
        const double divisor = correctEdges ? mass + c*massRe + s*massIm : double(length);
        samples[n] = (n >= validBegin && n < validEnd && divisor > 1.e-12) ? (dc + c*re + s*im) / divisor : std::complex<double>{};
        update(n-half, -1); update(n+half+1, 1);
    }
}
}
