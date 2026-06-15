#ifndef BPSK31DECODER_H
#define BPSK31DECODER_H

#include "../../audio/AudioBlock.h"
#include "../../dsp/FrequencyMarker.h"
#include "../../dsp/text/BaudAwareFilter.h"
#include "../../dsp/text/LinearResampler.h"
#include "../../dsp/text/Nco.h"
#include "../../dsp/text/SoftSquelch.h"
#include "../../dsp/text/SymbolClockDpll.h"

#include <QObject>
#include <QString>
#include <QVector>

/**
 * @brief Streaming BPSK31 decoder.
 *
 * V28 receiver architecture:
 * - internal 8000 Hz CPU DSP rate, giving 256/128/64 samples per symbol at 31.25/62.5/125 baud;
 * - complex NCO/channelizer around the selected audio tone;
 * - baud-aware narrow filter derived from symbol rate;
 * - order-2 Costas loop for BPSK carrier recovery;
 * - fractional symbol clock helper;
 * - differential phase decisions and Varicode parser;
 * - lock/squelch before emitting text, so noise is not treated as traffic.
 *
 * v48 adds a fldigi-inspired BPSK31/BPSK63/BPSK125 selector.
 * v1.42 extends the same Varicode/Costas path to BPSK250/BPSK500/BPSK1000.
 * The core receiver is still the same Costas + differential Varicode path,
 * but symbol clock, channel filter and status/markers now follow the chosen
 * PSK symbol rate instead of being hard-wired to 31.25 baud.
 */
class Bpsk31Decoder : public QObject
{
    Q_OBJECT

public:
    explicit Bpsk31Decoder(QObject *parent = nullptr);

    static QString modeName();
    static QString variantNameForSymbolRate(double symbolRate, bool qpskMode = false);
    static QVector<FrequencyMarker> frequencyMarkers(double toneHz, double symbolRate = 31.25, bool qpskMode = false);

    void reset();
    void processAudioBlock(const AudioBlock &block);

    void setToneHz(double toneHz);
    void setSymbolRate(double symbolRate);
    void setQpskMode(bool enabled);
    void setAfcEnabled(bool enabled);
    void setAfcRangeHz(double rangeHz);
    void setInvertBits(bool invert);
    void setCoherentTrackingEnabled(bool enabled);

    double toneHz() const;
    double symbolRate() const;
    bool qpskMode() const;
    double trackedToneHz() const;
    bool afcEnabled() const;
    double afcRangeHz() const;
    bool invertBits() const;
    bool coherentTrackingEnabled() const;
    QString receivedText() const;

signals:
    void characterReceived(const QString &text);
    void textUpdated(const QString &text);
    void statusChanged(const QString &status);
    void markersChanged(const QVector<FrequencyMarker> &markers);

private:
    void configureForCurrentSettings();
    void processInternalSample(double sample);
    void processSymbol(double symbolI, double symbolQ);
    void updateCostasLoop(double i, double q);
    void updateLockMetrics(double mag, double differentialConfidence);
    void handleVaricodeBit(bool bitOne);
    void finishVaricodeCharacter();
    QString decodeVaricode(const QString &bits) const;
    void maybeEmitStatus();

private:
    static constexpr double kInternalSampleRate = 8000.0;

    double m_toneHz = 1000.0;
    double m_trackedToneHz = 1000.0;
    bool m_afcEnabled = true;
    double m_afcRangeHz = 20.0;
    bool m_invertBits = false;
    bool m_qpskMode = false;
    bool m_coherentTrackingEnabled = true;

    int m_inputSampleRate = 0;
    double m_symbolRate = 31.25;
    double m_symbolSamples = 256.0;
    double m_filterCutoffHz = 52.0;

    LinearResampler m_resampler;
    Nco m_nco;
    BaudAwareComplexLowPass m_channelFilter;
    SymbolClockDpll m_symbolClock;
    SoftSquelch m_lockSquelch;

    double m_costasIntegrator = 0.0;
    double m_costasCorrection = 0.0;
    double m_costasKp = 0.018;
    double m_costasKi = 0.00012;
    double m_costasErrorAvg = 0.0;

    double m_i = 0.0;
    double m_q = 0.0;
    double m_accI = 0.0;
    double m_accQ = 0.0;
    int m_accCount = 0;

    bool m_havePreviousSymbol = false;
    double m_prevI = 0.0;
    double m_prevQ = 0.0;

    bool m_pendingZero = false;
    QString m_currentBits;
    QString m_text;

    double m_signalPower = 1.0e-10;
    double m_noisePower = 1.0e-10;
    double m_snrLike = 1.0;
    double m_phaseConfidence = 0.0;
    bool m_locked = false;

    int m_decodedChars = 0;
    int m_badVaricode = 0;
    int m_statusCounter = 0;
    qint64 m_samplesProcessed = 0;
};

#endif // BPSK31DECODER_H
