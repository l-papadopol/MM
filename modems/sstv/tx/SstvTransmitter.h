#ifndef SSTVTRANSMITTER_H
#define SSTVTRANSMITTER_H

#include "../../../core/tx/TxModulator.h"

#include <QVector>

/**
 * @brief Generates Martin/Scottie SSTV audio from a still image.
 *
 * Purpose:
 * - Adapt an arbitrary image to the selected SSTV mode size.
 * - Generate sync, porch and RGB video tones in real time.
 * - Provide a simple manual-mode TX path matching the first SSTV RX modes.
 */
class SstvTransmitter final : public TxModulator
{
public:
    /**
     * @brief Creates an SSTV transmitter for a supported Martin/Scottie mode.
     */
    SstvTransmitter(const QImage &sourceImage,
                    const QString &modeName,
                    int sampleRate);

    /**
     * @brief Adapts a source image to the selected SSTV mode size.
     */
    static QImage prepareImage(const QImage &sourceImage, const QString &modeName);

    /**
     * @brief Returns true if the selected mode can be transmitted.
     */
    static bool isSupportedMode(const QString &modeName);

    int sampleRate() const override;
    int generate(float *output, int sampleCount) override;
    bool isFinished() const override;
    double progress() const override;
    QImage previewImage() const override;
    QString description() const override;

private:
    struct Segment
    {
        int type = 0;
        int channel = 0;
        double startMs = 0.0;
        double durationMs = 0.0;
        double toneHz = 0.0;
    };

    struct Mode
    {
        QString key;
        QString label;
        int width = 320;
        int height = 256;
        int transmittedLines = 256;
        int outputLinesPerTxLine = 1;
        int encoding = 0;
        double lineMs = 446.446;
        QVector<Segment> segments;
    };

private:
    static QVector<Mode> modes();
    static Mode modeByName(const QString &modeName);

    double frequencyAt(qint64 sampleIndex) const;
    double lineFrequency(double linePositionMs, int txLine) const;
    double videoFrequency(int x, int txLine, int channel) const;
    int yuvComponentValue(int x, int txLine, int channel) const;
    int lumaValue(int x, int displayLine) const;
    int redChromaValue(int x, int displayLineA, int displayLineB) const;
    int blueChromaValue(int x, int displayLineA, int displayLineB) const;
    float nextSample(double frequencyHz);

private:
    Mode m_mode;
    QImage m_image;

    int m_sampleRate = 48000;
    double m_lineSamples = 21429.0;
    qint64 m_position = 0;
    qint64 m_totalSamples = 0;
    qint64 m_leadSamples = 0;
    qint64 m_imageSamples = 0;
    qint64 m_tailSamples = 0;

    double m_phase = 0.0;
};

#endif // SSTVTRANSMITTER_H
