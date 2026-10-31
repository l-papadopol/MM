// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <complex>
#include <functional>
#include <string>
#include <vector>
#include <utility>
#include "CwCtcPrefixDecoder.h"

namespace madmodem::cwskimmer {

// Independently trained causal temporal CNN + CTC. No external ML runtime or model download.
// The feature bank follows the operator-selected tone, independently for A/B.
class CwNeuralDecoder {
public:
    using Callback = std::function<void(const std::string&)>;
    void setCallback(Callback callback) { m_callback = std::move(callback); }
    void setToneHz(double hz);
    void reset();
    void process(const float* audio, std::size_t count, int rate);
    // Exposed for reproducible cross-language parity checks and offline training QA.
    std::array<float, 42> inferFeatures(const std::array<float, 9>& features);
    const char* modelId() const { return "mm-cw-r13-search"; }
    const std::array<float, 9>& lastFeatures() const { return m_features; }
private:
    void configure(int rate);
    void frame();
    void emitLabel(int selected);
    Callback m_callback;
    double m_tone = 700.;
    int m_rate = 0, m_window = 0, m_hop = 0;
    std::size_t m_position = 0, m_samples = 0;
    std::array<std::complex<double>, 9> m_phase{}, m_rotation{}, m_sum{};
    std::array<std::vector<std::complex<double>>, 9> m_history;
    std::vector<double> m_energyHistory;
    CwCtcPrefixDecoder m_prefix;
    double m_peakEnergy = 0.;
    double m_energy = 0.;
    std::array<std::vector<float>, 4> m_layerHistory;
    std::size_t m_frameNumber = 0;
    std::array<float, 9> m_features{};
    bool m_haveText = false, m_lastWasSpace = true;
};
}
