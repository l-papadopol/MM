#ifndef BPSK31TRANSMITTER_H
#define BPSK31TRANSMITTER_H

#include "TxModulator.h"

#include <QString>
#include <QVector>

/**
 * @brief Real-time BPSK31/BPSK63/BPSK125 text transmitter.
 *
 * Purpose:
 * - Generate continuous-phase BPSK audio using PSK31 Varicode.
 * - Use cosine-shaped phase reversals for cleaner occupied bandwidth.
 * - Provide a text preview compatible with the common TX engine.
 */
class Bpsk31Transmitter : public TxModulator
{
public:
    /**
     * @brief Creates one complete BPSK31 text transmission.
     */
    Bpsk31Transmitter(const QString &text,
                      int sampleRate,
                      double toneHz,
                      double symbolRate,
                      bool invertBits,
                      bool qpskMode = false);

    int sampleRate() const override;
    int generate(float *output, int sampleCount) override;
    bool isFinished() const override;
    double progress() const override;
    QImage previewImage() const override;
    QString description() const override;

    /**
     * @brief Builds a simple rendered preview for TX text.
     */
    static QImage previewTextImage(const QString &text);

private:
    /**
     * @brief Converts user text into BPSK31 Varicode bits.
     */
    void buildBits(const QString &text);

    /**
     * @brief Appends one bit to the TX bitstream.
     */
    void appendBit(bool bitOne);

    /**
     * @brief Appends a character as Varicode plus 00 separator.
     */
    void appendCharacter(QChar ch);

    /**
     * @brief Returns the Varicode bit string for one character.
     */
    QString varicodeForChar(QChar ch) const;

    /**
     * @brief Sets symbol phase transition parameters for the current bit.
     */
    void setupCurrentSymbol();

private:
    QString m_text;
    int m_sampleRate = 48000;
    double m_toneHz = 1000.0;
    bool m_invertBits = false;
    bool m_qpskMode = false;
    double m_symbolRate = 31.25;
    double m_symbolSamples = 1536.0;

    QVector<bool> m_bits;
    int m_bitIndex = 0;
    double m_samplesInSymbol = 0.0;
    qint64 m_totalSamples = 0;
    qint64 m_generatedSamples = 0;

    double m_carrierPhase = 0.0;
    double m_dataPhase = 0.0;
    double m_symbolStartPhase = 0.0;
    double m_symbolEndPhase = 0.0;
};

#endif // BPSK31TRANSMITTER_H
