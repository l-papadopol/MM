#include "DspConditioner.h"

#include <QtMath>

#include <utility>

namespace {

constexpr double kTwoPi = 2.0 * M_PI;

/**
 * @brief Returns a frequency clamped below Nyquist with a small safety margin.
 */
double safeFrequency(double sampleRate, double frequencyHz)
{
    return qBound(5.0, frequencyHz, sampleRate * 0.45);
}

} // namespace

// -----------------------------------------------------------------------------
// Biquad
// -----------------------------------------------------------------------------

void DspConditioner::Biquad::reset()
{
    m_b0 = 1.0;
    m_b1 = 0.0;
    m_b2 = 0.0;
    m_a1 = 0.0;
    m_a2 = 0.0;
    m_z1 = 0.0;
    m_z2 = 0.0;
}

void DspConditioner::Biquad::setHighPass(double sampleRate, double frequencyHz, double q)
{
    if (sampleRate <= 0.0 || frequencyHz <= 0.0) {
        reset();
        return;
    }

    const double omega = kTwoPi * safeFrequency(sampleRate, frequencyHz) / sampleRate;
    const double cosOmega = qCos(omega);
    const double sinOmega = qSin(omega);
    const double alpha = sinOmega / (2.0 * qMax(0.05, q));

    const double b0 = (1.0 + cosOmega) * 0.5;
    const double b1 = -(1.0 + cosOmega);
    const double b2 = (1.0 + cosOmega) * 0.5;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cosOmega;
    const double a2 = 1.0 - alpha;

    setNormalized(b0, b1, b2, a0, a1, a2);
}

void DspConditioner::Biquad::setLowPass(double sampleRate, double frequencyHz, double q)
{
    if (sampleRate <= 0.0 || frequencyHz <= 0.0) {
        reset();
        return;
    }

    const double omega = kTwoPi * safeFrequency(sampleRate, frequencyHz) / sampleRate;
    const double cosOmega = qCos(omega);
    const double sinOmega = qSin(omega);
    const double alpha = sinOmega / (2.0 * qMax(0.05, q));

    const double b0 = (1.0 - cosOmega) * 0.5;
    const double b1 = 1.0 - cosOmega;
    const double b2 = (1.0 - cosOmega) * 0.5;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cosOmega;
    const double a2 = 1.0 - alpha;

    setNormalized(b0, b1, b2, a0, a1, a2);
}

void DspConditioner::Biquad::setNotch(double sampleRate, double frequencyHz, double q)
{
    if (sampleRate <= 0.0 || frequencyHz <= 0.0 || frequencyHz >= sampleRate * 0.45) {
        reset();
        return;
    }

    const double omega = kTwoPi * frequencyHz / sampleRate;
    const double cosOmega = qCos(omega);
    const double sinOmega = qSin(omega);
    const double alpha = sinOmega / (2.0 * qMax(0.05, q));

    const double b0 = 1.0;
    const double b1 = -2.0 * cosOmega;
    const double b2 = 1.0;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cosOmega;
    const double a2 = 1.0 - alpha;

    setNormalized(b0, b1, b2, a0, a1, a2);
}


void DspConditioner::Biquad::setBandPass(double sampleRate, double frequencyHz, double q)
{
    if (sampleRate <= 0.0 || frequencyHz <= 0.0 || frequencyHz >= sampleRate * 0.45) {
        reset();
        return;
    }

    const double omega = kTwoPi * safeFrequency(sampleRate, frequencyHz) / sampleRate;
    const double cosOmega = qCos(omega);
    const double sinOmega = qSin(omega);
    const double alpha = sinOmega / (2.0 * qMax(0.05, q));

    const double b0 = alpha;
    const double b1 = 0.0;
    const double b2 = -alpha;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cosOmega;
    const double a2 = 1.0 - alpha;

    setNormalized(b0, b1, b2, a0, a1, a2);
}

