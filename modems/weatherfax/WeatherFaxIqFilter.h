// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

// Fourth-order Butterworth baseband low-pass. Applying identical coefficients
// to I and Q preserves the phase relationship used by the FM discriminator.
// Two biquads reject the mixing image while retaining +/-400 Hz deviation
// and useful video sidebands. State persists across audio capture blocks.
class WeatherFaxIqFilter
{
public:
    void configure(int rate, double cutoff)
    {
        rate = std::max(1, rate);
        cutoff = std::max(1.0, std::min(cutoff, rate * 0.2));
        if (rate == m_rate && cutoff == m_cutoff)
            return;
        const bool newRate = rate != m_rate;
        m_rate = rate;
        m_cutoff = cutoff;
        const double w = 6.283185307179586 * cutoff / rate;
        const double cosine = std::cos(w), sine = std::sin(w);
        const double qualities[2] = {0.541196100146197, 1.306562964876377};
        for (int k = 0; k < 2; ++k) {
            auto &s = m_sections[k];
            const double alpha = sine / (2 * qualities[k]);
            const double a0 = 1 + alpha;
            s.b0 = (1 - cosine) / (2 * a0);
            s.b1 = 2 * s.b0;
            s.b2 = s.b0;
            s.a1 = -2 * cosine / a0;
            s.a2 = (1 - alpha) / a0;
        }
        if (newRate)
            reset();
    }

    void reset()
    {
        for (auto &s : m_sections)
            s.i1 = s.i2 = s.q1 = s.q2 = 0;
    }

    void process(double &i, double &q)
    {
        for (auto &s : m_sections) {
            step(i, s.i1, s.i2, s);
            step(q, s.q1, s.q2, s);
        }
    }

private:
    struct Section {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double i1 = 0, i2 = 0, q1 = 0, q2 = 0;
    };
    static void step(double &x, double &z1, double &z2, const Section &s)
    {
        const double y = s.b0 * x + z1;
        z1 = s.b1 * x - s.a1 * y + z2;
        z2 = s.b2 * x - s.a2 * y;
        x = y;
    }
    std::array<Section, 2> m_sections;
    int m_rate = 0;
    double m_cutoff = 0;
};
