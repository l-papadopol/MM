#ifndef WEATHERFAXTRANSMITTER_H
#define WEATHERFAXTRANSMITTER_H

#include "../../../core/tx/TxModulator.h"

#include <QImage>

/**
 * @brief Generates HF WEFAX audio from a still image.
 *
 * Purpose:
 * - Adapt an arbitrary image to WEFAX grayscale raster format.
 * - Convert grayscale pixels into a continuous audio FM tone.
 * - Provide lead-in/phasing/tail audio suitable for first TX tests.
 */
class WeatherFaxTransmitter final : public TxModulator
{
public:
    /**
     * @brief Creates a WEFAX transmitter from a source image.
     */
    WeatherFaxTransmitter(const QImage &sourceImage,
                          int sampleRate,
                          int lpm,
                          double blackHz,
                          double whiteHz,
                          int targetWidth = 800);

    /**
     * @brief Adapts a source image to WEFAX grayscale raster size.
     */
    static QImage prepareImage(const QImage &sourceImage, int targetWidth = 800);

    int sampleRate() const override;
    int generate(float *output, int sampleCount) override;
    bool isFinished() const override;
    double progress() const override;
    QImage previewImage() const override;
    QString description() const override;

private:
    /**
     * @brief Returns the instantaneous tone frequency at a global sample index.
     */
    double frequencyAt(qint64 sampleIndex) const;

    /**
     * @brief Converts one image pixel to WEFAX video frequency.
     */
    double pixelFrequency(int x, int y) const;

    /**
     * @brief Appends one continuous-phase audio sample.
     */
    float nextSample(double frequencyHz);

private:
    QImage m_image;

    int m_sampleRate = 48000;
    int m_lpm = 120;
    double m_blackHz = 1500.0;
    double m_whiteHz = 2300.0;
    double m_centerHz = 1900.0;
    double m_lineSamples = 24000.0;

    qint64 m_position = 0;
    qint64 m_totalSamples = 0;
    qint64 m_leadSamples = 0;
    qint64 m_phasingSamples = 0;
    qint64 m_imageSamples = 0;
    qint64 m_tailSamples = 0;

    double m_phase = 0.0;
};

#endif // WEATHERFAXTRANSMITTER_H
