#include "FrequencyTracker.h"

#include <QtMath>

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

FrequencyEstimate FrequencyTracker::estimateAutocorrelation(
    const QVector<float> &samples,
    int sampleRate,
    double minHz,
    double maxHz,
    double minRms
    )
{
    FrequencyEstimate result;

    if (samples.size() < 16 || sampleRate <= 0 || minHz <= 0.0 || maxHz <= minHz) {
        return result;
    }

    double mean = 0.0;

    for (float sample : samples) {
        mean += static_cast<double>(sample);
    }

    mean /= static_cast<double>(samples.size());

    QVector<double> centered;
    centered.resize(samples.size());

    double sumSquares = 0.0;

    for (int i = 0; i < samples.size(); ++i) {
        const double value = static_cast<double>(samples[i]) - mean;
        centered[i] = value;
        sumSquares += value * value;
    }

    result.rms = qSqrt(sumSquares / static_cast<double>(samples.size()));

    if (result.rms < minRms) {
        return result;
    }

    const int minLag = qMax(1, static_cast<int>(qFloor(static_cast<double>(sampleRate) / maxHz)));
    const int maxLag = qMin(
        samples.size() - 2,
        static_cast<int>(qCeil(static_cast<double>(sampleRate) / minHz))
        );

    if (maxLag <= minLag) {
        return result;
    }

    int bestLag = minLag;
    double bestScore = -1.0;

    for (int lag = minLag; lag <= maxLag; ++lag) {
        const double score = correlationAtLag(centered, lag);

        if (score > bestScore) {
            bestScore = score;
            bestLag = lag;
        }
    }

    if (bestScore < 0.25) {
        result.confidence = bestScore;
        return result;
    }

    const double leftScore = correlationAtLag(centered, qMax(minLag, bestLag - 1));
    const double centerScore = bestScore;
    const double rightScore = correlationAtLag(centered, qMin(maxLag, bestLag + 1));

    double lagOffset = 0.0;
    const double denominator = leftScore - (2.0 * centerScore) + rightScore;

    if (qAbs(denominator) > 1.0e-12) {
        lagOffset = 0.5 * (leftScore - rightScore) / denominator;
        lagOffset = qBound(-0.5, lagOffset, 0.5);
    }

    const double interpolatedLag = static_cast<double>(bestLag) + lagOffset;

    if (interpolatedLag <= 0.0) {
        result.confidence = bestScore;
        return result;
    }

    const double estimatedHz = static_cast<double>(sampleRate) / interpolatedLag;

    if (estimatedHz < minHz || estimatedHz > maxHz) {
        result.confidence = bestScore;
        return result;
    }

    result.frequencyHz = estimatedHz;
    result.confidence = bestScore;
    result.valid = true;

    return result;
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

double FrequencyTracker::correlationAtLag(
    const QVector<double> &samples,
    int lag
    )
{
    if (lag <= 0 || lag >= samples.size()) {
        return 0.0;
    }

    double correlation = 0.0;
    double energyA = 0.0;
    double energyB = 0.0;

    const int count = samples.size() - lag;

    for (int i = 0; i < count; ++i) {
        const double a = samples[i];
        const double b = samples[i + lag];

        correlation += a * b;
        energyA += a * a;
        energyB += b * b;
    }

    const double denominator = qSqrt(energyA * energyB);

    if (denominator <= 1.0e-18) {
        return 0.0;
    }

    return correlation / denominator;
}
