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
        bool continuousTracking = false;
        bool stochastic = false;
    };

    struct SearchLimits
    {
        double centerAzimuthDeg = 0.0;
        double centerElevationDeg = 0.0;
        double azimuthSpanDeg = 30.0;
        double elevationSpanDeg = 0.0;
        AxisMode axisMode = AxisMode::AzimuthOnly;
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
        double nextAzimuthDeg = 0.0;
        double nextElevationDeg = 0.0;
        double bestAzimuthDeg = 0.0;
        double bestElevationDeg = 0.0;
        double bestMetricDb = -999.0;
        QString status;
    };

    static QVector<AlgorithmInfo> algorithms();
    static AlgorithmInfo algorithmById(const QString &id);
    static bool isCompatible(const QString &id, AxisMode axisMode);
    static QString defaultAlgorithm(AxisMode axisMode);
    static Recommendation recommendNextStep(const QString &algorithmId,
                                            const SearchLimits &limits,
                                            const QVector<Sample> &samples,
                                            double currentAzimuthDeg,
                                            double currentElevationDeg);

private:
    RotatorPeakSearch() = delete;
};

} // namespace mm

#endif // ROTATORPEAKSEARCH_H
