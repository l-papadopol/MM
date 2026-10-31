// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cmath>
#include <complex>
#include <vector>
namespace MadModemFt {
template<int Tones>
inline std::complex<double> coherentCorrelation32(
    const std::vector<std::complex<double>> &samples, int begin, int tone)
{
    static const auto basis = [] {
        std::array<std::array<std::complex<double>, 32>, Tones> values{};
        constexpr double twoPi = 6.283185307179586476925286766559;
        for (int t = 0; t < Tones; ++t) for (int n = 0; n < 32; ++n) {
            const double phase = -twoPi * static_cast<double>(t * n) / 32.0;
            values[t][n] = std::complex<double>(std::cos(phase), std::sin(phase));
        }
        return values;
    }();
    std::complex<double> sum(0.0, 0.0);
    for (int n = 0; n < 32; ++n) {
        const int index = begin + n;
        if (index >= 0 && index < static_cast<int>(samples.size()))
            sum += samples[static_cast<size_t>(index)] * basis[tone][n];
    }
    return sum;
}
}
