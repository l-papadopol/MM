#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// Causal, unity-DC-gain Blackman FIR. State belongs to one continuous stream.
// The delay is exposed so timestamped consumers can compensate explicitly.
class AntiAliasFilter {
public:
    void configure(double inputRate, double outputRate) {
        m_coefficients.clear(); m_history.clear(); m_cursor = 0;
        if (inputRate <= outputRate || outputRate <= 0) return;
        const int taps = 32 * static_cast<int>(std::ceil(inputRate / outputRate)) + 1;
        m_coefficients.resize(taps); m_history.assign(taps, 0.0);
        constexpr double pi = 3.14159265358979323846;
        const double cutoff = 0.45 * outputRate / inputRate;
        double sum = 0;
        for (int i = 0; i < taps; ++i) {
            const double x = i - (taps - 1) / 2;
            const double sinc = x == 0 ? 2 * cutoff : std::sin(2 * pi * cutoff * x) / (pi * x);
            const double window = 0.42 - 0.5 * std::cos(2 * pi * i / (taps - 1))
                                      + 0.08 * std::cos(4 * pi * i / (taps - 1));
            sum += m_coefficients[i] = sinc * window;
        }
        for (double &c : m_coefficients) c /= sum;
    }
    double process(double sample) {
        if (m_coefficients.empty()) return sample;
        m_history[m_cursor] = sample;
        double result = 0;
        auto index = m_cursor;
        for (double coefficient : m_coefficients) {
            result += coefficient * m_history[index];
            index = index == 0 ? m_history.size() - 1 : index - 1;
        }
        m_cursor = (m_cursor + 1) % m_history.size();
        return result;
    }
    int delaySamples() const { return m_coefficients.empty() ? 0 : static_cast<int>((m_coefficients.size() - 1) / 2); }
private:
    std::vector<double> m_coefficients, m_history;
    std::size_t m_cursor = 0;
};
