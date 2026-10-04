#ifndef FREQUENCYTRACKER_H
#define FREQUENCYTRACKER_H

#include <QVector>

/**
 * @brief Result of a tone frequency estimation.
 *
 * Purpose:
 * - Carry estimated frequency.
 * - Carry signal RMS.
 * - Carry confidence score.
 * - Mark invalid estimates explicitly.
 */
struct FrequencyEstimate
{
    double frequencyHz = 0.0;
    double rms = 0.0;
    double confidence = 0.0;
    bool valid = false;
};

/**
 * @brief Estimates audio tone frequency inside a bounded frequency range.
 *
 * Purpose:
 * - Provide a reusable frequency tracker for image/audio modems.
 * - Support WeatherFax and future SSTV decoding.
 * - Avoid putting tone estimation directly inside modem classes.
 */
class FrequencyTracker
{
public:
    /**
     * @brief Estimates tone frequency using normalized autocorrelation.
     *
     * Behavior:
     * - Removes DC offset before analysis.
     * - Searches periods corresponding to minHz...maxHz.
     * - Uses parabolic interpolation around the best lag.
     * - Rejects weak or low-confidence estimates.
     */
    static FrequencyEstimate estimateAutocorrelation(
        const QVector<float> &samples,
        int sampleRate,
        double minHz,
        double maxHz,
        double minRms
        );

private:
    /**
     * @brief Computes normalized autocorrelation score for one lag.
     */
    static double correlationAtLag(
        const QVector<double> &samples,
        int lag
        );
};

#endif // FREQUENCYTRACKER_H
