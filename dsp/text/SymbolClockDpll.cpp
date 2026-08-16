#include "SymbolClockDpll.h"

#include <algorithm>

void SymbolClockDpll::configure(double samplesPerSymbol)
{
    m_samplesPerSymbol = std::max(4.0, samplesPerSymbol);
    if (m_countdown <= 0.0 || m_countdown > m_samplesPerSymbol * 2.0) {
        m_countdown = m_samplesPerSymbol;
    }
}

void SymbolClockDpll::reset(double initialDelaySymbols)
{
    m_countdown = m_samplesPerSymbol * std::max(0.05, initialDelaySymbols);
}

bool SymbolClockDpll::tick()
{
    m_countdown -= 1.0;
    if (m_countdown <= 0.0) {
        m_countdown += m_samplesPerSymbol;
        return true;
    }
    return false;
}

void SymbolClockDpll::nudge(double samples)
{
    const double limit = m_samplesPerSymbol * 0.08;
    const double bounded = std::max(-limit, std::min(samples, limit));
    m_countdown = std::max(1.0, std::min(m_countdown + bounded, m_samplesPerSymbol * 1.50));
}

double SymbolClockDpll::samplesPerSymbol() const
{
    return m_samplesPerSymbol;
}

double SymbolClockDpll::countdown() const
{
    return m_countdown;
}
