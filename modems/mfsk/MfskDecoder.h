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
 * @brief MFSK text receiver.
 *
 * v0.5.22 replaces the former internal/framed MFSK16 receiver with a
 * fldigi/gMFSK-style MFSK16 text pipeline: 16 tones at 15.625 baud,
 * Gray tone weighting, IZ8BLY Varicode, R=1/2 K=7 convolutional FEC,
 * and the 10-stage 4x4 diagonal de-interleaver.  MFSK32 is kept as a
 * legacy experimental variant until a matching standard MFSK32/FEC path
 * is added.
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
    struct ViterbiRuntime
    {
        QVector<double> metrics;
        QVector<QVector<unsigned char>> paths;
        int steps = 0;
        double lastMetric = 0.0;
    };

    int toneCount() const;
    double symbolRate() const;
    double toneSpacingHz() const;
    double firstToneHz() const;
    void configureForCurrentSettings();
    void processInternalSample(double sample);
    void processSymbol(const QVector<double> &symbol);
    int detectTone(const QVector<double> &symbol, double *confidenceOut, double *offsetHzOut);
    QVector<unsigned char> symbolSoftBits(const QVector<double> &symbol, double *confidenceOut, double *offsetHzOut);

    void handleLegacyTone(int toneIndex, double confidence);
    void finishLegacyCharacter(int code);

    void resetStandardMFSK16();
    void deinterleaveSoftBits(QVector<unsigned char> &softBits);
    void feedSoftBit(unsigned char softBit);
    int viterbiFeed(ViterbiRuntime &decoder, unsigned char softA, unsigned char softB, double *metricOut);
    void receiveDecodedBit(int bit);
    void receiveVaricodeCharacter(int code);
    void updateAfcFromSymbol(double offsetHz, double confidence);
    void maybeEmitStatus();

    static int grayWeightForTone(int toneIndex);
    static int parity7(int value);
    static int varicodeDecode(unsigned int symbol);

private:
    static constexpr double kInternalSampleRate = 8000.0;
    static constexpr int kMfsk16ToneCount = 16;
    static constexpr int kMfsk16SymbolBits = 4;
    static constexpr int kMfsk16Traceback = 45;

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

    // Legacy internal/framed receiver state, still used for MFSK32 only.
    int m_rxState = 0;
    int m_firstDataTone = 0;

    // Standard MFSK16 Varicode/FEC receiver state.
    QVector<unsigned char> m_inlvTable;
    ViterbiRuntime m_viterbi;
    int m_softPairCount = 0;
    unsigned char m_softPair[2] = {0, 0};
    unsigned int m_dataShiftRegister = 0;
    int m_validCharacters = 0;

    QString m_text;
    int m_decodedChars = 0;
    int m_badFrames = 0;
    int m_statusCounter = 0;
    double m_lastConfidence = 0.0;
    double m_lastToneOffsetHz = 0.0;
    double m_lastViterbiMetric = 0.0;
    qint64 m_symbolsSeen = 0;

    // fldigi-style dynamic continuous-wave interference avoidance for MFSK16
    // soft decisions.  Persistent single-tone QRM is punctured toward the
    // average bin level instead of dominating every soft bit.
    QVector<int> m_cwiCounters;
    int m_lastHardTone = -1;
};

#endif // MFSKDECODER_H
