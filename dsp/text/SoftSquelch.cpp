#include "SoftSquelch.h"

void SoftSquelch::configure(double openThreshold, double closeThreshold, double attack, double release)
{
    m_openThreshold = openThreshold;
    m_closeThreshold = closeThreshold;
    m_attack = attack;
    m_release = release;
}

void SoftSquelch::reset()
{
    m_score = 0.0;
    m_open = false;
}

bool SoftSquelch::update(bool candidate)
{
    const double alpha = candidate ? m_attack : m_release;
    const double target = candidate ? 1.0 : 0.0;
    m_score = ((1.0 - alpha) * m_score) + (alpha * target);
    if (!m_open && m_score >= m_openThreshold) {
        m_open = true;
    } else if (m_open && m_score <= m_closeThreshold) {
        m_open = false;
    }
    return m_open;
}

bool SoftSquelch::isOpen() const
{
    return m_open;
}

double SoftSquelch::score() const
{
    return m_score;
}

void SoftSquelch::forceClosed()
{
    m_score = 0.0;
    m_open = false;
}
