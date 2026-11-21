// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <optional>
#include <vector>

namespace MadModemFt {

// Search in a 200 Hz complex baseband around a recently decoded CQ.
// Costas symbols alone determine the time/frequency trajectory. The data
// symbols remain independent evidence for accepting the current message.
class Ft8RepeatRecovery
{
    static constexpr int samplesPerSymbol = 32;
    static constexpr double rate = 200.0;
    static constexpr double binHz = 1.5625;
    static constexpr double twoPi = 6.2831853071795864769;
    static constexpr int maxShift = 16;
    static constexpr int tones = 8;
    static constexpr std::array<int, 7> costas {3, 1, 4, 0, 6, 5, 2};
    static constexpr std::array<int, 3> costasStarts {0, 36, 72};

    using Complex = std::complex<double>;
    using FrequencyScores = std::array<double, 2 * maxShift + 1>;
    using SyncGrid = std::array<std::array<FrequencyScores, 7>, 3>;

    struct Alignment {
        int start = 0;
        double offsetHz = 0.0;
        double drift = 0.0;
        double score = 0.0;
    };

public:
    struct Result {
        std::array<std::array<double, tones>, 79> power{};
        double frequencyHz = 0.0;
        double startSec = 0.0;
        double drift = 0.0;
        double syncRatio = 0.0;
        int hardSync = 0;
    };

    static std::optional<Result> demodulate(const std::vector<Complex> &baseband, double centerHz,
                                             double timeHint, double driftLimit)
    {
        Alignment best;
        const int initial = int(std::lround(timeHint * rate));
        for (int shift : {0, -4, 4, -8, 8}) {
            const int start = initial + shift;
            if (start < 0 || start + 79 * samplesPerSymbol > int(baseband.size())) continue;
            const auto grid = syncGrid(baseband, start);
            searchGrid(grid, start, driftLimit, best);
        }
        // In noise the expected tone accounts for one eighth of the power.
        if (best.score < .18) return {};

        Result result;
        result.frequencyHz = centerHz + best.offsetHz;
        result.startSec = best.start / rate;
        result.drift = best.drift;
        result.syncRatio = best.score;
        for (int sym = 0; sym < 79; ++sym) {
            const double t = (sym - 39) * samplesPerSymbol / rate;
            const double offset = best.offsetHz + best.drift * t;
            const Complex *samples = baseband.data() + best.start + sym * samplesPerSymbol;
            const Complex step = std::polar(1.0, -twoPi * offset / rate);
            std::array<Complex, samplesPerSymbol> corrected{};
            Complex phasor(1., 0.);
            for (int i = 0; i < samplesPerSymbol; ++i) {
                corrected[i] = samples[i] * phasor;
                phasor *= step;
            }
            for (int tone = 0; tone < tones; ++tone)
                result.power[sym][tone] = correlate(corrected.data(), 4 * tone);
        }
        for (int block : costasStarts) {
            for (int k = 0; k < 7; ++k) {
                const auto &tone = result.power[block + k];
                result.hardSync += tone[costas[k]] == *std::max_element(tone.begin(), tone.end());
            }
        }
        if (result.hardSync < 8) return {};
        return result;
    }

private:
    static double correlate(const Complex *samples, int bin)
    {
        static const auto kernels = [] {
            std::array<std::array<Complex, samplesPerSymbol>, 4 * (tones - 1) + 2 * maxShift + 1> table{};
            for (int bin = -maxShift; bin <= 4 * (tones - 1) + maxShift; ++bin)
                for (int i = 0; i < samplesPerSymbol; ++i)
                    table[bin + maxShift][i] = std::polar(1.0, -twoPi * bin * binHz * i / rate);
            return table;
        }();
        const auto &kernel = kernels[bin + maxShift];
        Complex sum{};
        for (int i = 0; i < samplesPerSymbol; ++i) sum += samples[i] * kernel[i];
        return std::norm(sum);
    }

    static SyncGrid syncGrid(const std::vector<Complex> &baseband, int start)
    {
        SyncGrid grid{};
        for (int b = 0; b < 3; ++b) {
            for (int k = 0; k < 7; ++k) {
                const Complex *samples = baseband.data() + start + (costasStarts[b] + k) * samplesPerSymbol;
                std::array<double, 4 * (tones - 1) + 2 * maxShift + 1> power{};
                for (int bin = -maxShift; bin <= 4 * (tones - 1) + maxShift; ++bin)
                    power[bin + maxShift] = correlate(samples, bin);
                for (int q = 0; q <= 2 * maxShift; ++q) {
                    double total = 0.0;
                    for (int tone = 0; tone < tones; ++tone) total += power[q + 4 * tone];
                    grid[b][k][q] = total > 0 ? power[q + 4 * costas[k]] / total : 0.0;
                }
            }
        }
        return grid;
    }

    static void searchGrid(const SyncGrid &grid, int start, double driftLimit, Alignment &best)
    {
        const int rates = int(std::floor(std::min(2.5, driftLimit) * 4.0));
        for (int mid = -4; mid <= 4; ++mid) {
            for (int slope = -rates; slope <= rates; ++slope) {
                const double drift = slope * .25;
                double sum = 0.0;
                for (int b = 0; b < 3; ++b) {
                    for (int k = 0; k < 7; ++k) {
                        const double t = (costasStarts[b] + k - 39) * samplesPerSymbol / rate;
                        const double bin = maxShift + mid + drift * t / binHz;
                        const int j = int(bin);
                        const auto &scores = grid[b][k];
                        sum += scores[j] + (bin - j) * (scores[j + 1] - scores[j]);
                    }
                }
                const double score = sum / 21.0;
                if (score > best.score) best = {start, mid * binHz, drift, score};
            }
        }
    }
};

} // namespace MadModemFt
