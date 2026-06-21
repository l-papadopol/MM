#ifndef RTTYDECODER_H
#define RTTYDECODER_H

#include "../../audio/AudioBlock.h"
#include "../../dsp/FrequencyMarker.h"

#include <QPointF>
#include <QObject>
#include <QString>
#include <QVector>
#include <functional>

/**
 * @brief Streaming AFSK RTTY decoder using ITA2/Baudot code.
 *
 * Purpose:
 * - Decode amateur-radio compatible AFSK RTTY text from live audio.
 * - Support common baud/shift/mark/space combinations.
 * - Reject noise-only input with tone presence squelch and UART framing checks.
 * - Keep the demodulator independent from the UI and audio backend.
 */
class RttyDecoder : public QObject
{
    Q_OBJECT

public:
    using MindRttyClassifier = std::function<bool(const QVector<float> &, QVector<float> *, double *)>;
    /**
     * @brief Creates a decoder with common amateur RTTY defaults.
     */
    explicit RttyDecoder(QObject *parent = nullptr);

    /**
     * @brief Returns the user-visible modem name.
     */
    static QString modeName();

    /**
     * @brief Returns current waterfall markers.
     */
    static QVector<FrequencyMarker> frequencyMarkers(double markHz,
                                                     double spaceHz,
                                                     bool reverse);

    /**
     * @brief Clears decoder state and received text.
     */
    void reset();

    /**
     * @brief Processes one block of normalized mono audio.
     */
    void processAudioBlock(const AudioBlock &block);

    /**
     * @brief Sets baud rate in symbols per second.
     */
    void setBaudRate(double baud);

    /**
     * @brief Sets mark/space tones in Hz and clears the current serial frame.
     */
    void setTones(double markHz, double spaceHz);

    /**
     * @brief Retunes mark/space tones for AFC without clearing RX text/frame state.
     */
    void retuneTones(double markHz, double spaceHz);

    /**
     * @brief Sets inverted logic/polarity.
     */
    void setReverse(bool reverse);

    /**
     * @brief Returns baud rate.
     */
    double baudRate() const;

    /**
     * @brief Returns mark tone in Hz.
     */
    double markHz() const;

    /**
     * @brief Returns space tone in Hz.
     */
    double spaceHz() const;

    /**
     * @brief Returns reverse polarity flag.
     */
    bool reverse() const;

    /**
     * @brief Returns accumulated received text.
     */
    QString receivedText() const;

    /**
     * @brief Enables MIND Active soft-slicer assist for low-confidence RTTY bit decisions.
     *
     * Training mode still collects samples but cannot alter the classical decoder.
     * Off keeps the decoder hard-bypassed from MIND.
     */
    void setMindSoftSlicerEnabled(bool enabled);

    /**
     * @brief Installs the MIND RTTY classifier callback.
     */
    void setMindSoftSlicerClassifier(MindRttyClassifier classifier);

signals:
    /**
     * @brief Emits every decoded printable/control character.
     */
    void characterReceived(const QString &text);

    /**
     * @brief Emits when the whole text buffer changes.
     */
    void textUpdated(const QString &text);

    /**
     * @brief Emits decoder diagnostic text.
     */
    void statusChanged(const QString &status);

    /**
     * @brief Emits updated waterfall markers.
     */
    void markersChanged(const QVector<FrequencyMarker> &markers);

    /**
     * @brief Emits live Mark/Space levels for the RTTY CRT tuning scope.
     */
    void tuningScopeChanged(double markLevel, double spaceLevel, double snrLike, bool locked);

    /**
     * @brief Emits RTTY bit-level samples for the dedicated MIND soft-slicer profile.
     *
     * The target is an 8-class one-hot vector: strong Mark, strong Space, weak Mark,
     * weak Space, transition, carrier drop, reverse suspicion, noise/ambiguous.
     */
    void mindRttyBitSampleReady(const QVector<float> &input, const QVector<float> &target, const QString &label);

