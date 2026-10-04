#include "RotatorPeakSearch.h"

#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace mm {
namespace {

constexpr double kNegInfMetric = -999.0;
constexpr double kPhi = 1.6180339887498948482;
constexpr double kInvPhi = 1.0 / kPhi;

inline double normAz(double value)
{
    double az = std::fmod(value, 360.0);
    if (az < 0.0) az += 360.0;
    return az;
}

inline double signedAzDelta(double azimuthDeg, double centerDeg)
{
    double d = normAz(azimuthDeg) - normAz(centerDeg);
    while (d > 180.0) d -= 360.0;
    while (d < -180.0) d += 360.0;
    return d;
}

inline double clampOffset(double value, double halfSpan)
{
    return qBound(-qAbs(halfSpan), value, qAbs(halfSpan));
}

inline double clampElevation(const RotatorPeakSearch::SearchLimits &limits, double elevationDeg)
{
    const double span = qAbs(limits.elevationSpanDeg);
    return qBound(limits.centerElevationDeg - span,
                  elevationDeg,
                  limits.centerElevationDeg + span);
}

inline double clampAzimuth(const RotatorPeakSearch::SearchLimits &limits, double azimuthDeg)
{
    const double offset = clampOffset(signedAzDelta(azimuthDeg, limits.centerAzimuthDeg),
                                      limits.azimuthSpanDeg);
    return normAz(limits.centerAzimuthDeg + offset);
}

inline RotatorPeakSearch::Sample boundedSample(const RotatorPeakSearch::SearchLimits &limits,
                                                double az,
                                                double el,
                                                double metric = kNegInfMetric)
{
    RotatorPeakSearch::Sample s;
    s.azimuthDeg = clampAzimuth(limits, az);
    s.elevationDeg = limits.axisMode == RotatorPeakSearch::AxisMode::AzimuthElevation
        ? clampElevation(limits, el)
        : limits.centerElevationDeg;
    s.metricDb = metric;
    return s;
}

inline void updateBest(RotatorPeakSearch::Session *s, const RotatorPeakSearch::Sample &sample)
{
    if (sample.metricDb > s->bestMetricDb || s->evaluations <= 1) {
        s->bestMetricDb = sample.metricDb;
        s->bestAzimuthDeg = sample.azimuthDeg;
        s->bestElevationDeg = sample.elevationDeg;
    }
}

inline bool nearPoint(const RotatorPeakSearch::Sample &s, double az, double el, double tolerance)
{
    return std::fabs(signedAzDelta(s.azimuthDeg, az)) <= tolerance &&
           std::fabs(s.elevationDeg - el) <= tolerance;
}

RotatorPeakSearch::Recommendation makeRecommendation(const RotatorPeakSearch::Session &s,
                                                      const QString &status)
{
    RotatorPeakSearch::Recommendation r;
    r.valid = s.initialized;
    r.finished = s.finished;
    r.nextAzimuthDeg = s.pendingAzimuthDeg;
    r.nextElevationDeg = s.pendingElevationDeg;
    r.bestAzimuthDeg = s.bestAzimuthDeg;
    r.bestElevationDeg = s.bestElevationDeg;
    r.bestMetricDb = s.bestMetricDb;
    r.evaluations = s.evaluations;
    r.status = status;
    return r;
}

void setPending(RotatorPeakSearch::Session *s,
                const RotatorPeakSearch::Sample &point)
{
    s->pendingAzimuthDeg = point.azimuthDeg;
    s->pendingElevationDeg = point.elevationDeg;
    s->awaitingSample = true;
}

QVector<RotatorPeakSearch::Sample> patternProbes(const RotatorPeakSearch::Session &s)
{
    QVector<RotatorPeakSearch::Sample> p;
    p.reserve(s.limits.axisMode == RotatorPeakSearch::AxisMode::AzimuthElevation ? 4 : 2);
    p << boundedSample(s.limits, s.patternCenterAz - s.patternStepAz, s.patternCenterEl)
      << boundedSample(s.limits, s.patternCenterAz + s.patternStepAz, s.patternCenterEl);
    if (s.limits.axisMode == RotatorPeakSearch::AxisMode::AzimuthElevation) {
        p << boundedSample(s.limits, s.patternCenterAz, s.patternCenterEl - s.patternStepEl)
          << boundedSample(s.limits, s.patternCenterAz, s.patternCenterEl + s.patternStepEl);
    }
    return p;
}

void beginPatternCycle(RotatorPeakSearch::Session *s)
{
    s->patternCycle.clear();
    s->patternProbeIndex = -1;
    if (s->patternCenterMetric <= kNegInfMetric + 1.0) {
        setPending(s, boundedSample(s->limits, s->patternCenterAz, s->patternCenterEl));
        return;
    }
    s->patternProbeIndex = 0;
    const QVector<RotatorPeakSearch::Sample> probes = patternProbes(*s);
    if (!probes.isEmpty()) setPending(s, probes.first());
}

RotatorPeakSearch::Recommendation advancePattern(RotatorPeakSearch::Session *s,
                                                  const RotatorPeakSearch::Sample *sample)
{
    if (sample != nullptr) {
        updateBest(s, *sample);
        if (s->patternProbeIndex < 0) {
            s->patternCenterMetric = sample->metricDb;
            s->patternCenterAz = sample->azimuthDeg;
            s->patternCenterEl = sample->elevationDeg;
            s->patternProbeIndex = 0;
            const QVector<RotatorPeakSearch::Sample> probes = patternProbes(*s);
            if (!probes.isEmpty()) {
                setPending(s, probes.first());
                return makeRecommendation(*s, QStringLiteral("Pattern search: sampling -Az around the current centre."));
            }
        } else {
            s->patternCycle.append(*sample);
            const QVector<RotatorPeakSearch::Sample> probes = patternProbes(*s);
            ++s->patternProbeIndex;
            if (s->patternProbeIndex < probes.size()) {
                setPending(s, probes.at(s->patternProbeIndex));
                return makeRecommendation(*s, QStringLiteral("Pattern search: sampling coordinate neighbour %1/%2.")
                                               .arg(s->patternProbeIndex + 1)
                                               .arg(probes.size()));
            }

            const RotatorPeakSearch::Sample *cycleBest = nullptr;
            for (const auto &candidate : s->patternCycle) {
                if (cycleBest == nullptr || candidate.metricDb > cycleBest->metricDb) cycleBest = &candidate;
            }
            constexpr double kUsefulImprovementDb = 0.20;
            if (cycleBest != nullptr && cycleBest->metricDb > s->patternCenterMetric + kUsefulImprovementDb) {
                s->patternCenterAz = cycleBest->azimuthDeg;
                s->patternCenterEl = cycleBest->elevationDeg;
                s->patternCenterMetric = cycleBest->metricDb;
            } else {
                s->patternStepAz *= 0.5;
                s->patternStepEl *= 0.5;
            }

            const double tol = qMax(0.10, s->limits.toleranceDeg);
            const bool azDone = s->patternStepAz <= tol;
            const bool elDone = s->limits.axisMode == RotatorPeakSearch::AxisMode::AzimuthOnly || s->patternStepEl <= tol;
            if (azDone && elDone) {
                s->finished = true;
                s->awaitingSample = false;
                s->pendingAzimuthDeg = s->bestAzimuthDeg;
                s->pendingElevationDeg = s->bestElevationDeg;
                return makeRecommendation(*s, QStringLiteral("Pattern search converged at the best measured point."));
            }
            beginPatternCycle(s);
            return makeRecommendation(*s, QStringLiteral("Pattern search: new coordinate cycle with reduced/adapted step."));
        }
    }

    if (!s->awaitingSample) beginPatternCycle(s);
    return makeRecommendation(*s, QStringLiteral("Pattern search: measure the current centre."));
}

void goldenRecomputePoints(RotatorPeakSearch::Session *s)
{
    const double width = s->goldenHi - s->goldenLo;
    s->goldenX1 = s->goldenHi - kInvPhi * width;
    s->goldenX2 = s->goldenLo + kInvPhi * width;
}

RotatorPeakSearch::Sample goldenPoint(const RotatorPeakSearch::Session &s, double offset)
{
    return boundedSample(s.limits,
                         s.limits.centerAzimuthDeg + offset,
                         s.limits.centerElevationDeg);
}

RotatorPeakSearch::Recommendation advanceGolden(RotatorPeakSearch::Session *s,
                                                 const RotatorPeakSearch::Sample *sample)
{
    const double tol = qMax(0.10, s->limits.toleranceDeg);

    if (sample != nullptr) {
        updateBest(s, *sample);
        if (s->phase == RotatorPeakSearch::Session::Phase::GoldenAwaitX1) {
            s->goldenF1 = sample->metricDb;
            s->goldenHaveF1 = true;
        } else if (s->phase == RotatorPeakSearch::Session::Phase::GoldenAwaitX2) {
            s->goldenF2 = sample->metricDb;
            s->goldenHaveF2 = true;
        }
    }

    // Initial pair: measure only the points that are not already cached.
    if (!s->goldenHaveF1) {
        s->phase = RotatorPeakSearch::Session::Phase::GoldenAwaitX1;
        setPending(s, goldenPoint(*s, s->goldenX1));
        return makeRecommendation(*s, QStringLiteral("Golden section: measure the first interior azimuth."));
    }
    if (!s->goldenHaveF2) {
        s->phase = RotatorPeakSearch::Session::Phase::GoldenAwaitX2;
        setPending(s, goldenPoint(*s, s->goldenX2));
        return makeRecommendation(*s, QStringLiteral("Golden section: measure the second interior azimuth."));
    }

    // Both interior samples are known. Preserve the winning interior point and
    // evaluate only one new point after each interval reduction; this is the
    // defining low-evaluation property of golden-section search.
    if (s->goldenF1 < s->goldenF2) {
        s->goldenLo = s->goldenX1;
        s->goldenX1 = s->goldenX2;
        s->goldenF1 = s->goldenF2;
        s->goldenHaveF1 = true;
        s->goldenX2 = s->goldenLo + kInvPhi * (s->goldenHi - s->goldenLo);
        s->goldenHaveF2 = false;
    } else {
        s->goldenHi = s->goldenX2;
        s->goldenX2 = s->goldenX1;
        s->goldenF2 = s->goldenF1;
        s->goldenHaveF2 = true;
        s->goldenX1 = s->goldenHi - kInvPhi * (s->goldenHi - s->goldenLo);
        s->goldenHaveF1 = false;
    }

    if ((s->goldenHi - s->goldenLo) <= tol) {
        s->finished = true;
        s->awaitingSample = false;
        s->pendingAzimuthDeg = s->bestAzimuthDeg;
        s->pendingElevationDeg = s->bestElevationDeg;
        return makeRecommendation(*s, QStringLiteral("Golden-section azimuth search converged."));
    }

    if (!s->goldenHaveF1) {
        s->phase = RotatorPeakSearch::Session::Phase::GoldenAwaitX1;
        setPending(s, goldenPoint(*s, s->goldenX1));
    } else {
        s->phase = RotatorPeakSearch::Session::Phase::GoldenAwaitX2;
        setPending(s, goldenPoint(*s, s->goldenX2));
    }
    return makeRecommendation(*s, QStringLiteral("Golden section: interval reduced to %1°.")
                                   .arg(QString::number(s->goldenHi - s->goldenLo, 'f', 2)));
}

void sortSimplex(QVector<RotatorPeakSearch::Sample> *simplex)
{
    std::sort(simplex->begin(), simplex->end(), [](const auto &a, const auto &b) {
        return a.metricDb > b.metricDb; // maximise signal metric
    });
}

RotatorPeakSearch::Sample simplexCentroidBestTwo(const RotatorPeakSearch::Session &s)
{
    RotatorPeakSearch::Sample c;
    const auto &a = s.simplex.at(0);
    const auto &b = s.simplex.at(1);
    const double aOff = signedAzDelta(a.azimuthDeg, s.limits.centerAzimuthDeg);
    const double bOff = signedAzDelta(b.azimuthDeg, s.limits.centerAzimuthDeg);
    c.azimuthDeg = normAz(s.limits.centerAzimuthDeg + 0.5 * (aOff + bOff));
    c.elevationDeg = 0.5 * (a.elevationDeg + b.elevationDeg);
    c.metricDb = kNegInfMetric;
    return c;
}

RotatorPeakSearch::Sample affinePoint(const RotatorPeakSearch::Session &s,
                                       const RotatorPeakSearch::Sample &origin,
                                       const RotatorPeakSearch::Sample &towards,
                                       double factor)
{
    const double oAz = signedAzDelta(origin.azimuthDeg, s.limits.centerAzimuthDeg);
    const double tAz = signedAzDelta(towards.azimuthDeg, s.limits.centerAzimuthDeg);
    return boundedSample(s.limits,
                         s.limits.centerAzimuthDeg + oAz + factor * (tAz - oAz),
                         origin.elevationDeg + factor * (towards.elevationDeg - origin.elevationDeg));
}

bool simplexConverged(const RotatorPeakSearch::Session &s)
{
    if (s.simplex.size() != 3) return false;
    const double tol = qMax(0.10, s.limits.toleranceDeg);
    double maxAz = 0.0;
    double maxEl = 0.0;
    for (int i = 0; i < 3; ++i) {
        for (int j = i + 1; j < 3; ++j) {
            maxAz = qMax(maxAz, std::fabs(signedAzDelta(s.simplex.at(i).azimuthDeg, s.simplex.at(j).azimuthDeg)));
            maxEl = qMax(maxEl, std::fabs(s.simplex.at(i).elevationDeg - s.simplex.at(j).elevationDeg));
        }
    }
    return maxAz <= tol && maxEl <= tol;
}

RotatorPeakSearch::Recommendation beginNelderIteration(RotatorPeakSearch::Session *s)
{
    sortSimplex(&s->simplex);
    for (const auto &v : s->simplex) updateBest(s, v);
    if (simplexConverged(*s)) {
        s->finished = true;
        s->awaitingSample = false;
        s->pendingAzimuthDeg = s->bestAzimuthDeg;
        s->pendingElevationDeg = s->bestElevationDeg;
        return makeRecommendation(*s, QStringLiteral("Nelder-Mead simplex converged."));
    }

    const auto centroid = simplexCentroidBestTwo(*s);
    const auto worst = s->simplex.at(2);
    // reflection = centroid + (centroid - worst)
    s->nelderCandidate = affinePoint(*s, worst, centroid, 2.0);
    s->phase = RotatorPeakSearch::Session::Phase::NelderReflect;
    setPending(s, s->nelderCandidate);
    return makeRecommendation(*s, QStringLiteral("Nelder-Mead: reflect the worst simplex vertex."));
}

RotatorPeakSearch::Recommendation advanceNelder(RotatorPeakSearch::Session *s,
                                                 const RotatorPeakSearch::Sample *sample)
{
    if (sample != nullptr) {
        updateBest(s, *sample);
        switch (s->phase) {
        case RotatorPeakSearch::Session::Phase::NelderInit:
            if (s->nelderInitIndex >= 0 && s->nelderInitIndex < s->simplex.size()) {
                s->simplex[s->nelderInitIndex].metricDb = sample->metricDb;
            }
            ++s->nelderInitIndex;
            if (s->nelderInitIndex < 3) {
                setPending(s, s->simplex.at(s->nelderInitIndex));
                return makeRecommendation(*s, QStringLiteral("Nelder-Mead: measure initial simplex vertex %1/3.")
                                               .arg(s->nelderInitIndex + 1));
            }
            return beginNelderIteration(s);

        case RotatorPeakSearch::Session::Phase::NelderReflect: {
            sortSimplex(&s->simplex);
            s->nelderReflection = *sample;
            const double best = s->simplex.at(0).metricDb;
            const double second = s->simplex.at(1).metricDb;
            const double worst = s->simplex.at(2).metricDb;
            const auto centroid = simplexCentroidBestTwo(*s);
            if (sample->metricDb > best) {
                // expansion = centroid + 2 * (reflection - centroid)
                s->nelderCandidate = affinePoint(*s, centroid, *sample, 2.0);
                s->phase = RotatorPeakSearch::Session::Phase::NelderExpand;
                setPending(s, s->nelderCandidate);
                return makeRecommendation(*s, QStringLiteral("Nelder-Mead: reflected point improved the best; test expansion."));
            }
            if (sample->metricDb > second) {
                s->simplex[2] = *sample;
                return beginNelderIteration(s);
            }

            s->nelderOutsideContraction = sample->metricDb > worst;
            const auto target = s->nelderOutsideContraction ? *sample : s->simplex.at(2);
            s->nelderCandidate = affinePoint(*s, centroid, target, 0.5);
            s->phase = RotatorPeakSearch::Session::Phase::NelderContract;
            setPending(s, s->nelderCandidate);
            return makeRecommendation(*s, QStringLiteral("Nelder-Mead: reflection not sufficient; test contraction."));
        }

        case RotatorPeakSearch::Session::Phase::NelderExpand:
            sortSimplex(&s->simplex);
            s->simplex[2] = sample->metricDb > s->nelderReflection.metricDb ? *sample : s->nelderReflection;
            return beginNelderIteration(s);

        case RotatorPeakSearch::Session::Phase::NelderContract:
            sortSimplex(&s->simplex);
            {
                const double acceptanceFloor = s->nelderOutsideContraction
                    ? s->nelderReflection.metricDb
                    : s->simplex.at(2).metricDb;
                if (sample->metricDb > acceptanceFloor) {
                    s->simplex[2] = *sample;
                    return beginNelderIteration(s);
                }
            }
            // Shrink both non-best vertices halfway towards the best.
            s->nelderShrinkIndex = 1;
            s->simplex[1] = affinePoint(*s, s->simplex.at(0), s->simplex.at(1), 0.5);
            s->simplex[2] = affinePoint(*s, s->simplex.at(0), s->simplex.at(2), 0.5);
            s->phase = RotatorPeakSearch::Session::Phase::NelderShrink;
            setPending(s, s->simplex.at(1));
            return makeRecommendation(*s, QStringLiteral("Nelder-Mead: contraction failed; shrink simplex."));

        case RotatorPeakSearch::Session::Phase::NelderShrink:
            s->simplex[s->nelderShrinkIndex].metricDb = sample->metricDb;
            ++s->nelderShrinkIndex;
            if (s->nelderShrinkIndex <= 2) {
                setPending(s, s->simplex.at(2));
                return makeRecommendation(*s, QStringLiteral("Nelder-Mead: measure second shrunken vertex."));
            }
            return beginNelderIteration(s);

        default:
            break;
        }
    }

    if (!s->awaitingSample) {
        s->phase = RotatorPeakSearch::Session::Phase::NelderInit;
        s->nelderInitIndex = 0;
        setPending(s, s->simplex.first());
    }
    return makeRecommendation(*s, QStringLiteral("Nelder-Mead: measure initial simplex vertex 1/3."));
}

} // namespace

