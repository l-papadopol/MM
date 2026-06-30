#ifndef MSK144TRANSMITTER_H
#define MSK144TRANSMITTER_H

#include "../../../core/tx/TxModulator.h"

#include <QImage>
#include <QString>
#include <QVector>

class Msk144Transmitter final : public TxModulator
{
public:
    Msk144Transmitter(const QString &message,
                      int sampleRate,
                      int periodSeconds,
                      bool shortMessage,
                      double txFrequencyHz = 1500.0);

    int sampleRate() const override;
    int generate(float *output, int sampleCount) override;
    bool isFinished() const override;
    double progress() const override;
    QImage previewImage() const override;
    QString description() const override;
    bool lowLatencyTx() const override;
    int trailingSilenceSamples() const override;

    bool generationSucceeded() const;
    QString generationError() const;
    QString normalizedMessage() const;

private:
    void buildWaveform();
    void buildFallbackMskLikeWaveform();
    QImage makePreviewImage() const;

private:
    QString m_message;
    QString m_unpackedMessage;
    QString m_error;
    int m_sampleRate = 48000;
    int m_periodSeconds = 15;
    bool m_shortMessage = false;
    double m_txFrequencyHz = 1500.0;
    QVector<float> m_samples;
    int m_position = 0;
    bool m_ok = false;
};

#endif // MSK144TRANSMITTER_H
