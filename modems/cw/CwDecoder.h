#ifndef CWDECODER_H
#define CWDECODER_H

#include "../../audio/AudioBlock.h"
#include "../../dsp/FrequencyMarker.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMetaType>

#include <memory>
#include <string>

namespace madmodem::cwskimmer {
class CwSkimmerEngine;
class SelectedToneCwTracker;
}


struct CwDiagnosticPoint
{
    double timestampSec = 0.0;
    float acquisitionEnvelope = 0.0f;
    float filteredEnvelope = 0.0f;
    float signalLevel = 0.0f;
    float thresholdLow = 0.0f;
    float thresholdHigh = 0.0f;
    float markProbability = 0.0f;
    bool qsbErasure = false;
    float qsbErasureStartSec = 0.0f;
    float qsbErasureEndSec = 0.0f;
    bool keyDown = false;
    float markerHz = 0.0f;
    float trackedHz = 0.0f;
    float snrDb = -99.0f;
    float requestedBandwidthHz = 0.0f;
    float effectiveBandwidthHz = 0.0f;
    float wpm = 0.0f;
    float lockQuality = 0.0f;
    bool trackingConfirmed = false;
    float carrierProminenceDb = -99.0f;
    float coherentSnrDb = -99.0f;
    float coherence = 0.0f;
};
Q_DECLARE_METATYPE(CwDiagnosticPoint)

/**
 * @brief Native MadModem CW receiver.
 *
 * A full-passband FFT scanner discovers persistent carrier lanes. RX A and RX B
 * are two independent exact-tone receivers using the same clean-room native
 * soft-decision Bayesian timing decoder. The waterfall shows carrier lanes and
 * A/B markers only; decoded text is emitted continuously to the RX panes.
 *
 * WPM is an acquisition hint and a derived display value, never a rigid clock.
 * SNR changes observation confidence but never rewrites measured durations.
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
    void clearReceiver(int rank);
    void processAudioBlock(const AudioBlock &block);

    void setToneHz(double toneHz);
    void setSecondaryToneHz(double toneHz);
    void setSecondaryEnabled(bool enabled);
    void setWpm(double wpm);                 // Compatibility: apply to RX A and RX B.
    void setAutoWpm(bool enabled);           // Compatibility: apply to RX A and RX B.
    void setReceiverWpm(int rank, double wpm);
    void setReceiverAutoWpm(int rank, bool enabled);
    void setBandwidthHz(double bandwidthHz);
    void setAutoBandwidth(bool enabled);
    void setAfcEnabled(bool enabled);
    void setAfcRangeHz(double rangeHz);

    double toneHz() const;
    double wpm() const;
    bool autoWpm() const;
    double receiverWpm(int rank) const;
    bool receiverAutoWpm(int rank) const;
    double trackedWpm() const;
    double bandwidthHz() const;
    double trackedToneHz(int rank) const;
    double effectiveBandwidthHz(int rank) const;
    double acquisitionBandwidthHz(int rank) const;
    QString trackingState(int rank) const;
    QString receivedText() const;

signals:
    // Compatibility signal; RX A/B normally use priorityTextReceived.
    void characterReceived(const QString &text);
    void priorityTextReceived(int rank, const QString &text);
    void textUpdated(const QString &text);
    void statusChanged(const QString &status);
    void markersChanged(const QVector<FrequencyMarker> &markers);
    void speedEstimateChanged(double wpm);
    void receiverSpeedEstimateChanged(int rank, double wpm);
    void skimmerOverlaysChanged(const QStringList &labels,
                                const QVector<double> &frequenciesHz,
                                const QVector<float> &confidences);
    void diagnosticSamplesReady(int rank, const QVector<CwDiagnosticPoint> &samples);
    void diagnosticSpectrumReady(int rank, const QVector<float> &offsetsHz,
                                 const QVector<float> &inputPsdDb,
                                 const QVector<float> &filteredPsdDb,
                                 const QVector<float> &theoreticalResponseDb,
                                 float trackedOffsetHz,
                                 float effectiveBandwidthHz,
                                 float interfererOffsetHz,
                                 float interfererConfidence,
                                 float carrierProminenceDb,
                                 float carrierPeakWidthHz,
                                 float carrierToSecondDb);
    void diagnosticResetRequested(int rank);

private:
    void resetSkimmer();
    void emitSkimmerStatus(bool force = false);
    void refreshPriorityAndOverlays();
    QString sanitizeSkimmerText(const std::string &text) const;

private:
    double m_toneHz = 700.0;        // RX A operator marker; skimmer scans all channels.
    double m_secondaryToneHz = 1200.0;
    bool m_secondaryEnabled = false;
    double m_wpmA = 20.0;           // RX A initial/manual timing prior.
    double m_wpmB = 20.0;           // RX B initial/manual timing prior.
    bool m_autoWpmA = true;
    bool m_autoWpmB = true;
    double m_bandwidthHz = 120.0;   // Total two-sided selected-tone passband.
    bool m_afcEnabled = true;       // Bounded AFC for selected RX A/B trackers.
    bool m_autoBandwidth = true;
    double m_afcRangeHz = 20.0;

    int m_sampleRate = 0;
    qint64 m_sampleCounter = 0;
    int m_statusCounter = 0;
    double m_trackedWpm = 20.0;
    QString m_text;
    QString m_textA;
    QString m_textB;
    QString m_selectedCommittedA;
    QString m_selectedCommittedB;

    std::unique_ptr<madmodem::cwskimmer::CwSkimmerEngine> m_skimmer;
    std::unique_ptr<madmodem::cwskimmer::SelectedToneCwTracker> m_toneTrackerA;
    std::unique_ptr<madmodem::cwskimmer::SelectedToneCwTracker> m_toneTrackerB;
    QVector<CwDiagnosticPoint> m_pendingDiagnosticsA;
    QVector<CwDiagnosticPoint> m_pendingDiagnosticsB;
    CwDiagnosticPoint m_lastDiagnosticA;
    CwDiagnosticPoint m_lastDiagnosticB;
    bool m_haveDiagnosticA = false;
    bool m_haveDiagnosticB = false;
};

#endif // CWDECODER_H
