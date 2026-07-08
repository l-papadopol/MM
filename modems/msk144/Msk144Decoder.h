#ifndef MSK144DECODER_H
#define MSK144DECODER_H

#include "../../audio/AudioBlock.h"

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QVector>
#include <complex>
#include <functional>
#include <atomic>

struct Msk144Decode
{
    QDateTime utc;
    double tSeconds = 0.0;
    int snrDb = 0;
    int dfHz = 0;
    double frequencyHz = 1500.0;
    QString message;
    int navg = 0;
    double eye = 0.0;
    bool shortMessage = false;
};
Q_DECLARE_METATYPE(Msk144Decode)

class Msk144Decoder : public QObject
{
    Q_OBJECT
public:
    explicit Msk144Decoder(QObject *parent = nullptr);

    static QString modeName() { return QStringLiteral("MSK144"); }

    void setPeriodSeconds(int seconds);
    void setDecodeDepth(int depth); // 1 fast, 2 normal, 3 deep
    void setRxFrequencyHz(int hz);
    void setDfToleranceHz(int hz);
    void setShortMessagesEnabled(bool enabled);
    void setSwlEnabled(bool enabled);
    void setContestModeEnabled(bool enabled);
    void setMyCall(const QString &call);
    void setDxCall(const QString &call);
    void setMindIntegrationState(bool hardBypass,
                                 bool scoringEnabled,
                                 bool sampleExportEnabled,
                                 bool assistedRankingEnabled);
    void setMindScoreCallback(const std::function<bool(const QVector<float> &, double *, bool *)> &callback);
    void setMindSampleCallback(const std::function<void(const QVector<float> &, bool, const QString &)> &callback);

public slots:
    void reset();
    void processAudioBlock(const AudioBlock &block);
    void flushPeriod();

signals:
    void decoded(const Msk144Decode &decode);
    void statusChanged(const QString &status);
    void pingDetected(double frequencyHz, int snrDb, double tSeconds);
    void periodReady(int secondsBuffered, int periodSeconds);
    void mindStats(int scoredCandidates, int promotedCandidates, double averageConfidencePercent);

private:
    void appendResampledTo12k(const AudioBlock &block);
    void analyzeRecentPingWindow();
    void tryPeriodDecode(bool force);
    void tryPeriodDecodeSync(bool force);
    bool tryDecodeFrameAt(int startSample, double frequencyHz, Msk144Decode &decode) const;
    bool tryDecodeCoherentAt(int startSample, double frequencyHz, Msk144Decode &decode) const;
    bool decodeMsk144Frame(const QVector<std::complex<double>> &frame, QString &message, double &qualityMetric) const;
    void makeBasebandFrame(int startSample, double frequencyHz, QVector<std::complex<double>> &frame) const;
    QVector<float> makeMindCandidateFeatures(int startSample, double frequencyHz) const;
    double estimateFrameSnrDb(int startSample, int frameSamples) const;
    double bandEnergyGoertzel(const QVector<float> &samples, int start, int count, double frequencyHz) const;
    QString backendStatusText() const;

private:
    QVector<float> m_samples12k;
    qint64 m_totalInputSamples = 0;
    qint64 m_total12kSamples = 0;
    int m_inputSampleRate = 48000;
    int m_periodSeconds = 15;
    int m_decodeDepth = 2;
    int m_rxFrequencyHz = 1500;
    int m_dfToleranceHz = 100;
    bool m_shortMessages = false;
    bool m_swl = false;
    bool m_contest = false;
    QString m_myCall;
    QString m_dxCall;
    bool m_mindHardBypass = true;
    bool m_mindScoringEnabled = false;
    bool m_mindSampleExportEnabled = false;
    bool m_mindAssistedRankingEnabled = false;
    std::function<bool(const QVector<float> &, double *, bool *)> m_mindScoreCallback;
    std::function<void(const QVector<float> &, bool, const QString &)> m_mindSampleCallback;
    QDateTime m_periodStartUtc;
    qint64 m_nextPingAnalysisSample = 0;
    QString m_lastStatus;
    std::atomic_bool m_decodeInProgress{false};
    bool m_asyncDecodeEnabled = true;
};

#endif // MSK144DECODER_H
