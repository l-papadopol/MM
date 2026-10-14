#pragma once
#include "../../audio/AudioBlock.h"
#include "../../dsp/text/LinearResampler.h"
#include "../../dsp/text/SkimmerSpot.h"
#include "../../dsp/text/SkimmerCqTracker.h"
#include "RttyDecoder.h"
#include <QObject>

// Independent ITA2 receivers on the RX worker, sharing one resampled stream.
class RttyMultiDecoder : public QObject {
    Q_OBJECT
public:
    explicit RttyMultiDecoder(QObject* parent = nullptr);
    void reset();
    void resumeAfterLocalTransmit();
    void processAudioBlock(const AudioBlock& block);
    void configure(double baud, int shiftHz, bool reverse, bool enabled, int maxDecoders);
signals:
    void skimmerSpotsChanged(const QVector<SkimmerSpot>& spots);
private:
    struct Candidate { int markHz = 0, spaceHz = 0; double score = 0; };
    struct Track {
        RttyDecoder* decoder = nullptr;
        quint64 id = 0;
        int markHz = 0, spaceHz = 0;
        double score = 0;
        qint64 lastSeenSample = 0;
        QString recentText;
        SkimmerCqTracker cq;
    };
    QVector<Candidate> scanCandidates(const AudioBlock& block) const;
    void addOrRefreshTrack(const Candidate& candidate, qint64 sampleIndex);
    void pruneTracks(qint64 sampleIndex);
    void appendTrackText(RttyDecoder* decoder, const QString& text);
    void publishSpots();
    static double goertzelPower(const QVector<float>& samples, int sampleRate, double frequencyHz);
    bool m_enabled = false, m_reverse = false;
    int m_maxDecoders = 12, m_shiftHz = 170;
    double m_baud = 45.45;
    int m_sampleRate = 12000, m_inputSampleRate = 0;
    quint64 m_nextId = 1;
    qint64 m_samplesProcessed = 0, m_samplesUntilScan = 0, m_publishSamples = 0;
    LinearResampler m_resampler;
    QVector<float> m_scanBuffer;
    QVector<Track> m_tracks;
    QVector<Candidate> m_previousCandidates;
};