QVector<RotatorPeakSearch::AlgorithmInfo> RotatorPeakSearch::algorithms()
{
    return {
        { QStringLiteral("pattern-search"),
          QStringLiteral("Pattern search (recommended)"),
          QStringLiteral("Deterministic coordinate pattern search. Measures centre and +/-Az (+/-El on Alt-Az), moves only when the signal improves, then halves the step until mechanical tolerance is reached."),
          true, true },
        { QStringLiteral("golden-section"),
          QStringLiteral("Golden-section azimuth search"),
          QStringLiteral("True bounded one-dimensional golden-section maximisation. Intended for azimuth-only rotors and narrow terrestrial beams."),
          true, false },
        { QStringLiteral("nelder-mead"),
          QStringLiteral("Nelder-Mead simplex (Alt-Az)"),
          QStringLiteral("True two-dimensional three-vertex simplex with reflection, expansion, contraction and shrink operations. Intended for precise Alt-Az systems."),
          false, true }
    };
}

RotatorPeakSearch::AlgorithmInfo RotatorPeakSearch::algorithmById(const QString &id)
{
    const QString normalized = id.trimmed();
    for (const AlgorithmInfo &info : algorithms()) {
        if (info.id == normalized) return info;
    }
    return algorithms().first();
}

QString RotatorPeakSearch::normalizeAlgorithmId(const QString &id, AxisMode axisMode)
{
    QString normalized = id.trimmed().toLower();
    // Migrate historical 0.5.8/early-0.5.9 IDs.  The two prototype algorithms
    // were never real SPSA/extremum-seeking controllers; map them to the safe,
    // deterministic implementation rather than preserving misleading modes.
    if (normalized == QStringLiteral("bounded-adaptive") ||
        normalized == QStringLiteral("spsa") ||
        normalized == QStringLiteral("extremum-seeking")) {
        normalized = QStringLiteral("pattern-search");
    }
    if (normalized.isEmpty()) normalized = defaultAlgorithm(axisMode);
    if (!isCompatible(normalized, axisMode)) normalized = defaultAlgorithm(axisMode);
    return normalized;
}

