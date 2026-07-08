#ifndef DEEPDSPCONTROLLER_H
#define DEEPDSPCONTROLLER_H

#include "DeepDspTinyNet.h"

#include <QObject>
#include <QString>
#include <QVector>
#include <QTimer>
#include <QMutex>
#include <QThread>
#include <deque>

class DeepDspController : public QObject
{
    Q_OBJECT
public:
    struct Status
    {
        bool enabled = true;
        bool assistEnabled = false;
        bool assistRequested = false;
        QString assistMode = QStringLiteral("assisted"); // fixed operator mode: assist requested, readiness-gated
        bool ready = false;
        bool assistReady = false;
        double estimatedExactFrameAccuracy = 0.0;
        QString readinessReason;
        int sampleCount = 0;
        int ftSamples = 0;
        int ft8Samples = 0;
        int ft4Samples = 0;
        int msk144Samples = 0;
        int trainingRuns = 0;
        int validationCount = 0;
        double bitAccuracy = 0.0;
        double bestBitAccuracy = 0.0;
        double messageAccuracy = 0.0;
        double validationAccuracy = 0.0;
        double trainingCompletionPercent = 0.0;
        double lastLoss = 0.0;
        int nativeFtSamples = 0;
        int manualSamples = 0;
        int replayBufferSamples = 0;
        QVector<float> neuralActivity;
        QVector<float> ftActivity;
        QString stateText;
        QString checkpointPath;
        QString statsPath;
        QString goldDatasetPath;
        QString architectureText;
        QString activeProfile;
        QString backendText;
        QString modelStateText;
        QString lastCheckpointText;
        int eigenThreads = 1;
        int ftBatchSize = 0;
        double trainStepsPerSecond = 0.0;
        double trainSamplesPerSecond = 0.0;
        int adaptiveTrainingBudgetMs = 0;
        int adaptiveBatchSize = 0;
        QString activityHint;
        int loadedGoldSamples = 0;
        int rankerPositiveSamples = 0;
        int rankerNegativeSamples = 0;
        int mindExtraLastSlot = 0;
        int mindExtraSession = 0;
        int mindLdpcSkippedLastSlot = 0;
        int mindLdpcSkippedSession = 0;
        int mindScoredLastSlot = 0;
        double mindAvgSuccessLastSlot = 0.0;
        int activeProfileSamples = 0;
        int activeProfilePositiveSamples = 0;
        int activeProfileNegativeSamples = 0;
        int activeProfileValidationCount = 0;
        double activeProfileRankerAccuracy = 0.0;
        double activeProfileBestRankerAccuracy = 0.0;
        bool activeProfileAssistReady = false;
        QString activeProfileLifecycleState;
        QString activeProfileReadinessReason;
    };

    explicit DeepDspController(QObject *parent = nullptr);
    ~DeepDspController() override;

    /**
     * @brief Stops the autonomous training timer/thread before application shutdown.
     *
     * Safe to call multiple times; the destructor calls it too.
     */
    void shutdown();

    Status status() const;
    QString checkpointPath() const;
    QString statsPath() const;
    QString goldDatasetPath() const;
    bool scoreNativeFtCandidate(const QString &mode,
                                const QVector<float> &candidateMagnitudes,
                                QVector<float> *predictedBits,
                                double *confidencePercent);
    bool scoreMsk144Candidate(const QVector<float> &candidateFeatures,
                              double *confidencePercent,
                              bool *mayPromote);

public slots:
    void setEnabled(bool enabled);
    void setAssistEnabled(bool enabled);
    void setAssistMode(const QString &mode);
    void setTrainingBudgetMs(int ms);
    void setActivityHint(const QString &hint);
    void setRuntimeMode(const QString &mode);
    /**
     * @brief Explicit live-critical guard for FT timing-sensitive sections.
     *
     * AutoTest uses the normal candidate-driven cooldown instead, so benchmarks
     * measure the same asynchronous deferral policy used in live operation.
     */
    void setDecodeCritical(bool active);
    void observeFtActivity(const QString &mode);
    void observeMsk144Activity();
    void submitNativeMsk144Sample(const QVector<float> &candidateFeatures,
                                  bool decodeSucceeded,
                                  const QString &message);
    void submitNativeFtSample(const QString &mode,
                              const QVector<float> &candidateMagnitudes,
                              const QVector<float> &targetBits,
                              const QString &message);
    void resetModel();
    void saveCheckpoint();
    void loadCheckpoint();
    void updateFtMindGainStats(int extraDecodes,
                               int ldpcSkipped,
                               int scoredCandidates,
                               double avgSuccessPercent);

signals:
    void statusChanged(const DeepDspController::Status &status);
    void logMessage(const QString &message);

private slots:
    void trainIdleSlice();

private:
    struct Sample
    {
        QVector<float> input;
        QVector<float> target;
        QString mode;
        QString label;
        bool nativeFt = false;
    };

