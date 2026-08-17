#include "Nco.h"

#include <cmath>

namespace {
constexpr double kTwoPi = 6.283185307179586476925286766559;
}

void Nco::configure(double sampleRate, double frequencyHz)
{
    m_sampleRate = sampleRate > 1.0 ? sampleRate : 8000.0;
    m_frequencyHz = frequencyHz;
    rebuildIncrement();
}

void Nco::reset(double phaseRadians)
{
    m_phase = std::fmod(phaseRadians, kTwoPi);
    if (m_phase < 0.0) {
        m_phase += kTwoPi;
    }
}

void Nco::setFrequency(double frequencyHz)
{
    m_frequencyHz = frequencyHz;
    rebuildIncrement();
}

double Nco::frequencyHz() const
{
    return m_frequencyHz;
}

double Nco::phaseRadians() const
{
    return m_phase;
}

void Nco::mixDown(double sample, double *i, double *q, double extraPhaseStep)
{
    if (i != nullptr) {
        *i = sample * std::cos(m_phase);
    }
    if (q != nullptr) {
        *q = -sample * std::sin(m_phase);
    }
    advance(extraPhaseStep);
}

void Nco::advance(double extraPhaseStep)
{
    m_phase += m_increment + extraPhaseStep;
    while (m_phase >= kTwoPi) {
        m_phase -= kTwoPi;
    }
    while (m_phase < 0.0) {
        m_phase += kTwoPi;
    }
}

void Nco::rebuildIncrement()
{
    m_increment = kTwoPi * m_frequencyHz / m_sampleRate;
}
