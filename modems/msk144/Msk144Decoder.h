#ifndef MSK144DECODER_H
#define MSK144DECODER_H

#include "../../audio/AudioBlock.h"
#include "../../dsp/text/LinearResampler.h"

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QVector>
#include <complex>
#include <atomic>
#include <thread>

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
    ~Msk144Decoder() override;

    static QString modeName() { return QStringLiteral("MSK144"); }

    void setPeriodSeconds(int seconds);
    void setDecodeDepth(int depth); // 1 fast, 2 normal, 3 deep
    void setRxFrequencyHz(int hz);
    void setFrequencyToleranceHz(int hz);
    void setShortMessagesEnabled(bool enabled);
    void setSwlEnabled(bool enabled);
    void setContestModeEnabled(bool enabled);
    void setMyCall(const QString &call);
    void setDxCall(const QString &call);
    /** Deterministic offline entry point used by WAV analysis and regressions. */
    QVector<Msk144Decode> decodeRecordedPeriod(const QVector<float> &samples12k,
                                               const QDateTime &periodStartUtc);
public slots:
    void reset();
    void processAudioBlock(const AudioBlock &block);
    void flushPeriod();

signals:
    void decoded(const Msk144Decode &decode);
    void statusChanged(const QString &status);
    void pingDetected(double frequencyHz, int snrDb, double tSeconds);
    void periodReady(int secondsBuffered, int periodSeconds);

private:
    void appendResampledTo12k(const AudioBlock &block);
    void beginUtcPeriod(qint64 periodId, qint64 firstSampleUtcNs);
    void finishUtcPeriod(bool force);
    void analyzeRecentPingWindow();
    void tryPeriodDecode(bool force);
    void tryPeriodDecodeSync(bool force);
    bool tryDecodeFrameAt(int startSample, double frequencyHz, Msk144Decode &decode) const;
    bool tryDecodeShortAt(int startSample, double frequencyHz, Msk144Decode &decode) const;
    bool tryDecodeCoherentAt(int startSample, double frequencyHz, Msk144Decode &decode) const;
    bool decodeMsk144Frame(const QVector<std::complex<double>> &frame, QString &message, double &qualityMetric) const;
    bool decodeMsk40Frame(const QVector<std::complex<double>> &frame, QString &message, double &qualityMetric) const;
    double frameSyncMetricAt(int startSample, int frameSamples,
                             double frequencyHz, bool shortFrame) const;
    void makeBasebandFrame(int startSample, double frequencyHz, QVector<std::complex<double>> &frame) const;
    void makeMixedBaseband(int startSample, int sampleCount, double frequencyHz,
                           QVector<std::complex<double>> &frame) const;
    void makeBaseband(int startSample, int sampleCount, double frequencyHz,
                      QVector<std::complex<double>> &frame) const;
    double estimateFrameSnrDb(int startSample, int frameSamples) const;
    double bandEnergyGoertzel(const QVector<float> &samples, int start, int count, double frequencyHz) const;
    QString backendStatusText() const;

private:
    QVector<float> m_samples12k;
    LinearResampler m_resampler;
    qint64 m_totalInputSamples = 0;
    qint64 m_total12kSamples = 0;
    int m_inputSampleRate = 48000;
    int m_periodSeconds = 15;
    int m_decodeDepth = 2;
    int m_rxFrequencyHz = 1500;
    int m_frequencyToleranceHz = 200;
    bool m_shortMessages = false;
    bool m_swl = false;
    bool m_contest = false;
    QString m_myCall;
    QString m_dxCall;
    QDateTime m_periodStartUtc;
    qint64 m_currentPeriodId = -1;
    qint64 m_nextOutputUtcNs = 0;
    qint64 m_outputTimeRemainder = 0;
    qint64 m_lastInputEndUtcNs = 0;
    quint64 m_captureGeneration = 0;
    bool m_periodTimelineValid = false;
    qint64 m_nextPingAnalysisSample = 0;
    QString m_lastStatus;
    std::atomic_bool m_decodeInProgress{false};
    std::atomic<quint64> m_decodeGeneration{0};
    std::thread m_decodeThread;
    bool m_asyncDecodeEnabled = true;
};

#endif // MSK144DECODER_H