void DspConditioner::Biquad::setNormalized(double b0, double b1, double b2,
                                           double a0, double a1, double a2)
{
    if (qAbs(a0) < 1.0e-18) {
        reset();
        return;
    }

    m_b0 = b0 / a0;
    m_b1 = b1 / a0;
    m_b2 = b2 / a0;
    m_a1 = a1 / a0;
    m_a2 = a2 / a0;

    m_z1 = 0.0;
    m_z2 = 0.0;
}

double DspConditioner::Biquad::process(double input)
{
    const double output = (m_b0 * input) + m_z1;
    m_z1 = (m_b1 * input) - (m_a1 * output) + m_z2;
    m_z2 = (m_b2 * input) - (m_a2 * output);
    return output;
}


// -----------------------------------------------------------------------------
// Adaptive line enhancer
// -----------------------------------------------------------------------------

void DspConditioner::AdaptiveLineEnhancer::reset()
{
    for (int i = 0; i < kMaxTaps; ++i) {
        m_weights[i] = 0.0;
    }
    for (int i = 0; i < kMaxDelay + kMaxTaps; ++i) {
        m_delayLine[i] = 0.0;
    }
    m_index = 0;
}

void DspConditioner::AdaptiveLineEnhancer::configure(int delaySamples, double stepSize, double wetMix)
{
    m_delaySamples = qBound(2, delaySamples, kMaxDelay);
    m_stepSize = qBound(0.00002, stepSize, 0.004);
    m_wetMix = qBound(0.0, wetMix, 1.0);
}

double DspConditioner::AdaptiveLineEnhancer::process(double sample)
{
    m_delayLine[m_index] = sample;

    double estimate = 0.0;
    double norm = 1.0e-9;
    for (int tap = 0; tap < kMaxTaps; ++tap) {
        int read = m_index - m_delaySamples - tap;
        while (read < 0) {
            read += kMaxDelay + kMaxTaps;
        }
        const double delayed = m_delayLine[read % (kMaxDelay + kMaxTaps)];
        estimate += m_weights[tap] * delayed;
        norm += delayed * delayed;
    }

    const double error = sample - estimate;
    const double mu = m_stepSize / norm;
    for (int tap = 0; tap < kMaxTaps; ++tap) {
        int read = m_index - m_delaySamples - tap;
        while (read < 0) {
            read += kMaxDelay + kMaxTaps;
        }
        const double delayed = m_delayLine[read % (kMaxDelay + kMaxTaps)];
        m_weights[tap] = qBound(-2.0, m_weights[tap] + (mu * error * delayed), 2.0);
    }

    m_index = (m_index + 1) % (kMaxDelay + kMaxTaps);

    // The LMS estimate is the correlated/tonal component.  Blend it with the
    // input so weak keyed edges and Baudot transitions are not over-smoothed.
    return (m_wetMix * estimate) + ((1.0 - m_wetMix) * sample);
}

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

DspConditioner::DspConditioner()
{
    reset();
}

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------

void DspConditioner::setConfig(const Config &config)
{
    const bool filterShapeChanged =
        m_config.profile != config.profile ||
        m_config.modeBandpassEnabled != config.modeBandpassEnabled ||
        qAbs(m_config.blackHz - config.blackHz) > 0.5 ||
        qAbs(m_config.whiteHz - config.whiteHz) > 0.5;

    m_config = config;

    if (filterShapeChanged && m_sampleRate > 0) {
        updateFilters(m_sampleRate);
    }
}

void DspConditioner::reset()
{
    m_dcPrevInput = 0.0;
    m_dcPrevOutput = 0.0;

    m_notch50.reset();
    m_notch100.reset();
    m_notch150.reset();
    m_hp1.reset();
    m_hp2.reset();
    m_lp1.reset();
    m_lp2.reset();
    m_rttyMarkBp1.reset();
    m_rttyMarkBp2.reset();
    m_rttySpaceBp1.reset();
    m_rttySpaceBp2.reset();
    m_adaptiveLineEnhancer.reset();

    m_blankerEnvelope = 0.02;
    m_lastGoodSample = 0.0;
    m_rmsEstimate = 0.03;
    m_noiseFloorEstimate = 0.012;
    m_agcGain = 1.0;

    if (m_sampleRate > 0) {
        updateFilters(m_sampleRate);
    }
}

