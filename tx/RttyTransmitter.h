#ifndef RTTYTRANSMITTER_H
#define RTTYTRANSMITTER_H

#include "TxModulator.h"

#include <QHash>
#include <QVector>

#include <atomic>

/**
 * @brief Real-time AFSK RTTY text transmitter.
 *
 * Purpose:
 * - Generate continuous-phase AFSK for ITA2/Baudot RTTY.
 * - Support common amateur baud/shift/mark/space settings.
 * - Provide a text preview compatible with the common TX engine.
 */
class RttyTransmitter : public TxModulator
{
public:
    /**
     * @brief Creates one complete RTTY text transmission.
     */
    RttyTransmitter(const QString &text,
                    int sampleRate,
                    double baudRate,
                    double markHz,
                    double spaceHz,
                    bool reverse);

    int sampleRate() const override;
    int generate(float *output, int sampleCount) override;
    bool isFinished() const override;
    double progress() const override;
    QImage previewImage() const override;
    QString description() const override;
    bool rttyToneState(bool *markOut) const override;

    /**
     * @brief Builds a simple rendered preview for TX text.
     */
    static QImage previewTextImage(const QString &text);

private:
    struct Segment
    {
        bool mark = true;
        int samples = 0;
    };

private:
    void buildSegments(const QString &text);
    void appendBit(bool mark, double bitCount = 1.0);
    void appendCharacter(QChar ch);
    void appendCode(int code);
    bool findIta2Code(QChar ch, int &code, bool &figures) const;
    void ensureShift(bool figures);
    bool currentMarkState() const;
    double currentFrequency() const;

private:
    QString m_text;
    int m_sampleRate = 48000;
    double m_baudRate = 45.45;
    double m_markHz = 2125.0;
    double m_spaceHz = 2295.0;
    bool m_reverse = false;
    double m_symbolSamples = 1056.0;

    QVector<Segment> m_segments;
    int m_segmentIndex = 0;
    int m_segmentRemaining = 0;
    qint64 m_totalSamples = 0;
    qint64 m_generatedSamples = 0;
    double m_phase = 0.0;
    bool m_figuresShift = false;
    std::atomic_bool m_scopeMarkState { true };
};

#endif // RTTYTRANSMITTER_H
