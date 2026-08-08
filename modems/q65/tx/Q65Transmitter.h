#ifndef Q65TRANSMITTER_H
#define Q65TRANSMITTER_H

#include "../../../core/tx/TxModulator.h"
#include "../Q65Mode.h"

#include <QImage>
#include <QString>
#include <QVector>

class Q65Transmitter final : public TxModulator
{
public:
    Q65Transmitter(const QString &message,
                   int sampleRate,
                   int periodSeconds,
                   Q65Mode::Submode submode,
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
    QImage makePreviewImage() const;

private:
    QString m_message;
    QString m_unpackedMessage;
    QString m_error;
    int m_sampleRate = 48000;
    int m_periodSeconds = 60;
    Q65Mode::Submode m_submode = Q65Mode::Submode::A;
    double m_txFrequencyHz = 1500.0;
    QVector<float> m_samples;
    int m_position = 0;
    bool m_ok = false;
};

#endif // Q65TRANSMITTER_H
