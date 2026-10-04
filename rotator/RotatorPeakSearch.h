#ifndef ROTATORPEAKSEARCH_H
#define ROTATORPEAKSEARCH_H

#include <QString>
#include <QVector>

namespace mm {

class RotatorPeakSearch final
{
public:
    enum class AxisMode
    {
        AzimuthOnly,
        AzimuthElevation
    };

    struct AlgorithmInfo
    {
        QString id;
        QString displayName;
        QString description;
        bool azimuthOnlyCompatible = true;
        bool azimuthElevationCompatible = true;
    };

    struct SearchLimits
    {
        double centerAzimuthDeg = 0.0;
        double centerElevationDeg = 0.0;
        // Half-spans around the theoretical/current centre.
        double azimuthSpanDeg = 30.0;
        double elevationSpanDeg = 0.0;
        AxisMode axisMode = AxisMode::AzimuthOnly;
        // Mechanical convergence floor.  A search never asks the rotator to
        // resolve indefinitely smaller moves than this.
        double toleranceDeg = 0.35;
    };

    struct Sample
    {
        double azimuthDeg = 0.0;
        double elevationDeg = 0.0;
        double metricDb = -999.0;
    };

    struct Recommendation
    {
        bool valid = false;
        bool finished = false;
        double nextAzimuthDeg = 0.0;
        double nextElevationDeg = 0.0;
        double bestAzimuthDeg = 0.0;
        double bestElevationDeg = 0.0;
        double bestMetricDb = -999.0;
        int evaluations = 0;
        QString status;
    };

    // Stateful optimiser session.  The caller moves to the returned point,
    // measures exactly one metric there, then calls advance() with that sample.
    // No hidden timers, random generators or parallel recovery paths exist.
    struct Session
    {
        enum class Phase
        {
            Idle,
            PatternAwait,
            GoldenAwaitX1,
            GoldenAwaitX2,
            NelderInit,
            NelderReflect,
            NelderExpand,
            NelderContract,
            NelderShrink
        };

        QString algorithmId;
        SearchLimits limits;
        bool initialized = false;
        bool finished = false;
        bool awaitingSample = false;
        int evaluations = 0;
        Phase phase = Phase::Idle;

        double pendingAzimuthDeg = 0.0;
        double pendingElevationDeg = 0.0;
        double bestAzimuthDeg = 0.0;
        double bestElevationDeg = 0.0;
        double bestMetricDb = -999.0;

        // Pattern-search state.
        double patternCenterAz = 0.0;
        double patternCenterEl = 0.0;
        double patternCenterMetric = -999.0;
        double patternStepAz = 0.0;
        double patternStepEl = 0.0;
        int patternProbeIndex = -1;
        QVector<Sample> patternCycle;

        // Golden-section state, expressed as azimuth offsets from the fixed
        // search centre so wrap-around never corrupts the interval.
        double goldenLo = 0.0;
        double goldenHi = 0.0;
        double goldenX1 = 0.0;
        double goldenX2 = 0.0;
        double goldenF1 = -999.0;
        double goldenF2 = -999.0;
        bool goldenHaveF1 = false;
        bool goldenHaveF2 = false;

        // Nelder-Mead state: exactly three vertices for a 2-D simplex.
        QVector<Sample> simplex;
        int nelderInitIndex = 0;
        Sample nelderCandidate;
        Sample nelderReflection;
        bool nelderOutsideContraction = false;
        int nelderShrinkIndex = 0;
    };

    static QVector<AlgorithmInfo> algorithms();
    static AlgorithmInfo algorithmById(const QString &id);
    static QString normalizeAlgorithmId(const QString &id, AxisMode axisMode);
    static bool isCompatible(const QString &id, AxisMode axisMode);
    static QString defaultAlgorithm(AxisMode axisMode);

    static void reset(Session *session,
                      const QString &algorithmId,
                      const SearchLimits &limits);

    // Start or continue one optimiser.  completedSample must be null for the
    // first call after reset(), then must describe the point returned by the
    // preceding Recommendation.  The API is deliberately sequential because
    // a real rotator can only occupy one physical point at a time.
    static Recommendation advance(Session *session,
                                  const Sample *completedSample = nullptr);

private:
    RotatorPeakSearch() = delete;
};

} // namespace mm

#endif // ROTATORPEAKSEARCH_H
