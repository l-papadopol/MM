#ifndef RTTYMULTIDECODER_H
#define RTTYMULTIDECODER_H

#include "../../audio/AudioBlock.h"
#include "RttyDecoder.h"

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

/**
 * @brief Lightweight parallel RTTY contest monitor.
 *
 * This object does not replace the operator-selected RttyDecoder.  It scans the
 * live waterfall passband for 45.45-baud/170-Hz style AFSK pairs, attaches a
 * small shadow decoder to the strongest candidates and emits CQ/callsign labels
 * for the waterfall.  The main terminal continues to show only the selected
 * RTTY signal.
 */
class RttyMultiDecoder : public QObject
{
    Q_OBJECT

public:
    struct Callout
    {
        int markHz = 0;
        int spaceHz = 0;
        QString label;
        double score = 0.0;
        bool cq = false;
    };

    explicit RttyMultiDecoder(QObject *parent = nullptr);

    void reset();
    void processAudioBlock(const AudioBlock &block);

    void configure(double baud,
                   int shiftHz,
                   bool reverse,
                   bool enabled,
                   bool overlayEnabled,
                   bool contestEnhanced,
                   bool secondPass,
                   int maxDecoders);

    QVector<Callout> callouts() const;

signals:
    void calloutsChanged(const QVector<RttyMultiDecoder::Callout> &callouts);
    void statusChanged(const QString &status);

private:
    struct Candidate
    {
        int markHz = 0;
        int spaceHz = 0;
        double energy = 0.0;
        double score = 0.0;
    };

    struct Track
    {
        RttyDecoder *decoder = nullptr;
        int markHz = 0;
        int spaceHz = 0;
        double score = 0.0;
        qint64 lastSeenSample = 0;
        QString recentText;
        QString label;
        bool cq = false;
    };

private:
    QVector<Candidate> scanCandidates(const AudioBlock &block,
                                      const QVector<QPair<int, int>> &suppressedBands = {}) const;
    void updateTracks(const QVector<Candidate> &candidates, const AudioBlock &block);
    void addOrRefreshTrack(const Candidate &candidate, qint64 sampleIndex);
    void pruneTracks(qint64 sampleIndex);
    void appendTrackText(RttyDecoder *decoder, const QString &text);
    void rebuildCallouts();
    static QString extractBestLabel(const QString &text, bool *cqOut = nullptr);
    static bool looksLikeCallsign(const QString &token);
    static double goertzelPower(const QVector<float> &samples, int sampleRate, double frequencyHz);

private:
    bool m_enabled = false;
    bool m_overlayEnabled = true;
    bool m_contestEnhanced = false;
    bool m_secondPass = false;
    int m_maxDecoders = 12;
    double m_baud = 45.45;
    int m_shiftHz = 170;
    bool m_reverse = false;

    int m_sampleRate = 0;
    qint64 m_samplesProcessed = 0;
    qint64 m_samplesUntilScan = 0;
    int m_lastScanMs = 0;
    QVector<Track> m_tracks;
    QVector<Callout> m_callouts;
};

Q_DECLARE_METATYPE(RttyMultiDecoder::Callout)
Q_DECLARE_METATYPE(QVector<RttyMultiDecoder::Callout>)

#endif // RTTYMULTIDECODER_H
