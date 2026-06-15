#ifndef HELLSCHREIBERTRANSMITTER_H
#define HELLSCHREIBERTRANSMITTER_H

#include "TxModulator.h"
#include "../modems/hell/HellschreiberDecoder.h"

#include <QImage>
#include <QString>
#include <QVector>

/**
 * @brief Real-time Feld Hell / Hellschreiber text transmitter.
 *
 * Purpose:
 * - Render text into a low-height Hellschreiber raster.
 * - Send each column top-to-bottom as keyed audio tone amplitude for Feld Hell.
 * - Send each raster pixel as one of two FSK tones for FSK-105.
 * - Keep TX timing compatible with the classic 17.5 columns/s Feld Hell rate.
 */
class HellschreiberTransmitter : public TxModulator
{
public:
    /**
     * @brief Creates one complete Hellschreiber text transmission.
     */
    HellschreiberTransmitter(const QString &text,
                             int sampleRate,
                             double toneHz,
                             double columnRate,
                             HellschreiberDecoder::Variant variant = HellschreiberDecoder::Variant::FeldHell,
                             double fskShiftHz = 55.0);

    int sampleRate() const override;
    int generate(float *output, int sampleCount) override;
    bool isFinished() const override;
    double progress() const override;
    QImage previewImage() const override;
    QString description() const override;

    /**
     * @brief Builds a readable preview for the outgoing Hellschreiber raster.
     */
    static QImage previewTextImage(const QString &text,
                                   HellschreiberDecoder::Variant variant = HellschreiberDecoder::Variant::FeldHell,
                                   double columnRate = 17.5);

private:
    /**
     * @brief Builds the 14-row transmit raster from text.
     */
    QImage buildTransmitRaster(const QString &text) const;

    /**
     * @brief Returns whether one TX raster pixel should key/shift the carrier.
     */
    bool pixelOn(int column, int row) const;

private:
    static constexpr int kRasterHeight = 14;

    QString m_text;
    int m_sampleRate = 48000;
    double m_toneHz = 1000.0;
    double m_columnRate = 17.5;
    double m_fskShiftHz = 55.0;
    double m_pixelRate = 245.0;
    double m_samplesPerPixel = 195.9;
    HellschreiberDecoder::Variant m_variant = HellschreiberDecoder::Variant::FeldHell;

    QImage m_raster;
    QImage m_preview;

    qint64 m_totalSamples = 0;
    qint64 m_generatedSamples = 0;
    double m_phase = 0.0;
    double m_envelope = 0.0;
    QVector<float> m_waveform;
};

#endif // HELLSCHREIBERTRANSMITTER_H
