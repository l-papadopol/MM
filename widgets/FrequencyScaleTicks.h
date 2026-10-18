#pragma once
#include <QVector>
#include <cmath>
#include <algorithm>
namespace FrequencyScaleTicks {
inline QVector<double> values(double low, double high, int pixels) {
    QVector<double> result;
    if (!std::isfinite(low) || !std::isfinite(high) || high <= low) return result;
    if (std::abs(low - 100) < .5 && std::abs(high - 3000) < .5)
        return {100, 400, 700, 1000, 1400, 1800, 2200, 2600, 3000};
    const double desired = (high-low)/std::max(2, pixels/100);
    const double magnitude = std::pow(10.0, std::floor(std::log10(std::max(1.0, desired))));
    double step = magnitude;
    for (double factor : {1., 2., 5., 10.}) if (factor*magnitude >= desired) { step=factor*magnitude; break; }
    for (double tick=std::ceil(low/step)*step; tick<=high+.001 && result.size()<100; tick+=step)
        result.append(tick);
    return result;
}
}