bool RotatorPeakSearch::isCompatible(const QString &id, AxisMode axisMode)
{
    QString normalized = id.trimmed().toLower();
    if (normalized == QStringLiteral("bounded-adaptive") || normalized == QStringLiteral("spsa") || normalized == QStringLiteral("extremum-seeking")) {
        normalized = QStringLiteral("pattern-search");
    }
    for (const AlgorithmInfo &info : algorithms()) {
        if (info.id != normalized) continue;
        return axisMode == AxisMode::AzimuthElevation ? info.azimuthElevationCompatible : info.azimuthOnlyCompatible;
    }
    return false;
}

QString RotatorPeakSearch::defaultAlgorithm(AxisMode)
{
    // Mechanical rotators default to the least surprising, deterministic
    // algorithm on both axis configurations.
    return QStringLiteral("pattern-search");
}

void RotatorPeakSearch::reset(Session *s,
                              const QString &algorithmId,
                              const SearchLimits &limits)
{
    if (s == nullptr) return;
    *s = Session();
    s->limits = limits;
    s->limits.centerAzimuthDeg = normAz(limits.centerAzimuthDeg);
    s->limits.azimuthSpanDeg = qBound(0.10, qAbs(limits.azimuthSpanDeg), 180.0);
    s->limits.elevationSpanDeg = limits.axisMode == AxisMode::AzimuthElevation
        ? qBound(0.10, qAbs(limits.elevationSpanDeg), 180.0)
        : 0.0;
    s->limits.toleranceDeg = qBound(0.10, qAbs(limits.toleranceDeg), 5.0);
    s->algorithmId = normalizeAlgorithmId(algorithmId, s->limits.axisMode);
    s->initialized = true;
    s->bestAzimuthDeg = s->limits.centerAzimuthDeg;
    s->bestElevationDeg = s->limits.centerElevationDeg;

    if (s->algorithmId == QStringLiteral("golden-section")) {
        s->goldenLo = -s->limits.azimuthSpanDeg;
        s->goldenHi = s->limits.azimuthSpanDeg;
        goldenRecomputePoints(s);
        s->phase = Session::Phase::GoldenAwaitX1;
    } else if (s->algorithmId == QStringLiteral("nelder-mead")) {
        const double stepAz = qMax(s->limits.toleranceDeg * 2.0, s->limits.azimuthSpanDeg * 0.5);
        const double stepEl = qMax(s->limits.toleranceDeg * 2.0, s->limits.elevationSpanDeg * 0.5);
        s->simplex = {
            boundedSample(s->limits, s->limits.centerAzimuthDeg, s->limits.centerElevationDeg),
            boundedSample(s->limits, s->limits.centerAzimuthDeg + stepAz, s->limits.centerElevationDeg),
            boundedSample(s->limits, s->limits.centerAzimuthDeg, s->limits.centerElevationDeg + stepEl)
        };
        s->phase = Session::Phase::NelderInit;
        s->nelderInitIndex = 0;
    } else {
        s->algorithmId = QStringLiteral("pattern-search");
        s->patternCenterAz = s->limits.centerAzimuthDeg;
        s->patternCenterEl = s->limits.centerElevationDeg;
        s->patternStepAz = qMax(s->limits.toleranceDeg * 2.0, s->limits.azimuthSpanDeg * 0.5);
        s->patternStepEl = s->limits.axisMode == AxisMode::AzimuthElevation
            ? qMax(s->limits.toleranceDeg * 2.0, s->limits.elevationSpanDeg * 0.5)
            : 0.0;
        s->phase = Session::Phase::PatternAwait;
    }
}