    void appendSample(const Sample &sample);
    void updateValidation(const QVector<float> &prediction, const QVector<float> &target, const QString &mode);
    void emitStatus();
    void emitStatusThrottled();
    void rebuildNetwork();
    void saveStats();
    void loadStats();
    bool saveGoldDataset();
    bool loadGoldDataset();
    double trainOnSamples(const QVector<Sample> &batch);
    QVector<float> predict(const QVector<float> &input);
    int adaptiveTrainingBudgetMs(qint64 nowMs, int *batchCap) const;
    QString autonomousStateText(int budgetMs) const;

private:
    bool m_enabled = true;
    bool m_assistEnabled = false;
    QString m_assistMode = QStringLiteral("assisted"); // fixed operator mode: assist requested, readiness-gated
    bool m_decodeCritical = false;
    qint64 m_decodeCriticalCooldownUntilMs = 0; // legacy explicit guard, kept for future live-critical use
    qint64 m_neuralWorkDeferredUntilMs = 0;     // native candidate-driven cooldown after FT label bursts
    bool m_checkpointLoaded = false;
    QString m_activityHint = QStringLiteral("normal");
    qint64 m_lastRealtimeActivityMs = 0;
    int m_lastAdaptiveBudgetMs = 0;
    int m_lastAdaptiveBatchSize = 0;
    int m_trainingBudgetMs = 0; // legacy/manual value ignored; trainer budget is adaptive only
    int m_trainingRuns = 0;
    int m_ftSamples = 0;
    int m_ft8Samples = 0;
    int m_ft4Samples = 0;
    int m_msk144Samples = 0;
    int m_nativeFtSamples = 0;
    int m_manualSamples = 0;
    QString m_activeProfile = QStringLiteral("FT8");
    double m_lastLoss = 0.0;
    qint64 m_lastStatusEmitMs = 0;
    qint64 m_lastAutoCheckpointMs = 0;
    qint64 m_lastStatsSaveMs = 0;
    int m_lastStatsSavedSampleCount = 0;
    bool m_statsDirty = false;
    mutable Status m_cachedStatus;
    QTimer *m_trainTimer = nullptr;
    std::deque<Sample> m_samples;          // persistent FT gold replay buffer
    std::deque<bool> m_validationWindow;
    std::deque<double> m_bitAccuracyWindow;
    std::deque<QString> m_validationModeWindow;
    int m_replayCursor = 0;
    int m_loadedGoldSamples = 0;
    double m_loadedBitAccuracyPercent = 0.0;
    double m_loadedMessageAccuracyPercent = 0.0;
    bool m_goldDatasetDirty = false;
    double m_bestBitAccuracy = 0.0;
    int m_mindExtraLastSlot = 0;
    int m_mindExtraSession = 0;
    int m_mindLdpcSkippedLastSlot = 0;
    int m_mindLdpcSkippedSession = 0;
    int m_mindScoredLastSlot = 0;
    double m_mindAvgSuccessLastSlot = 0.0;
    int m_ftBatchSize = 128;
    int m_lastFtBatchSize = 0;
    qint64 m_trainedFtSamplesTotal = 0;
    qint64 m_lastTrainMetricMs = 0;
    int m_metricTrainingRuns = 0;
    qint64 m_metricTrainedFtSamplesTotal = 0;
    double m_trainStepsPerSecond = 0.0;
    double m_trainSamplesPerSecond = 0.0;
    mutable QMutex m_mutex;
    QThread m_trainingThread;
    bool m_shutdownComplete = false;
    DeepDspTinyNet *m_network = nullptr;
};

Q_DECLARE_METATYPE(DeepDspController::Status)

#endif // DEEPDSPCONTROLLER_H