    /**
     * @brief Emits real crossed-ellipse Mark/Space CRT deflection samples.
     *
     * The points are generated from measured Mark/Space envelopes plus live
     * oscillator phase: Mark dominant = horizontal ellipse, Space dominant =
     * vertical ellipse, mixed/incorrect tuning = diagonal smear.
     */
    void tuningScopeTraceChanged(const QVector<QPointF> &tracePoints, double snrLike, bool locked);

private:
    enum class RxState
    {
        WaitingStart,
        ValidateStart,
        DataBits,
        StopBits
    };

private:
    /**
     * @brief Rebuilds oscillator increments after sample rate/tone changes.
     */
    void updateOscillators(int sampleRate);

    /**
     * @brief Updates tone-presence squelch from demodulator energies.
     */
    void updateCarrierGate(double sumEnergy, double bitQuality);

    /**
     * @brief Clears the current serial frame without clearing received text.
     */
    void resetFrame();

    /**
     * @brief Samples the current serial stream state according to the UART clock.
     */
    void advanceSymbolClock(bool bitIsMark, double bitQuality);

    /**
     * @brief Handles one complete 5-bit ITA2 character code.
     */
    void handleCode(int code);

    /**
     * @brief Maps one ITA2 code using the current letters/figures shift.
     */
    QString decodeCode(int code) const;

    /**
     * @brief Emits status periodically.
     */
    void maybeEmitStatus();

    void updateMindFeatureWindow(double diffNorm, double bitQuality, double sumEnergy);
    QVector<float> mindRttyFeature() const;
    QVector<float> mindRttyTarget(bool bitIsMark, double bitQuality) const;
    bool maybeApplyMindSoftSlicer(bool classicBitIsMark, double bitQuality);
    void submitMindRttyBitSample(bool bitIsMark, double bitQuality);

private:
    double m_baudRate = 45.45;
    double m_markHz = 2125.0;
    double m_spaceHz = 2295.0;
    bool m_reverse = false;

    int m_sampleRate = 0;
    double m_symbolSamples = 1056.0;

    double m_markPhase = 0.0;
    double m_spacePhase = 0.0;
    double m_markInc = 0.0;
    double m_spaceInc = 0.0;
    double m_markI = 0.0;
    double m_markQ = 0.0;
    double m_spaceI = 0.0;
    double m_spaceQ = 0.0;
    double m_energyAlpha = 0.006;
    double m_confidence = 0.0;

    double m_inputPower = 0.0;
    double m_noiseFloor = 0.0;
    double m_energySnr = 1.0;
    double m_toneRatio = 0.0;
    double m_gateScore = 0.0;
    bool m_carrierOpen = false;

    RxState m_state = RxState::WaitingStart;
    bool m_previousBitMark = true;
    double m_samplesToNextDecision = 0.0;
    int m_idleMarkSamples = 0;
    int m_startSpaceSamples = 0;
    int m_markRunSamples = 0;
    int m_spaceRunSamples = 0;
    bool m_autoInvert = false;
    int m_dataBitIndex = 0;
    int m_currentCode = 0;
    bool m_lettersShift = true;

    qint64 m_samplesProcessed = 0;
    int m_decodedChars = 0;
    int m_goodFrames = 0;
    int m_badFrames = 0;
    int m_squelchedStarts = 0;
    int m_statusCounter = 0;
    int m_scopeDecimator = 0;
    QVector<QPointF> m_scopeTrace;

    QVector<float> m_mindFeatureWindow;
    int m_mindFeatureDecimator = 0;
    bool m_mindSoftSlicerEnabled = false;
    MindRttyClassifier m_mindClassifier;
    int m_mindScored = 0;
    int m_mindAssistedBits = 0;
    int m_mindSamples = 0;

    QString m_text;
};

#endif // RTTYDECODER_H
