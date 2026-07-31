#ifndef DSPCONDITIONER_H
#define DSPCONDITIONER_H

#include "../../audio/AudioBlock.h"

#include <QVector>

/**
 * @brief Shared audio conditioning pipeline used before modem-specific decoders.
 *
 * Purpose:
 * - Keep waterfall analysis independent from decoder conditioning.
 * - Apply safe, reusable radio-audio DSP before WeatherFax/SSTV decoders.
 * - Provide mode-specific pass-bands without duplicating common filters.
 * - Keep the original sample rate and full timing resolution for image modes.
 *
 * Processing order:
 * - DC blocker.
 * - Optional mains-hum notches.
 * - Optional impulse blanker for short QRN/click spikes.
 * - Optional mode pass-band.
 * - Optional soft noise reducer.
 * - Optional slow AGC/normalizer.
 */
class DspConditioner
{
public:
    /**
     * @brief Decoder profile used to select the useful audio pass-band.
     */
    enum class Profile
    {
        General,
        WeatherFax,
        Sstv,
        Rtty,
        Bpsk31,
        Mfsk,
        FtWeakSignal,
        DisplayWide,
        Cw,
        Hell
    };

    /**
     * @brief Runtime configuration for the conditioning chain.
     */
    struct Config
    {
        Profile profile = Profile::General;

        bool enabled = true;
        bool humNotchEnabled = true;
        bool impulseBlankerEnabled = true;
        bool modeBandpassEnabled = true;
        bool noiseReductionEnabled = true;
        bool agcEnabled = true;

        // Optional advanced, mode-selective DSP modules.  They are disabled
        // by default and intentionally not used by FT4/FT8.
        bool adaptiveLineEnhancerEnabled = false;
        bool rttyMatchedFilterEnabled = false;
        bool rttyMarkSpaceEnhancerEnabled = false;
        bool bpskCoherentTrackingEnabled = false;
        bool imageWaveletDenoiseEnabled = false;

        double blackHz = 1500.0;
        double whiteHz = 2300.0;
    };

public:
    /**
     * @brief Creates a conditioner with safe default settings.
     */
    DspConditioner();

    /**
     * @brief Applies a new configuration and refreshes filters if required.
     */
    void setConfig(const Config &config);

    /**
     * @brief Clears all filter memories and estimators.
     */
    void reset();

    /**
     * @brief Processes one audio block and returns a conditioned block.
     */
    AudioBlock processBlock(const AudioBlock &block);

private:
    /**
     * @brief Second-order IIR biquad used by the shared conditioning chain.
     */
    class Biquad
    {
    public:
        /**
         * @brief Clears filter state and sets bypass coefficients.
         */
        void reset();

        /**
         * @brief Configures a high-pass section.
         */
        void setHighPass(double sampleRate, double frequencyHz, double q);

        /**
         * @brief Configures a low-pass section.
         */
        void setLowPass(double sampleRate, double frequencyHz, double q);

        /**
         * @brief Configures a notch section.
         */
        void setNotch(double sampleRate, double frequencyHz, double q);

        /**
         * @brief Configures a narrow band-pass section.
         */
        void setBandPass(double sampleRate, double frequencyHz, double q);

        /**
         * @brief Processes one sample.
         */
        double process(double input);

    private:
        /**
         * @brief Normalizes RBJ-style biquad coefficients.
         */
        void setNormalized(double b0, double b1, double b2,
                           double a0, double a1, double a2);

    private:
        double m_b0 = 1.0;
        double m_b1 = 0.0;
        double m_b2 = 0.0;
        double m_a1 = 0.0;
        double m_a2 = 0.0;
        double m_z1 = 0.0;
        double m_z2 = 0.0;
    };

private:
    /**
     * @brief Lightweight LMS adaptive line enhancer for tonal CW/RTTY signals.
     */
    class AdaptiveLineEnhancer
    {
    public:
        void reset();
        void configure(int delaySamples, double stepSize, double wetMix);
        double process(double sample);

    private:
        static constexpr int kMaxTaps = 16;
        static constexpr int kMaxDelay = 96;
        double m_weights[kMaxTaps] = {0.0};
        double m_delayLine[kMaxDelay + kMaxTaps] = {0.0};
        int m_delaySamples = 12;
        double m_stepSize = 0.00045;
        double m_wetMix = 0.55;
        int m_index = 0;
    };

    /**
     * @brief Rebuilds filters for the current sample rate and profile.
     */
    void updateFilters(int sampleRate);

    /**
     * @brief Returns the pass-band low edge for the selected profile.
     */
    double passBandLowHz(int sampleRate) const;

    /**
     * @brief Returns the pass-band high edge for the selected profile.
     */
    double passBandHighHz(int sampleRate) const;

    /**
     * @brief Processes the impulse blanker stage.
     */
    double processImpulseBlanker(double sample);

    /**
     * @brief Processes the soft noise reducer stage.
     */
    double processNoiseReducer(double sample) const;

    /**
     * @brief Applies a block Haar soft-threshold denoiser for image modes.
     */
    void processImageWaveletDenoise(QVector<double> *samples) const;

private:
    Config m_config;

    int m_sampleRate = 0;

    double m_dcPrevInput = 0.0;
    double m_dcPrevOutput = 0.0;

    Biquad m_notch50;
    Biquad m_notch100;
    Biquad m_notch150;
    Biquad m_hp1;
    Biquad m_hp2;
    Biquad m_lp1;
    Biquad m_lp2;
    Biquad m_rttyMarkBp1;
    Biquad m_rttyMarkBp2;
    Biquad m_rttySpaceBp1;
    Biquad m_rttySpaceBp2;
    AdaptiveLineEnhancer m_adaptiveLineEnhancer;

    double m_blankerEnvelope = 0.02;
    double m_lastGoodSample = 0.0;

    double m_rmsEstimate = 0.03;
    double m_noiseFloorEstimate = 0.012;
    double m_agcGain = 1.0;
};

#endif // DSPCONDITIONER_H