// -----------------------------------------------------------------------------
// Block processing
// -----------------------------------------------------------------------------

AudioBlock DspConditioner::processBlock(const AudioBlock &block)
{
    if (!m_config.enabled || block.samples.isEmpty() || block.sampleRate <= 0) {
        return block;
    }

    if (block.sampleRate != m_sampleRate) {
        m_sampleRate = block.sampleRate;
        reset();
        updateFilters(m_sampleRate);
    }

    AudioBlock output;
    output.sampleRate = block.sampleRate;
    output.firstSampleIndex = block.firstSampleIndex;
    output.samples.resize(block.samples.size());

    QVector<double> stage;
    stage.resize(block.samples.size());

    double squareSum = 0.0;

    for (int i = 0; i < block.samples.size(); ++i) {
        const double input = qBound(-1.5, static_cast<double>(block.samples[i]), 1.5);

        const double dcBlocked = input - m_dcPrevInput + (0.995 * m_dcPrevOutput);
        m_dcPrevInput = input;
        m_dcPrevOutput = dcBlocked;

        double sample = dcBlocked;

        if (m_config.humNotchEnabled) {
            sample = m_notch50.process(sample);
            sample = m_notch100.process(sample);
            sample = m_notch150.process(sample);
        }

        if (m_config.impulseBlankerEnabled) {
            sample = processImpulseBlanker(sample);
        }

        if (m_config.modeBandpassEnabled) {
            sample = m_hp1.process(sample);
            sample = m_hp2.process(sample);
            sample = m_lp1.process(sample);
            sample = m_lp2.process(sample);
        }

        if (m_config.profile == Profile::Rtty &&
            (m_config.rttyMatchedFilterEnabled || m_config.rttyMarkSpaceEnhancerEnabled)) {
            double mark = m_rttyMarkBp1.process(sample);
            mark = m_rttyMarkBp2.process(mark);
            double space = m_rttySpaceBp1.process(sample);
            space = m_rttySpaceBp2.process(space);
            const double combined = mark + space;
            sample = m_config.rttyMarkSpaceEnhancerEnabled
                         ? (0.85 * combined) + (0.15 * sample)
                         : (0.65 * combined) + (0.35 * sample);
        }

        if (m_config.adaptiveLineEnhancerEnabled) {
            sample = m_adaptiveLineEnhancer.process(sample);
        }

        stage[i] = sample;
        squareSum += sample * sample;
    }

    if (m_config.imageWaveletDenoiseEnabled &&
        (m_config.profile == Profile::WeatherFax || m_config.profile == Profile::Sstv)) {
        processImageWaveletDenoise(&stage);
        squareSum = 0.0;
        for (double v : std::as_const(stage)) {
            squareSum += v * v;
        }
    }

    const double blockRms = qSqrt(squareSum / static_cast<double>(qMax(1, stage.size())));
    m_rmsEstimate = (0.92 * m_rmsEstimate) + (0.08 * blockRms);

    if (blockRms < m_noiseFloorEstimate * 1.8) {
        m_noiseFloorEstimate = (0.98 * m_noiseFloorEstimate) + (0.02 * blockRms);
    } else {
        m_noiseFloorEstimate = qMin(m_noiseFloorEstimate * 1.002,
                                    qMax(0.004, blockRms * 0.45));
    }

    const bool rttyProfile = (m_config.profile == Profile::Rtty);
    const double targetRms = rttyProfile ? 0.16 : 0.22;
    const double minGain = rttyProfile ? 0.70 : 0.35;
    const double maxGain = rttyProfile ? 2.20 : 8.00;
    const double agcAlpha = rttyProfile ? 0.004 : 0.020;
    const double targetGain = qBound(minGain, targetRms / qMax(0.002, m_rmsEstimate), maxGain);
    // RTTY Baudot slicers are sensitive to Mark/Space amplitude balance.  When
    // the optional software AGC is enabled for RTTY, keep it deliberately gentle
    // and slow; the default remains OFF because wideband AGC can make contest
    // RTTY worse during selective fading.
    m_agcGain = ((1.0 - agcAlpha) * m_agcGain) + (agcAlpha * targetGain);

    for (int i = 0; i < stage.size(); ++i) {
        double sample = stage[i];

        if (m_config.noiseReductionEnabled) {
            sample = processNoiseReducer(sample);
        }

        if (m_config.agcEnabled) {
            sample *= m_agcGain;
        }

        output.samples[i] = static_cast<float>(qBound(-1.0, sample, 1.0));
    }

    return output;
}

