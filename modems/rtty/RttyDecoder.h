#ifndef RTTYDECODER_H
#define RTTYDECODER_H

#include "../../audio/AudioBlock.h"
#include "../../dsp/FrequencyMarker.h"

#include <QPointF>
#include <QObject>
#include <QString>
#include <QVector>

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
     * @brief Enables decoder-driven polarity recommendation.
     *
     * When enabled, the decoder may ask the UI to flip Reverse polarity
     * after detecting a stable carrier whose UART start/stop framing is
     * consistently inverted.  The final switch is still performed by the UI
     * so the user-visible Reverse checkbox remains the single manual state.
     */
    void setAutoReverseEnabled(bool enabled);

    /**
     * @brief Supplies the current CAT demodulation mode as an automatic-polarity prior.
     *
     * Direct Hamlib CAT polling reports modes such as LSB, USB, RTTY or RTTYR.
     * The hint never overrides manual polarity: it is used only when Auto polarity
     * is enabled, and the live dual-hypothesis UART probe may override it when
     * the received framing/text strongly favours the opposite polarity.
     */
    void setCatModeHint(const QString &modeName);

    /**
     * @brief Returns whether automatic reverse-polarity requests are enabled.
     */
    bool autoReverseEnabled() const;

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
     * @brief Emits real crossed-ellipse Mark/Space CRT deflection samples.
     *
     * The points are generated from measured Mark/Space envelopes plus live
     * oscillator phase: Mark dominant = horizontal ellipse, Space dominant =
     * vertical ellipse, mixed/incorrect tuning = diagonal smear.
     */
    void tuningScopeTraceChanged(const QVector<QPointF> &tracePoints, double snrLike, bool locked);

    /**
     * @brief Requests that the UI flips the persistent Reverse polarity flag.
     */
    void reversePolarityRequested(bool reverse);

    /**
     * @brief Reports the current automatic polarity decision for the tuning scope.
     */
    void polarityDecisionChanged(bool reverse,
                                 const QString &source,
                                 const QString &catMode,
                                 double normalScore,
                                 double reverseScore);

private:
    enum class RxState
    {
        WaitingStart,
        ValidateStart,
        DataBits,
        StopBits
    };

    enum class ProbeRxState
    {
        WaitingStart,
        ValidateStart,
        DataBits,
        StopBits
    };

    struct PolarityProbe
    {
        ProbeRxState state = ProbeRxState::WaitingStart;
        double samplesToNextDecision = 0.0;
        int idleMarkSamples = 0;
        int startSpaceSamples = 0;
        int dataBitIndex = 0;
        int currentCode = 0;
        bool lettersShift = true;
        int goodFrames = 0;
        int badFrames = 0;
        int plausibleChars = 0;
        int weakChars = 0;
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

    void resetPolarityProbe(PolarityProbe &probe, bool keepStatistics = false);
    void resetPolarityProbes();
    void advancePolarityProbe(PolarityProbe &probe,
                              bool bitIsMark,
                              double bitQuality);
    double polarityProbeScore(const PolarityProbe &probe) const;
    void evaluateAutomaticPolarity();
    int catPreferredReverse(const QString &modeName) const;

private:
    double m_baudRate = 45.45;
    double m_markHz = 2125.0;
    double m_spaceHz = 2295.0;
    bool m_reverse = false;
    bool m_autoReverseEnabled = true;
    QString m_catModeHint;
    int m_catReversePreference = -1; // -1 unknown, 0 normal, 1 reverse
    QString m_polarityDecisionSource = QStringLiteral("signal");
    PolarityProbe m_normalProbe;
    PolarityProbe m_reverseProbe;
    int m_polarityEvaluationCooldown = 0;

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
    double m_samplesToNextDecision = 0.0;
    int m_idleMarkSamples = 0;
    int m_startSpaceSamples = 0;
    bool m_autoReverseRequestPending = false;
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

    QString m_text;
};

#endif // RTTYDECODER_H
