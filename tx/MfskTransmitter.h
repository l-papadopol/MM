#ifndef MFSKTRANSMITTER_H
#define MFSKTRANSMITTER_H

#include "TxModulator.h"
#include "../modems/mfsk/MfskDecoder.h"

#include <QImage>
#include <QString>
#include <QVector>

/**
 * @brief Experimental self-contained MFSK16/MFSK32 text transmitter.
 */
class MfskTransmitter : public TxModulator
{
public:
    MfskTransmitter(const QString &text,
                    int sampleRate,
                    double centerHz,
                    MfskDecoder::Variant variant);

    int sampleRate() const override;
    int generate(float *output, int sampleCount) override;
    bool isFinished() const override;
    double progress() const override;
    QImage previewImage() const override;
    QString description() const override;

    static QImage previewTextImage(const QString &text, MfskDecoder::Variant variant);

private:
    int toneCount() const;
    double symbolRate() const;
    double toneSpacingHz() const;
    double firstToneHz() const;
    void buildTones(const QString &text);
    void appendCharacter(QChar ch);
    double frequencyForTone(int toneIndex) const;

private:
    QString m_text;
    int m_sampleRate = 48000;
    double m_centerHz = 1000.0;
    MfskDecoder::Variant m_variant = MfskDecoder::Variant::Mfsk16;
    QVector<int> m_tones;
    int m_toneIndex = 0;
    double m_symbolSamples = 3072.0;
    double m_samplesInSymbol = 0.0;
    double m_carrierPhase = 0.0;
    qint64 m_totalSamples = 0;
    qint64 m_generatedSamples = 0;
};

#endif // MFSKTRANSMITTER_H
