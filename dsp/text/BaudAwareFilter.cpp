#include "BaudAwareFilter.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double kTwoPi = 6.283185307179586476925286766559;

double clampDouble(double v, double lo, double hi)
{
    return std::max(lo, std::min(v, hi));
}
}

void BaudAwareLowPass::configure(double sampleRate,
                                 double symbolRate,
                                 double rolloff,
                                 double minCutoffHz,
                                 double maxCutoffHz,
                                 int stages)
{
    const double sr = sampleRate > 1.0 ? sampleRate : 8000.0;
    const double baud = symbolRate > 1.0 ? symbolRate : 31.25;
    const double nyquistHz = baud * 0.5;
    m_cutoffHz = clampDouble(nyquistHz * rolloff, minCutoffHz, maxCutoffHz);
    // High-rate PSK variants need a much faster one-pole response than
    // BPSK31/63/125.  Existing narrow modes keep the same alpha because their
    // cutoff is low; the wider ceiling only affects PSK250 and above.
    m_alpha = clampDouble(1.0 - std::exp(-kTwoPi * m_cutoffHz / sr), 0.00005, 0.80000);
    m_stages = std::max(1, std::min(stages, 4));
}

void BaudAwareLowPass::reset()
{
    m_z1 = 0.0;
    m_z2 = 0.0;
    m_z3 = 0.0;
    m_z4 = 0.0;
}

double BaudAwareLowPass::process(double x)
{
    m_z1 += m_alpha * (x - m_z1);
    if (m_stages <= 1) {
        return m_z1;
    }
    m_z2 += m_alpha * (m_z1 - m_z2);
    if (m_stages <= 2) {
        return m_z2;
    }
    m_z3 += m_alpha * (m_z2 - m_z3);
    if (m_stages <= 3) {
        return m_z3;
    }
    m_z4 += m_alpha * (m_z3 - m_z4);
    return m_z4;
}

double BaudAwareLowPass::cutoffHz() const
{
    return m_cutoffHz;
}

double BaudAwareLowPass::alpha() const
{
    return m_alpha;
}

void BaudAwareComplexLowPass::configure(double sampleRate,
                                        double symbolRate,
                                        double rolloff,
                                        double minCutoffHz,
                                        double maxCutoffHz,
                                        int stages)
{
    m_i.configure(sampleRate, symbolRate, rolloff, minCutoffHz, maxCutoffHz, stages);
    m_q.configure(sampleRate, symbolRate, rolloff, minCutoffHz, maxCutoffHz, stages);
}

void BaudAwareComplexLowPass::reset()
{
    m_i.reset();
    m_q.reset();
}

void BaudAwareComplexLowPass::process(double iIn, double qIn, double *iOut, double *qOut)
{
    if (iOut != nullptr) {
        *iOut = m_i.process(iIn);
    }
    if (qOut != nullptr) {
        *qOut = m_q.process(qIn);
    }
}

double BaudAwareComplexLowPass::cutoffHz() const
{
    return m_i.cutoffHz();
}
