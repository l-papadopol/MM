#ifndef MFSKDECODER_H
#define MFSKDECODER_H

#include "../../audio/AudioBlock.h"
#include "../../dsp/FrequencyMarker.h"
#include "../../dsp/text/LinearResampler.h"
#include "../../dsp/text/GoertzelToneBank.h"

#include <QObject>
#include <QString>
#include <QVector>

/**
 * @brief Experimental MFSK text receiver for MadModem.
 *
 * v1.43 adds a lightweight, self-contained MFSK16/MFSK32 RX path inspired by
 * fldigi-style MFSK operator workflow.  It intentionally stays conservative:
 * fixed symbol timing, tone-bank energy detection and a simple framed ASCII
 * payload are used so the feature can be exercised before a later full
 * fldigi-compatible Varicode/FEC interop core is added.
 */
class MfskDecoder : public QObject
{
    Q_OBJECT

public:
    enum class Variant
    {
        Mfsk16,
        Mfsk32
    };

    explicit MfskDecoder(QObject *parent = nullptr);

    static QString modeName();
    static QString variantName(Variant variant);
    static Variant variantFromKey(const QString &key);
    static QVector<FrequencyMarker> frequencyMarkers(double centerHz, Variant variant);

    void reset();
    void processAudioBlock(const AudioBlock &block);

    void setVariant(Variant variant);
    void setCenterHz(double centerHz);
    void setAfcEnabled(bool enabled);
    void setAfcRangeHz(double rangeHz);

    Variant variant() const;
    double centerHz() const;
    bool afcEnabled() const;
    double afcRangeHz() const;
    QString receivedText() const;

signals:
    void characterReceived(const QString &text);
    void textUpdated(const QString &text);
    void statusChanged(const QString &status);
    void markersChanged(const QVector<FrequencyMarker> &markers);

private:
    int toneCount() const;
    double symbolRate() const;
    double toneSpacingHz() const;
    double firstToneHz() const;
    void configureForCurrentSettings();
    void processInternalSample(double sample);
    void processSymbol(const QVector<double> &symbol);
    int detectTone(const QVector<double> &symbol, double *confidenceOut, double *offsetHzOut);
    void handleTone(int toneIndex, double confidence);
    void updateAfcFromSymbol(double offsetHz, double confidence);
    void finishCharacter(int code);
    void maybeEmitStatus();

private:
    static constexpr double kInternalSampleRate = 8000.0;

    Variant m_variant = Variant::Mfsk16;
    double m_centerHz = 1000.0;
    bool m_afcEnabled = false;
    double m_afcRangeHz = 50.0;

    int m_inputSampleRate = 0;
    LinearResampler m_resampler;
    QVector<double> m_symbolBuffer;
    double m_symbolSamples = 512.0;
    double m_symbolPhase = 0.0;
    GoertzelToneBank m_toneBank;
    double m_effectiveCenterHz = 1000.0;
    double m_afcOffsetHz = 0.0;

    int m_rxState = 0;
    int m_firstDataTone = 0;
    QString m_text;
    int m_decodedChars = 0;
    int m_badFrames = 0;
    int m_statusCounter = 0;
    double m_lastConfidence = 0.0;
    double m_lastToneOffsetHz = 0.0;
    qint64 m_symbolsSeen = 0;
};

#endif // MFSKDECODER_H
