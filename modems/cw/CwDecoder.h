#ifndef CWDECODER_H
#define CWDECODER_H

#include "../../audio/AudioBlock.h"
#include "../../dsp/FrequencyMarker.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>
#include <string>

namespace madmodem::cwskimmer {
class CwSkimmerEngine;
}

/**
 * @brief CW receive decoder backed by the assimilated FFT multi-channel skimmer.
 *
 * This replaces the previous selected-tone CW RX path.  The decoder scans the
 * CW audio passband as fixed overlapping FFT channels, estimates a per-bin
 * noise floor, applies hysteresis and classifies mark/space timing.
 *
 * Operator control remains simple: RX A is the green waterfall marker selected
 * with left click; RX B is the optional blue marker selected with right click.
 * The skimmer may decode other channels too, but only A/B are promoted to the
 * main RX terminal. Other decoded streams are exposed as transient OSD labels.
 *
 * Legacy bandwidth/AFC setters are retained only for old settings file
 * compatibility. They are not part of the skimmer decision path.
 */
class CwDecoder : public QObject
{
    Q_OBJECT

public:
    explicit CwDecoder(QObject *parent = nullptr);
    ~CwDecoder() override;

    static QString modeName();
    static QVector<FrequencyMarker> frequencyMarkers(double toneHz = 0.0);

    void reset();
    void processAudioBlock(const AudioBlock &block);

    void setToneHz(double toneHz);
    void setSecondaryToneHz(double toneHz);
    void setSecondaryEnabled(bool enabled);
    void setWpm(double wpm);
    void setAutoWpm(bool enabled);
    void setBandwidthHz(double bandwidthHz);
    void setAfcEnabled(bool enabled);
    void setAfcRangeHz(double rangeHz);

    double toneHz() const;
    double wpm() const;
    bool autoWpm() const;
    double trackedWpm() const;
    double bandwidthHz() const;
    QString receivedText() const;

signals:
    // Legacy signal kept for compatibility; the skimmer path primarily emits
    // priorityTextReceived(rank,text), rank 0=A and rank 1=B.
    void characterReceived(const QString &text);
    void priorityTextReceived(int rank, const QString &text);
    void textUpdated(const QString &text);
    void statusChanged(const QString &status);
    void markersChanged(const QVector<FrequencyMarker> &markers);
    void speedEstimateChanged(double wpm);
    void skimmerOverlaysChanged(const QStringList &labels,
                                const QVector<double> &frequenciesHz,
                                const QVector<float> &confidences);

private:
    void resetSkimmer();
    void emitSkimmerStatus(bool force = false);
    void refreshPriorityAndOverlays();
    int channelForFrequency(double frequencyHz) const;
    int priorityRankForChannel(int channelIndex) const;
    QString sanitizeSkimmerText(const std::string &text) const;

private:
    double m_toneHz = 700.0;        // RX A operator marker; skimmer scans all channels.
    double m_secondaryToneHz = 1200.0;
    bool m_secondaryEnabled = false;
    double m_wpm = 20.0;            // Manual hint retained for UI/status compatibility.
    bool m_autoWpm = true;
    double m_bandwidthHz = 120.0;   // Legacy control retained; not a pre-filter.
    bool m_afcEnabled = false;      // Retained setting; ignored by skimmer RX.
    double m_afcRangeHz = 20.0;

    int m_sampleRate = 0;
    qint64 m_sampleCounter = 0;
    int m_statusCounter = 0;
    double m_trackedWpm = 20.0;
    QString m_text;

    std::unique_ptr<madmodem::cwskimmer::CwSkimmerEngine> m_skimmer;
    QVector<QString> m_lastRollingByChannel;
    QVector<int> m_priorityChannels;
};

#endif // CWDECODER_H
