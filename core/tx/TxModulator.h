#ifndef TXMODULATOR_H
#define TXMODULATOR_H

#include <QImage>
#include <QString>

/**
 * @brief Common real-time audio source interface for image transmitters.
 *
 * Purpose:
 * - Generate normalized mono audio samples for the TX audio engine.
 * - Expose transmission progress for the UI overlay.
 * - Keep WEFAX and SSTV modulation independent from the audio backend.
 */
class TxModulator
{
public:
    /**
     * @brief Releases transmitter resources.
     */
    virtual ~TxModulator() = default;

    /**
     * @brief Returns the sample rate required by this transmitter.
     */
    virtual int sampleRate() const = 0;

    /**
     * @brief Generates up to sampleCount normalized audio samples.
     *
     * Returns:
     * - The number of valid samples written to output.
     * - Fewer than sampleCount only when the transmission has ended.
     */
    virtual int generate(float *output, int sampleCount) = 0;

    /**
     * @brief Returns true after the whole transmission has been generated.
     */
    virtual bool isFinished() const = 0;

    /**
     * @brief Returns current image progress in the range 0.0 ... 1.0.
     */
    virtual double progress() const = 0;

    /**
     * @brief Returns the image adapted to the selected TX mode.
     */
    virtual QImage previewImage() const = 0;

    /**
     * @brief Returns a user-visible TX description for logs/status.
     */
    virtual QString description() const = 0;

    /**
     * @brief True for strictly time-slotted transmitters that must minimize backend latency.
     *
     * FT4/FT8 cannot afford generic post-audio padding or large output buffers:
     * the receiver must return to RX as soon as the slot waveform ends.
     * Other text/image modes keep the conservative default to avoid truncated symbols.
     */
    virtual bool lowLatencyTx() const
    {
        return false;
    }

    /**
     * @brief Optional explicit amount of trailing silence after generated audio.
     *
     * Negative means use the audio backend default.  FT4/FT8 returns zero.
     */
    virtual int trailingSilenceSamples() const
    {
        return -1;
    }

    /**
     * @brief Returns the current RTTY Mark/Space state for lightweight TX instruments.
     *
     * This optional hook is intentionally metadata-only.  It lets UI instruments
     * such as the crossed-ellipse RTTY scope follow the transmitter without
     * demodulating the PCM audio that has just been generated.
     *
     * Returns:
     * - true when this modulator is an RTTY source and markOut has been filled.
     * - false for non-RTTY modulators.
     */
    virtual bool rttyToneState(bool *markOut) const
    {
        if (markOut != nullptr) {
            *markOut = true;
        }
        return false;
    }
};

#endif // TXMODULATOR_H