// -----------------------------------------------------------------------------
// Filter setup
// -----------------------------------------------------------------------------

void DspConditioner::updateFilters(int sampleRate)
{
    if (sampleRate <= 0) {
        return;
    }

    m_notch50.setNotch(static_cast<double>(sampleRate), 50.0, 35.0);
    m_notch100.setNotch(static_cast<double>(sampleRate), 100.0, 35.0);
    m_notch150.setNotch(static_cast<double>(sampleRate), 150.0, 35.0);

    const double lowHz = passBandLowHz(sampleRate);
    const double highHz = passBandHighHz(sampleRate);

    m_hp1.setHighPass(static_cast<double>(sampleRate), lowHz, 0.707);
    m_hp2.setHighPass(static_cast<double>(sampleRate), lowHz, 0.707);
    m_lp1.setLowPass(static_cast<double>(sampleRate), highHz, 0.707);
    m_lp2.setLowPass(static_cast<double>(sampleRate), highHz, 0.707);

    const double markHz = safeFrequency(static_cast<double>(sampleRate), m_config.blackHz);
    const double spaceHz = safeFrequency(static_cast<double>(sampleRate), m_config.whiteHz);
    const double rttyQ = m_config.rttyMatchedFilterEnabled ? 22.0 : 14.0;
    m_rttyMarkBp1.setBandPass(static_cast<double>(sampleRate), markHz, rttyQ);
    m_rttyMarkBp2.setBandPass(static_cast<double>(sampleRate), markHz, rttyQ);
    m_rttySpaceBp1.setBandPass(static_cast<double>(sampleRate), spaceHz, rttyQ);
    m_rttySpaceBp2.setBandPass(static_cast<double>(sampleRate), spaceHz, rttyQ);

    const int aleDelay = qBound(4, sampleRate / 3000, 96);
    const double aleMu = (m_config.profile == Profile::Cw) ? 0.00075 : 0.00045;
    const double aleWet = (m_config.profile == Profile::Cw) ? 0.72 : 0.52;
    m_adaptiveLineEnhancer.configure(aleDelay, aleMu, aleWet);
}

double DspConditioner::passBandLowHz(int sampleRate) const
{
    Q_UNUSED(sampleRate)

    switch (m_config.profile) {
    case Profile::WeatherFax: {
        const double lowTone = qMin(m_config.blackHz, m_config.whiteHz);
        return qMax(120.0, lowTone - 320.0);
    }
    case Profile::Sstv:
        return 1050.0;
    case Profile::Rtty:
        /* RTTY V2 performs its own narrow Mark/Space channelization and AFC.
         * Keep the shared conditioner intentionally wide, otherwise an
         * off-center signal is filtered out before the decoder can acquire it. */
        return 300.0;
    case Profile::Bpsk31:
        return qMax(120.0, m_config.blackHz - (m_config.bpskCoherentTrackingEnabled ? 160.0 : 220.0));
    case Profile::Mfsk:
        return qMax(80.0, qMin(m_config.blackHz, m_config.whiteHz) - 140.0);
    case Profile::FtWeakSignal:
        /* MSHV-style weak-signal front-end: keep the whole FT audio passband,
         * not only the green RX marker.  This prevents a selected QSO
         * frequency from becoming a narrow pre-filter that hides other FT
         * candidates and creates confusing apparent images in the display. */
        return 65.0;
    case Profile::DisplayWide:
        /* The waterfall should display the same sanitized radio-audio band that
         * weak-signal programs such as MSHV feed to their FFT display: remove
         * sub-audio rumble and very high audio before analysis, but do not gate
         * or AGC it. */
        return 65.0;
    case Profile::Cw:
        return qMax(80.0, m_config.blackHz - 300.0);
    case Profile::Hell:
        return qMax(80.0, m_config.blackHz - 420.0);
    case Profile::General:
    default:
        return 100.0;
    }
}

