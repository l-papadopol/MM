#include "AtcDetector.h"

#include <algorithm>
#include <cmath>

void AtcDetector::reset()
{
    m_markReference = 1.0e-9;
    m_spaceReference = 1.0e-9;
    m_noiseFloor = 1.0e-10;
    m_presenceScore = 0.0;
}

AtcDecision AtcDetector::process(double markEnergy, double spaceEnergy, double inputPower)
{
    markEnergy = std::max(markEnergy, 1.0e-14);
    spaceEnergy = std::max(spaceEnergy, 1.0e-14);
    inputPower = std::max(inputPower, 1.0e-14);

    const double totalEnergy = markEnergy + spaceEnergy;

    /* Slow independent references track selective fading without following bit
     * transitions too quickly.  Stronger tone updates faster than weaker tone. */
    const double markAttack = markEnergy > m_markReference ? 0.0015 : 0.00020;
    const double spaceAttack = spaceEnergy > m_spaceReference ? 0.0015 : 0.00020;
    m_markReference = ((1.0 - markAttack) * m_markReference) + (markAttack * markEnergy);
    m_spaceReference = ((1.0 - spaceAttack) * m_spaceReference) + (spaceAttack * spaceEnergy);

    if (totalEnergy < m_noiseFloor * 2.0) {
        m_noiseFloor = (0.995 * m_noiseFloor) + (0.005 * totalEnergy);
    } else {
        m_noiseFloor = (0.99995 * m_noiseFloor) + (0.00005 * std::min(totalEnergy, m_noiseFloor * 4.0));
    }

    const double markNorm = markEnergy / std::max(m_markReference, 1.0e-14);
    const double spaceNorm = spaceEnergy / std::max(m_spaceReference, 1.0e-14);
    const double normSum = markNorm + spaceNorm + 1.0e-14;
    const double soft = (markNorm - spaceNorm) / normSum;
    const double quality = std::min(1.0, std::abs(soft) * 1.35);
    const double snrLike = totalEnergy / std::max(m_noiseFloor, 1.0e-14);
    const double toneFraction = totalEnergy / std::max(inputPower, 1.0e-14);
    const double balance = std::min(markEnergy, spaceEnergy) / std::max(std::max(markEnergy, spaceEnergy), 1.0e-14);

    const bool candidate = (snrLike > 5.0) && (toneFraction > 0.030) && (quality > 0.075);
    const double target = candidate ? 1.0 : 0.0;
    const double alpha = candidate ? 0.0035 : 0.0012;
    m_presenceScore = ((1.0 - alpha) * m_presenceScore) + (alpha * target);

    AtcDecision d;
    d.soft = soft;
    d.quality = quality;
    d.totalEnergy = totalEnergy;
    d.snrLike = snrLike;
    d.balance = balance;
    d.carrierPresent = m_presenceScore > 0.36;
    return d;
}
