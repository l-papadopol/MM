#include "RotatorPeakSearch.h"

#include <QtGlobal>
#include <QVector>
#include <QPair>
#include <algorithm>
#include <cmath>

namespace mm {
namespace {

double clamp(double value, double lo, double hi)
{
    return qBound(lo, value, hi);
}

double normAz(double value)
{
    while (value < 0.0) value += 360.0;
    while (value >= 360.0) value -= 360.0;
    return value;
}

double azError(double a, double b)
{
    double d = std::fabs(normAz(a) - normAz(b));
    return d > 180.0 ? 360.0 - d : d;
}

bool sampledNear(const QVector<RotatorPeakSearch::Sample> &samples, double az, double el, double tolDeg = 0.35)
{
    for (const RotatorPeakSearch::Sample &s : samples) {
        if (azError(s.azimuthDeg, az) <= tolDeg && std::fabs(s.elevationDeg - el) <= tolDeg) {
            return true;
        }
    }
    return false;
}

RotatorPeakSearch::Recommendation bestKnown(const RotatorPeakSearch::SearchLimits &limits,
                                             const QVector<RotatorPeakSearch::Sample> &samples)
{
    RotatorPeakSearch::Recommendation r;
    r.valid = true;
    r.bestAzimuthDeg = normAz(limits.centerAzimuthDeg);
    r.bestElevationDeg = limits.centerElevationDeg;
    r.nextAzimuthDeg = r.bestAzimuthDeg;
    r.nextElevationDeg = r.bestElevationDeg;
    r.status = QStringLiteral("No samples yet; start from theoretical bearing.");
    if (samples.isEmpty()) {
        return r;
    }

    const RotatorPeakSearch::Sample *best = &samples.first();
    for (const RotatorPeakSearch::Sample &s : samples) {
        if (s.metricDb > best->metricDb) {
            best = &s;
        }
    }
    r.bestAzimuthDeg = best->azimuthDeg;
    r.bestElevationDeg = best->elevationDeg;
    r.bestMetricDb = best->metricDb;
    r.nextAzimuthDeg = best->azimuthDeg;
    r.nextElevationDeg = best->elevationDeg;
    r.status = QStringLiteral("Best measured point selected.");
    return r;
}

QVector<QPair<double, double>> boundedPattern(const RotatorPeakSearch::SearchLimits &limits,
                                              const QVector<RotatorPeakSearch::Sample> &samples,
                                              const RotatorPeakSearch::Recommendation &best)
{
    const double baseStepAz = qMax(0.5, qAbs(limits.azimuthSpanDeg) / 2.0);
    const double shrink = std::pow(0.55, qMax(0, samples.size() / (limits.axisMode == RotatorPeakSearch::AxisMode::AzimuthElevation ? 6 : 3)));
    const double stepAz = qMax(0.4, baseStepAz * shrink);
    const double stepEl = qMax(0.4, qAbs(limits.elevationSpanDeg) * 0.5 * shrink);
    QVector<QPair<double, double>> probes;
    probes << qMakePair(limits.centerAzimuthDeg, limits.centerElevationDeg)
           << qMakePair(best.bestAzimuthDeg - stepAz, best.bestElevationDeg)
           << qMakePair(best.bestAzimuthDeg + stepAz, best.bestElevationDeg);
    if (limits.axisMode == RotatorPeakSearch::AxisMode::AzimuthElevation && qAbs(limits.elevationSpanDeg) > 0.0) {
        probes << qMakePair(best.bestAzimuthDeg, best.bestElevationDeg - stepEl)
               << qMakePair(best.bestAzimuthDeg, best.bestElevationDeg + stepEl)
               << qMakePair(best.bestAzimuthDeg - stepAz * 0.7, best.bestElevationDeg - stepEl * 0.7)
               << qMakePair(best.bestAzimuthDeg + stepAz * 0.7, best.bestElevationDeg + stepEl * 0.7);
    }
    return probes;
}

QVector<QPair<double, double>> goldenProbes(const RotatorPeakSearch::SearchLimits &limits)
{
    constexpr double invPhi = 0.6180339887498948482;
    const double span = qAbs(limits.azimuthSpanDeg);
    return {
        qMakePair(limits.centerAzimuthDeg, limits.centerElevationDeg),
        qMakePair(limits.centerAzimuthDeg - span * invPhi, limits.centerElevationDeg),
        qMakePair(limits.centerAzimuthDeg + span * invPhi, limits.centerElevationDeg),
        qMakePair(limits.centerAzimuthDeg - span * (1.0 - invPhi), limits.centerElevationDeg),
        qMakePair(limits.centerAzimuthDeg + span * (1.0 - invPhi), limits.centerElevationDeg)
    };
}

QVector<QPair<double, double>> nelderMeadProbes(const RotatorPeakSearch::SearchLimits &limits,
                                                const QVector<RotatorPeakSearch::Sample> &samples,
                                                const RotatorPeakSearch::Recommendation &best)
{
    const double stepAz = qMax(0.5, qAbs(limits.azimuthSpanDeg) / 2.0);
    const double stepEl = qMax(0.5, qAbs(limits.elevationSpanDeg) / 2.0);
    QVector<QPair<double, double>> probes;
    probes << qMakePair(limits.centerAzimuthDeg, limits.centerElevationDeg)
           << qMakePair(limits.centerAzimuthDeg + stepAz, limits.centerElevationDeg)
           << qMakePair(limits.centerAzimuthDeg, limits.centerElevationDeg + stepEl)
           << qMakePair(limits.centerAzimuthDeg - stepAz, limits.centerElevationDeg)
           << qMakePair(limits.centerAzimuthDeg, limits.centerElevationDeg - stepEl);

    if (samples.size() >= 3) {
        QVector<RotatorPeakSearch::Sample> sorted = samples;
        std::sort(sorted.begin(), sorted.end(), [](const RotatorPeakSearch::Sample &a, const RotatorPeakSearch::Sample &b) {
            return a.metricDb > b.metricDb;
        });
        const int usable = qMin(3, sorted.size());
        double centroidAz = 0.0;
        double centroidEl = 0.0;
        for (int i = 0; i < usable - 1; ++i) {
            centroidAz += sorted.at(i).azimuthDeg;
            centroidEl += sorted.at(i).elevationDeg;
        }
        centroidAz /= qMax(1, usable - 1);
        centroidEl /= qMax(1, usable - 1);
        const RotatorPeakSearch::Sample worst = sorted.at(usable - 1);
        probes.prepend(qMakePair(centroidAz + (centroidAz - worst.azimuthDeg), centroidEl + (centroidEl - worst.elevationDeg)));
        probes.append(qMakePair(best.bestAzimuthDeg + (best.bestAzimuthDeg - centroidAz) * 0.5,
                                best.bestElevationDeg + (best.bestElevationDeg - centroidEl) * 0.5));
    }
    return probes;
}

QVector<QPair<double, double>> spsaProbes(const RotatorPeakSearch::SearchLimits &limits,
                                          const QVector<RotatorPeakSearch::Sample> &samples,
                                          const RotatorPeakSearch::Recommendation &best)
{
    const int k = samples.size() + 1;
    const double gainAz = qMax(0.4, qAbs(limits.azimuthSpanDeg) / std::sqrt(static_cast<double>(k + 1)));
    const double gainEl = qMax(0.4, qAbs(limits.elevationSpanDeg) / std::sqrt(static_cast<double>(k + 1)));
    const double signAz = (k % 2) ? 1.0 : -1.0;
    const double signEl = ((k / 2) % 2) ? 1.0 : -1.0;
    return {
        qMakePair(best.bestAzimuthDeg + signAz * gainAz, best.bestElevationDeg + signEl * gainEl),
        qMakePair(best.bestAzimuthDeg - signAz * gainAz, best.bestElevationDeg - signEl * gainEl),
        qMakePair(best.bestAzimuthDeg, best.bestElevationDeg)
    };
}

QVector<QPair<double, double>> extremumProbes(const RotatorPeakSearch::SearchLimits &limits,
                                              const QVector<RotatorPeakSearch::Sample> &samples,
                                              const RotatorPeakSearch::Recommendation &best)
{
    const int k = samples.size();
    const double ampAz = qMax(0.3, qAbs(limits.azimuthSpanDeg) / 4.0);
    const double ampEl = qMax(0.3, qAbs(limits.elevationSpanDeg) / 4.0);
    const double phase = static_cast<double>(k) * 1.5707963267948966;
    return { qMakePair(best.bestAzimuthDeg + std::cos(phase) * ampAz,
                       best.bestElevationDeg + (limits.axisMode == RotatorPeakSearch::AxisMode::AzimuthElevation ? std::sin(phase) * ampEl : 0.0)) };
}

} // namespace

QVector<RotatorPeakSearch::AlgorithmInfo> RotatorPeakSearch::algorithms()
{
    return {
        { QStringLiteral("bounded-adaptive"),
          QStringLiteral("Bounded adaptive peak search"),
          QStringLiteral("Coarse/fine pattern search inside the configured beam span. Safe first choice for FT QSOs."),
          true, true, false, false },
        { QStringLiteral("golden-section"),
          QStringLiteral("Golden-section line search"),
          QStringLiteral("Efficient one-dimensional search along azimuth. Best for azimuth-only rotors."),
          true, false, false, false },
        { QStringLiteral("nelder-mead"),
          QStringLiteral("Nelder-Mead simplex"),
          QStringLiteral("Derivative-free two-axis simplex-style search for AZ/EL systems."),
          false, true, false, false },
        { QStringLiteral("spsa"),
          QStringLiteral("SPSA stochastic search"),
          QStringLiteral("Noise-tolerant paired perturbation search for weak/noisy AZ/EL signals."),
          false, true, false, true },
        { QStringLiteral("extremum-seeking"),
          QStringLiteral("Extremum seeking / dither tracking"),
          QStringLiteral("Continuous bounded dither around the current best point. Advanced mode."),
          true, true, true, true }
    };
}

RotatorPeakSearch::AlgorithmInfo RotatorPeakSearch::algorithmById(const QString &id)
{
    for (const AlgorithmInfo &info : algorithms()) {
        if (info.id == id) return info;
    }
    return algorithms().first();
}

bool RotatorPeakSearch::isCompatible(const QString &id, AxisMode axisMode)
{
    const AlgorithmInfo info = algorithmById(id);
    return axisMode == AxisMode::AzimuthElevation ? info.azimuthElevationCompatible : info.azimuthOnlyCompatible;
}

QString RotatorPeakSearch::defaultAlgorithm(AxisMode axisMode)
{
    return axisMode == AxisMode::AzimuthElevation ? QStringLiteral("nelder-mead") : QStringLiteral("bounded-adaptive");
}

RotatorPeakSearch::Recommendation RotatorPeakSearch::recommendNextStep(const QString &algorithmId,
                                                                        const SearchLimits &limits,
                                                                        const QVector<Sample> &samples,
                                                                        double currentAzimuthDeg,
                                                                        double currentElevationDeg)
{
    Recommendation r = bestKnown(limits, samples);
    const QString id = algorithmId.trimmed().isEmpty() ? defaultAlgorithm(limits.axisMode) : algorithmId.trimmed();
    if (!isCompatible(id, limits.axisMode)) {
        r.status = QStringLiteral("Selected algorithm is not compatible with this rotator axis mode.");
        return r;
    }

    const double azLo = limits.centerAzimuthDeg - qAbs(limits.azimuthSpanDeg);
    const double azHi = limits.centerAzimuthDeg + qAbs(limits.azimuthSpanDeg);
    const double elLo = limits.centerElevationDeg - qAbs(limits.elevationSpanDeg);
    const double elHi = limits.centerElevationDeg + qAbs(limits.elevationSpanDeg);

    QVector<QPair<double, double>> probes;
    if (id == QStringLiteral("golden-section")) {
        probes = goldenProbes(limits);
        r.status = QStringLiteral("Golden-section azimuth line-search probe.");
    } else if (id == QStringLiteral("nelder-mead")) {
        probes = nelderMeadProbes(limits, samples, r);
        r.status = QStringLiteral("Nelder-Mead simplex reflection/expansion probe.");
    } else if (id == QStringLiteral("spsa")) {
        probes = spsaProbes(limits, samples, r);
        r.status = QStringLiteral("SPSA paired stochastic perturbation probe.");
    } else if (id == QStringLiteral("extremum-seeking")) {
        probes = extremumProbes(limits, samples, r);
        r.status = QStringLiteral("Extremum-seeking bounded dither probe.");
    } else {
        probes = boundedPattern(limits, samples, r);
        r.status = QStringLiteral("Bounded adaptive coarse/fine pattern probe.");
    }

    for (const QPair<double, double> &probe : probes) {
        const double az = normAz(clamp(probe.first, azLo, azHi));
        const double el = clamp(probe.second, elLo, elHi);
        if (!sampledNear(samples, az, el)) {
            r.nextAzimuthDeg = az;
            r.nextElevationDeg = limits.axisMode == AxisMode::AzimuthElevation ? el : currentElevationDeg;
            return r;
        }
    }

    // All planned probes were already measured.  Return to the current best and let the caller stop if improvement is below threshold.
    r.nextAzimuthDeg = normAz(clamp(r.bestAzimuthDeg, azLo, azHi));
    r.nextElevationDeg = limits.axisMode == AxisMode::AzimuthElevation ? clamp(r.bestElevationDeg, elLo, elHi) : currentElevationDeg;
    r.status += QStringLiteral(" All scheduled probes measured; hold best point or stop by threshold.");
    Q_UNUSED(currentAzimuthDeg)
    return r;
}

} // namespace mm
