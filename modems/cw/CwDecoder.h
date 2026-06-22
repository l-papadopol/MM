#ifndef CWDECODER_H
#define CWDECODER_H

#include "../../audio/AudioBlock.h"
#include "../../dsp/FrequencyMarker.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <memory>

class GGMorse;

/**
 * @brief CW / Morse receive decoder backed by the bundled ggmorse engine.
 *
 * This class is intentionally a thin Qt wrapper around ggmorse. It owns tone,
 * bandwidth, AFC, bounded input leveling and dual-RX marker integration.
 * There is no second CW text decoder and no MIND path in CW.
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
    void resetGgmorse();
    void configureGgmorseForBlock(const AudioBlock &block);
    void processGgmorseBlock(const AudioBlock &block);
    void emitGgmorseOutput();
    void appendDecodedText(const QString &text);
    QString sanitizeGgmorseText(const QByteArray &bytes) const;
    float leveledSample(float sample) const;
    void updateBlockLevel(const AudioBlock &block);
    void maybeUpdateAfcFromGgmorse();
    void maybeEmitStatus();

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
    int m_statusCounter = 0;

    std::unique_ptr<GGMorse> m_ggmorse;
    QVector<float> m_ggmorseFifo;
    int m_ggmorseSampleRate = 0;
    double m_ggmorseConfiguredFreqHz = -9999.0;
    double m_ggmorseConfiguredToneHz = -1.0;
    double m_ggmorseConfiguredWpm = -999.0;
    double m_ggmorseConfiguredMinHz = -1.0;
    double m_ggmorseConfiguredMaxHz = -1.0;
    double m_ggmorseSmoothedWpm = 20.0;
    double m_ggmorseLastCost = 999.0;
    double m_ggmorseLastThreshold = 0.0;
    double m_inputRms = 0.0;
    double m_inputPeak = 0.0;
    double m_inputGain = 1.0;

    QString m_text;
    int m_decodedChars = 0;
};

#endif // CWDECODER_H
