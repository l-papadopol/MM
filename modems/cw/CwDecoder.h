#ifndef CWDECODER_H
#define CWDECODER_H

#include "../../audio/AudioBlock.h"
#include "../../dsp/FrequencyMarker.h"

#include <QObject>
#include <QHash>
#include <QString>
#include <QVector>

#include <memory>

class GGMorse;

/**
 * @brief fldigi-inspired adaptive CW / Morse receive decoder.
 *
 * MM v2.64 rewrites the CW RX front-end around the architecture used by
 * fldigi's cw modem rather than the previous short-frame Goertzel peak picker.
 *
 * Core idea:
 * - the selected CW tone is mixed to complex baseband;
 * - a narrow two-pole low-pass / matched-envelope filter extracts keyed energy;
 * - slow AGC and noise-floor tracking build an automatic hysteresis gate;
 * - stable key-up/key-down transitions feed a Morse timing state machine;
 * - symbol durations are decoded with a fldigi/SOM-style fuzzy matcher.
 *
 * Important UI behaviour:
 * - AFC disabled means hard lock to the user-clicked tone; the green marker must
 *   not wander;
 * - AFC enabled may only track within the configured local range.
 */
class CwDecoder : public QObject
{
    Q_OBJECT

public:
    explicit CwDecoder(QObject *parent = nullptr);
    ~CwDecoder() override;

    static QString modeName();
    static QVector<FrequencyMarker> frequencyMarkers(double toneHz);

    void reset();
    void processAudioBlock(const AudioBlock &block);

    void setToneHz(double toneHz);
    void setWpm(double wpm);
    void setAutoWpm(bool enabled);
    void setBandwidthHz(double bandwidthHz);
    void setAfcEnabled(bool enabled);
    void setAfcRangeHz(double rangeHz);

    double toneHz() const;
    double wpm() const;
    bool autoWpm() const;
    double trackedWpm() const;
    double bandwidthHz() const;
    QString receivedText() const;

signals:
    void characterReceived(const QString &text);
    void textUpdated(const QString &text);
    void statusChanged(const QString &status);
    void markersChanged(const QVector<FrequencyMarker> &markers);
    void speedEstimateChanged(double wpm);

private:
    void configureForSampleRate(int sampleRate);
    void rebuildDetectorConstants();
    void processSample(double sample);
    void resetGgmorse();
    void configureGgmorseForBlock(const AudioBlock &block);
    void processGgmorseBlock(const AudioBlock &block);
    void emitGgmorseOutput();
    void appendDecodedText(const QString &text);
    QString sanitizeGgmorseText(const QByteArray &bytes) const;
    void updateDetectorGate();
    void processAfcFrame();
    double goertzelMagnitudeSquared(const QVector<double> &samples, double freqHz) const;
    void updateKeyState(bool keyDown);
    void handleToneStarted();
    void handleToneEnded();
    void querySilenceDecoder();
    void finishCurrentCharacter();
    QString decodeMorse(const QString &pattern) const;
    QString decodeMorseFuzzy(const QVector<double> &durations, QString *winningPattern = nullptr,
                             double *score = nullptr) const;
    void maybeTrackSpeed(double markSeconds);
    void maybeEmitStatus();
    void resetSignalTracking();

private:
    double m_toneHz = 700.0;
    double m_trackedToneHz = 700.0;
    double m_wpm = 20.0;
    bool m_autoWpm = true;
    double m_bandwidthHz = 120.0;
    bool m_afcEnabled = false;
    double m_afcRangeHz = 20.0;

    int m_sampleRate = 0;
    qint64 m_sampleCounter = 0;

    // Baseband detector state.
    double m_phase = 0.0;
    double m_lastInput = 0.0;
    double m_dcState = 0.0;
    double m_lpfAlpha = 0.02;
    double m_envAlpha = 0.05;
    double m_i1 = 0.0;
    double m_q1 = 0.0;
    double m_i2 = 0.0;
    double m_q2 = 0.0;
    double m_env = 0.0;

    // Decimated gate / AGC state.
    int m_gateSamplesNeeded = 0;
    int m_gateSamples = 0;
    double m_gateAccumulator = 0.0;
    double m_gateValue = 0.0;
    double m_noiseLevel = 1.0e-4;
    double m_signalLevel = 8.0e-4;
    double m_openThreshold = 5.0e-4;
    double m_closeThreshold = 3.0e-4;
    double m_marginDb = 0.0;
    quint8 m_toneHistory = 0;

    // Bundled ggmorse primary decoder.  MadModem keeps its own fldigi-like
    // envelope/key detector for markers/status/AFC, but the selected-signal text
    // stream is emitted from ggmorse because its 3 s windowed speed/threshold
    // search is much more stable on hand-sent CW.
    bool m_useGgmorsePrimary = true;
    std::unique_ptr<GGMorse> m_ggmorse;
    QVector<float> m_ggmorseFifo;
    int m_ggmorseSampleRate = 0;
    double m_ggmorseConfiguredToneHz = -1.0;
    double m_ggmorseConfiguredWpm = -999.0;
    double m_ggmorseSmoothedWpm = 0.0;
    double m_ggmorseLastCost = 999.0;
    qint64 m_lastGgmorseOutputSample = -1;

    // Optional local AFC state.  This is deliberately separate from the gate:
    // it may move the marker only when AFC is enabled and only inside range.
    int m_afcFrameSamplesNeeded = 0;
    QVector<double> m_afcFrameSamples;
    QVector<double> m_searchFrequencies;
    double m_lastAfcBestHz = 700.0;
    double m_lastAfcMarginDb = 0.0;

    bool m_keyDown = false;
    bool m_haveElement = false;
    bool m_wordSpaceSent = true;
    qint64 m_keyDownStart = 0;
    qint64 m_keyUpStart = 0;

    QString m_currentPattern;
    QVector<double> m_currentDurations;
    QString m_text;

    double m_dotSeconds = 0.060; // 20 WPM: 1.2 / WPM.
    double m_trackingDotSeconds = 0.060;
    double m_lastElementSeconds = 0.0;
    double m_lastEmittedTrackedWpm = 0.0;
    QString m_lastFuzzyPattern;
    double m_lastFuzzyScore = 0.0;

    int m_decodedChars = 0;
    int m_badPatterns = 0;
    int m_statusCounter = 0;
};

#endif // CWDECODER_H
