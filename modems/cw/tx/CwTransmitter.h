#ifndef CWTRANSMITTER_H
#define CWTRANSMITTER_H

#include "../../../core/tx/TxModulator.h"

#include <QString>
#include <QVector>

/**
 * @brief Clean continuous-wave Morse transmitter.
 *
 * Generates keyed AFSK CW from a pure sine carrier.  The keying envelope is
 * raised-cosine shaped to avoid clicks, and the generated audio is passed
 * through a narrow cascaded band-pass filter centred on the selected CW pitch.
 * PTT/CAT keying is still handled by MainWindow/TxAudioEngine exactly like the
 * other text modes; this class only renders the audio waveform.
 */
class CwTransmitter : public TxModulator
{
public:
    CwTransmitter(const QString &text,
                  int sampleRate,
                  double toneHz,
                  double wpm);

    int sampleRate() const override;
    int generate(float *output, int sampleCount) override;
    bool isFinished() const override;
    double progress() const override;
    QImage previewImage() const override;
    QString description() const override;

    static QImage previewTextImage(const QString &text);

private:
    struct Segment
    {
        bool keyDown = false;
        qint64 samples = 0;
    };

    struct Biquad
    {
        double b0 = 1.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;
        double z1 = 0.0;
        double z2 = 0.0;

        void configureBandPass(double sampleRate, double centerHz, double bandwidthHz);
        void reset();
        double process(double x);
    };

    void buildSegments(const QString &text);
    void appendKey(bool down, double dotUnits);
    QString morseForChar(QChar ch) const;
    void configureOutputFilter();
    double shapedEnvelope(const Segment &seg) const;
    double filteredCarrierSample(double envelope);

private:
    QString m_text;
    int m_sampleRate = 48000;
    double m_toneHz = 1000.0;
    double m_wpm = 20.0;
    double m_dotSamples = 2880.0;
    double m_phase = 0.0;
    qint64 m_edgeSamples = 384;

    Biquad m_bandPass1;
    Biquad m_bandPass2;
    Biquad m_bandPass3;

    QVector<Segment> m_segments;
    int m_segmentIndex = 0;
    qint64 m_segmentSampleIndex = 0;
    qint64 m_totalSamples = 0;
    qint64 m_generatedSamples = 0;
};

#endif // CWTRANSMITTER_H