RotatorPeakSearch::Recommendation RotatorPeakSearch::advance(Session *s,
                                                              const Sample *completedSample)
{
    if (s == nullptr || !s->initialized) return Recommendation();
    if (s->finished) return makeRecommendation(*s, QStringLiteral("Peak search already converged."));

    if (completedSample != nullptr) {
        if (!s->awaitingSample) {
            Recommendation r = makeRecommendation(*s, QStringLiteral("Unexpected peak-search sample ignored: no measurement was pending."));
            r.valid = false;
            return r;
        }
        const double tolerance = qMax(0.75, s->limits.toleranceDeg * 2.0);
        if (!nearPoint(*completedSample, s->pendingAzimuthDeg, s->pendingElevationDeg, tolerance)) {
            Recommendation r = makeRecommendation(*s, QStringLiteral("Peak-search sample ignored: measured point does not match the requested probe."));
            r.valid = false;
            return r;
        }
        s->awaitingSample = false;
        ++s->evaluations;
    }

    if (s->algorithmId == QStringLiteral("golden-section")) {
        return advanceGolden(s, completedSample);
    }
    if (s->algorithmId == QStringLiteral("nelder-mead")) {
        return advanceNelder(s, completedSample);
    }
    return advancePattern(s, completedSample);
}

} // namespace mm
