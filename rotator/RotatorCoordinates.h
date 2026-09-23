#pragma once
#include <cmath>
#include <initializer_list>

namespace mm {
// A one-turn mechanical geometry has equivalent bearings; a multi-turn target
// carries cable/turn information and must never be silently reduced modulo 360.
inline bool backendAzimuth(double mechanical, double mechanicalSpan,
                           double backendMin, double backendMax, double &result) {
    if (!std::isfinite(mechanical) || !std::isfinite(mechanicalSpan) ||
        !std::isfinite(backendMin) || !std::isfinite(backendMax) || backendMax < backendMin) return false;
    if (mechanical >= backendMin && mechanical <= backendMax) { result = mechanical; return true; }
    if (mechanicalSpan > 360.0) return false;
    double bearing = std::fmod(mechanical, 360.0);
    if (bearing < 0) bearing += 360.0;
    for (int turn : {0, -1, 1}) {
        const double candidate = bearing + turn * 360.0;
        if (candidate >= backendMin && candidate <= backendMax) { result = candidate; return true; }
    }
    return false;
}
}
