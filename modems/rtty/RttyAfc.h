// Tone estimator shared by the fixed-shift RX AFC and regression tests.
#pragma once
#include <QtMath>
#include <algorithm>
#include "../../audio/AudioBlock.h"
namespace RttyAfc {
struct AfcTonePeak
{
    double frequencyHz = 0.0;
    double power = 0.0;
    double confidence = 0.0;
    bool valid = false;
};

/**
 * @brief Goertzel tone power normalized to roughly sample-power units.
 */
inline double goertzelPowerAt(const QVector<float> &samples, int sampleRate, double frequencyHz)
{
    const int n = samples.size();
    if (n < 64 || sampleRate <= 0 || frequencyHz <= 0.0 || frequencyHz >= sampleRate * 0.48) {
        return 0.0;
    }

    const double omega = 2.0 * M_PI * frequencyHz / static_cast<double>(sampleRate);
    const double coeff = 2.0 * qCos(omega);
    double s0 = 0.0;
    double s1 = 0.0;
    double s2 = 0.0;

    /* A very light triangular window reduces false pulls from symbol edges and
     * Hell keying clicks without adding allocations. */
    const double half = 0.5 * static_cast<double>(n - 1);
    for (int i = 0; i < n; ++i) {
        const double w = 1.0 - (0.35 * qAbs((static_cast<double>(i) - half) / qMax(1.0, half)));
        s0 = (static_cast<double>(samples.at(i)) * w) + (coeff * s1) - s2;
        s2 = s1;
        s1 = s0;
    }

    const double raw = (s1 * s1) + (s2 * s2) - (coeff * s1 * s2);
    const double norm = static_cast<double>(n) * static_cast<double>(n);
    return qMax(0.0, raw / qMax(1.0, norm));
}

inline AfcTonePeak estimateAfcTonePeak(const AudioBlock &block,
                                double centerHz,
                                int rangeHz,
                                double stepHz)
{
    AfcTonePeak result;

    if (block.samples.size() < 1024 || block.sampleRate <= 0 || rangeHz <= 0 || stepHz <= 0.0) {
        return result;
    }

    const double minHz = qMax(20.0, centerHz - static_cast<double>(rangeHz));
    const double maxHz = qMin(block.sampleRate * 0.45, centerHz + static_cast<double>(rangeHz));
    if (maxHz <= minHz) {
        return result;
    }

    QVector<double> powers;
    powers.reserve(static_cast<int>((maxHz - minHz) / stepHz) + 2);

    double bestFrequency = centerHz;
    double bestPower = 0.0;
    for (double f = minHz; f <= maxHz + 0.001; f += stepHz) {
        const double p = goertzelPowerAt(block.samples, block.sampleRate, f);
        powers.append(p);
        if (p > bestPower) {
            bestPower = p;
            bestFrequency = f;
        }
    }

    if (powers.size() < 3 || bestPower <= 0.0) {
        return result;
    }

    std::sort(powers.begin(), powers.end());
    const double medianPower = powers.at(powers.size() / 2);
    const double confidence = bestPower / qMax(1.0e-12, medianPower);

    double blockPower = 0.0;
    for (float sample : block.samples) {
        const double v = static_cast<double>(sample);
        blockPower += v * v;
    }
    blockPower /= qMax(1, static_cast<int>(block.samples.size()));

    /* Avoid chasing random noise.  Strong keyed text signals usually produce a
     * very clear local maximum inside ±10..30 Hz. */
    const bool enoughAbsoluteEnergy = bestPower > qMax(1.0e-8, blockPower * 0.010);
    const bool enoughContrast = confidence >= 1.35;

    result.frequencyHz = bestFrequency;
    result.power = bestPower;
    result.confidence = confidence;
    result.valid = enoughAbsoluteEnergy && enoughContrast;
    return result;
}

inline int nudgedToneValue(int currentHz, double measuredHz, int maxStepHz, int minHz, int maxHz)
{
    const int targetHz = static_cast<int>(qRound(measuredHz));
    const int delta = qBound(-maxStepHz, targetHz - currentHz, maxStepHz);
    return qBound(minHz, currentHz + delta, maxHz);
}


class Tracker {
public:
    void reset() { samples.clear(); elapsed=0; rate=0; offset=0; }
    int offsetHz() const { return offset; }
    bool process(const AudioBlock &block, int mark, int shift, int range) {
        if (block.sampleRate<=0 || block.samples.isEmpty()) return false;
        if (rate!=block.sampleRate || anchor!=mark || separation!=shift || limit!=range) {
            reset(); rate=block.sampleRate; anchor=mark; separation=shift; limit=range;
        }
        samples+=block.samples;
        const int size=rate/5;
        if (samples.size()>size) samples.remove(0,samples.size()-size);
        elapsed+=block.samples.size();
        if (samples.size()<size || elapsed<rate/4) return false;
        elapsed=0;
        AudioBlock window=block; window.samples=samples;
        const auto a=estimateAfcTonePeak(window,mark,range,1.0);
        const auto b=estimateAfcTonePeak(window,mark+shift,range,1.0);
        if (!a.valid || !b.valid || qAbs((a.frequencyHz-mark)-(b.frequencyHz-mark-shift))>6.0) return false;
        const int target=qBound(-range,qRound(((a.frequencyHz-mark)+(b.frequencyHz-mark-shift))/2.0),range);
        if(qAbs(target-offset)<2) return false;
        offset+=qBound(-3,target-offset,3);
        return true;
    }
private:
    QVector<float> samples;
    int elapsed=0,rate=0,offset=0,anchor=0,separation=0,limit=0;
};
} // namespace RttyAfc