double DspConditioner::passBandHighHz(int sampleRate) const
{
    const double nyquistSafe = static_cast<double>(sampleRate) * 0.45;

    switch (m_config.profile) {
    case Profile::WeatherFax: {
        const double highTone = qMax(m_config.blackHz, m_config.whiteHz);
        return qMin(nyquistSafe, highTone + 320.0);
    }
    case Profile::Sstv:
        return qMin(nyquistSafe, 2500.0);
    case Profile::Rtty:
        /* RTTY V2 performs its own narrow Mark/Space channelization and AFC.
         * Keep the shared conditioner intentionally wide, otherwise an
         * off-center signal is filtered out before the decoder can acquire it. */
        return qMin(nyquistSafe, 3000.0);
    case Profile::Bpsk31:
        return qMin(nyquistSafe, m_config.blackHz + (m_config.bpskCoherentTrackingEnabled ? 160.0 : 220.0));
    case Profile::Mfsk:
        return qMin(nyquistSafe, qMax(m_config.blackHz, m_config.whiteHz) + 140.0);
    case Profile::FtWeakSignal:
        return qMin(nyquistSafe, 3300.0);
    case Profile::DisplayWide:
        return qMin(nyquistSafe, 3400.0);
    case Profile::Cw:
        return qMin(nyquistSafe, m_config.blackHz + 300.0);
    case Profile::Hell:
        return qMin(nyquistSafe, m_config.blackHz + 420.0);
    case Profile::General:
    default:
        return qMin(nyquistSafe, 6000.0);
    }
}

// -----------------------------------------------------------------------------
// DSP stages
// -----------------------------------------------------------------------------

double DspConditioner::processImpulseBlanker(double sample)
{
    const double absSample = qAbs(sample);
    m_blankerEnvelope = (0.999 * m_blankerEnvelope) + (0.001 * absSample);

    const double threshold = qMax(0.20, m_blankerEnvelope * 10.0);

    if (absSample > threshold) {
        return m_lastGoodSample * 0.96;
    }

    m_lastGoodSample = sample;
    return sample;
}

double DspConditioner::processNoiseReducer(double sample) const
{
    const double absSample = qAbs(sample);
    const double floor = qMax(0.001, m_noiseFloorEstimate * 0.85);

    /*
     * Soft Wiener-like scalar mask.  It is intentionally gentle: weak gray
     * detail is attenuated less than 6 dB instead of being gated away.
     */
    const double snr = (absSample * absSample) / ((floor * floor) + 1.0e-12);
    const double gain = qBound(0.50, snr / (snr + 1.0), 1.0);

    return sample * gain;
}


void DspConditioner::processImageWaveletDenoise(QVector<double> *samples) const
{
    if (samples == nullptr || samples->size() < 4) {
        return;
    }

    QVector<double> &x = *samples;
    QVector<double> out = x;

    // Single-level Haar soft threshold on adjacent high-frequency detail.
    // This is intentionally gentle: it reduces grain/noise in SSTV/WEFAX audio
    // without blurring line/colour transitions as much as a static low-pass.
    double detailAbsMean = 0.0;
    int pairs = 0;
    for (int i = 0; i + 1 < x.size(); i += 2) {
        const double detail = (x[i] - x[i + 1]) * 0.5;
        detailAbsMean += qAbs(detail);
        ++pairs;
    }
    const double threshold = qMax(0.0004, (detailAbsMean / qMax(1, pairs)) * 0.85);

    for (int i = 0; i + 1 < x.size(); i += 2) {
        const double average = (x[i] + x[i + 1]) * 0.5;
        double detail = (x[i] - x[i + 1]) * 0.5;
        const double sign = detail < 0.0 ? -1.0 : 1.0;
        detail = sign * qMax(0.0, qAbs(detail) - threshold);
        out[i] = average + detail;
        out[i + 1] = average - detail;
    }

    x.swap(out);
}
