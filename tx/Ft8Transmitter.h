#ifndef FT8TRANSMITTER_H
#define FT8TRANSMITTER_H

#include "TxModulator.h"

#include <QImage>
#include <QString>
#include <QVector>

/**
 * @brief Generates FT-family transmit audio for FT8 and FT4.
 *
 * FT8 and FT4 both use the MSHV GPL generator ports.  MM only trims the
 * generator's post-frame silence for exact slot-timed PTT/audio return.
 */
class Ft8Transmitter final : public TxModulator
{
public:
    Ft8Transmitter(const QString &message,
                   int sampleRate,
                   double frequencyHz);

    Ft8Transmitter(const QString &modeName,
                   const QString &message,
                   int sampleRate,
                   double frequencyHz,
                   int leadingSilenceMs = 0);

    Ft8Transmitter(int sampleRate,
                   double frequencyHz,
                   double durationSeconds,
                   bool tuneMode);

    Ft8Transmitter(const QString &modeName,
                   int sampleRate,
                   double frequencyHz,
                   double durationSeconds,
                   bool tuneMode);

    int sampleRate() const override;
    int generate(float *output, int sampleCount) override;
    bool isFinished() const override;
    double progress() const override;
    QImage previewImage() const override;
    QString description() const override;
    bool lowLatencyTx() const override;
    int trailingSilenceSamples() const override;

    QString normalizedMessage() const;
    bool generationSucceeded() const;
    QString generationError() const;
    QString modeName() const;
    void skipInitialMilliseconds(int milliseconds);
    void prependLeadingSilenceMilliseconds(int milliseconds);

private:
    void buildMessageWaveform(const QString &message, double frequencyHz);
    void prependSilence(int milliseconds);
    void skipInitialSamples(int samples);
    void buildFt8MessageWaveform(const QString &message, double frequencyHz);
    void buildFt4MessageWaveform(const QString &message, double frequencyHz);
    void buildTuneWaveform(double frequencyHz, double durationSeconds);
    QImage makePreviewImage() const;

private:
    int m_sampleRate = 48000;
    double m_frequencyHz = 1500.0;
    QString m_modeName = QStringLiteral("FT8");
    QString m_message;
    QString m_unpackedMessage;
    QString m_error;
    QVector<float> m_samples;
    int m_position = 0;
    bool m_tuneMode = false;
    bool m_ok = false;
};

#endif // FT8TRANSMITTER_H
