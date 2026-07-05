#include "DeepDspController.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QMetaObject>
#include <QStringList>
#include <QtGlobal>
#include <Eigen/Core>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace {

constexpr int kInputCount = 464;
constexpr int kOutputCount = 1;
constexpr int kMaxSamples = 20000;
constexpr int kMinReadyValidation = 300;
constexpr int kMinMsk144AssistSamples = 240;
constexpr int kMinMsk144ReadyValidation = 120;

QString normalizedDomain(QString mode)
{
    mode = mode.trimmed().toUpper();
    if (mode == QStringLiteral("FT4") || mode == QStringLiteral("FT8") ||
        mode == QStringLiteral("MSK144")) {
        return mode;
    }
    if (mode.contains(QStringLiteral("FT4"))) return QStringLiteral("FT4");
    if (mode.contains(QStringLiteral("FT8"))) return QStringLiteral("FT8");
    if (mode.contains(QStringLiteral("MSK144")) || mode.contains(QStringLiteral("MSK"))) return QStringLiteral("MSK144");
    return QStringLiteral("OTHER");
}

int domainIndex(const QString &mode)
{
    const QString d = normalizedDomain(mode);
    if (d == QStringLiteral("FT8")) return 0;
    if (d == QStringLiteral("FT4")) return 1;
    if (d == QStringLiteral("MSK144")) return 2;
    return -1;
}

QString normalizedAssistMode(QString mode)
{
    mode = mode.trimmed().toLower();
    if (mode == QStringLiteral("off") || mode == QStringLiteral("shadow") ||
        mode == QStringLiteral("assisted")) {
        return mode;
    }
    if (mode == QStringLiteral("training")) {
        return QStringLiteral("shadow");
    }
    if (mode == QStringLiteral("assist") || mode == QStringLiteral("on") ||
        mode == QStringLiteral("active")) {
        return QStringLiteral("assisted");
    }
    return QStringLiteral("shadow");
}


double estimatedExactFramePercent(double classifierAccuracyPercent)
{
    // Historical Status field retained for UI compatibility.  In the ranker
    // architecture it is the measured classifier success/fail accuracy, not a
    // theoretical 174-bit exact-frame estimate.
    return qBound(0.0, classifierAccuracyPercent, 100.0);
}

bool mindAssistReadyForFt(int validationCount, int replaySamples, double classifierAccuracy, double bestClassifierAccuracy)
{
    /*
     * MIND Ranker v1 readiness: the network is a binary candidate classifier,
     * not a decoder.  It may assist only when it has seen enough mixed
     * positive/negative candidate examples and the validation window shows that
     * it can distinguish CRC-valid candidates from CRC/LDPC drops.
     */
    return validationCount >= kMinReadyValidation &&
           replaySamples >= 768 &&
           classifierAccuracy >= 84.0 &&
           bestClassifierAccuracy >= 88.0;
}

QString mindAssistReadinessReason(int validationCount, int replaySamples, double classifierAccuracy, double bestClassifierAccuracy)
{
    if (validationCount < kMinReadyValidation) {
        return QStringLiteral("collecting ranker validation window");
    }
    if (replaySamples < 768) {
        return QStringLiteral("collecting positive/negative weak-signal candidates");
    }
    if (classifierAccuracy < 84.0 || bestClassifierAccuracy < 88.0) {
        return QStringLiteral("waiting for ranker classifier stability");
    }
    return QStringLiteral("candidate ranker assist-ready");
}


bool mindAssistReadyForMsk144(int mskSamples, int validationCount, double classifierAccuracy, double bestClassifierAccuracy)
{
    // Do not let a trained FT8/FT4 ranker look "ready" for MSK144 before it has
    // seen real MSK144 positive/negative ping/chunk candidates.
    return mskSamples >= kMinMsk144AssistSamples &&
           validationCount >= kMinMsk144ReadyValidation &&
           classifierAccuracy >= 70.0 &&
           bestClassifierAccuracy >= 80.0;
}

QString mindAssistReadinessReasonForMsk144(int mskSamples, int validationCount, double classifierAccuracy, double bestClassifierAccuracy)
{
    if (mskSamples < kMinMsk144AssistSamples) {
        return QStringLiteral("collecting real MSK144 ping/chunk samples");
    }
    if (validationCount < kMinMsk144ReadyValidation) {
        return QStringLiteral("collecting MSK144 validation window");
    }
    if (classifierAccuracy < 70.0 || bestClassifierAccuracy < 80.0) {
        return QStringLiteral("waiting for MSK144 ranker stability");
    }
    return QStringLiteral("MSK144 candidate ranker assist-ready");
}

QString madnnessBaseDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.trimmed().isEmpty()) {
        base = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("MIND"));
    } else {
        base = QDir(base).filePath(QStringLiteral("MIND"));
    }
    QDir().mkpath(base);
    return base;
}

QString defaultCheckpointPath()
{
    return QDir(madnnessBaseDir()).filePath(QStringLiteral("mind_ft_candidate_ranker_v1.model"));
}

QString defaultStatsPath()
{
    return QDir(madnnessBaseDir()).filePath(QStringLiteral("mind_stats.json"));
}

QString defaultGoldDatasetPath()
{
    return QDir(madnnessBaseDir()).filePath(QStringLiteral("mind_ft_ranker_samples_v1.dat"));
}

bool writeJsonAtomically(const QString &path, const QJsonObject &obj)
{
    const QString tmp = path + QStringLiteral(".tmp");
    QFile::remove(tmp);
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    f.flush();
    f.close();
    QFile::remove(path);
    if (!QFile::rename(tmp, path)) {
        QFile::remove(tmp);
        return false;
    }
    return true;
}

} // namespace

DeepDspController::DeepDspController(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<DeepDspController::Status>("DeepDspController::Status");
    rebuildNetwork();
    loadCheckpoint();
    // Always inspect the stats file too: if an older MIND v1 stats file is still
    // present and no v2 checkpoint exists yet, rename it out of the way so users
    // do not accidentally upload stale 77-bit/fingerprint statistics.
    loadStats();
    loadGoldDataset();

    m_trainTimer = new QTimer();
    m_trainTimer->setInterval(80);
    m_trainTimer->moveToThread(&m_trainingThread);
    connect(&m_trainingThread, &QThread::started, m_trainTimer, QOverload<>::of(&QTimer::start));
    connect(m_trainTimer, &QTimer::timeout, this, &DeepDspController::trainIdleSlice, Qt::DirectConnection);
    connect(&m_trainingThread, &QThread::finished, m_trainTimer, &QObject::deleteLater);
    m_trainingThread.setObjectName(QStringLiteral("MIND trainer"));
    m_trainingThread.start(QThread::LowPriority);

    emit logMessage(QStringLiteral("MIND autonomous trainer thread started at low priority; training budget is adaptive and not user-controlled."));
    emitStatus();
}

DeepDspController::~DeepDspController()
{
    shutdown();
    delete m_network;
}

void DeepDspController::shutdown()
{
    if (m_shutdownComplete) {
        return;
    }
    m_shutdownComplete = true;

    if (m_trainTimer != nullptr) {
        disconnect(m_trainTimer, nullptr, this, nullptr);
        if (m_trainTimer->thread() == QThread::currentThread()) {
            m_trainTimer->stop();
        } else {
            QMetaObject::invokeMethod(m_trainTimer, "stop", Qt::BlockingQueuedConnection);
        }
    }

    if (m_trainingThread.isRunning()) {
        m_trainingThread.requestInterruption();
        m_trainingThread.quit();
        if (!m_trainingThread.wait(3000)) {
            m_trainingThread.terminate();
            m_trainingThread.wait(1000);
        }
    }

    saveCheckpoint();
}

DeepDspController::Status DeepDspController::status() const
{
    QMutexLocker locker(&m_mutex);
    Status s;
    s.enabled = m_enabled;
    s.assistMode = normalizedAssistMode(m_assistMode);
    s.assistRequested = (s.assistMode == QStringLiteral("assisted"));
    s.assistEnabled = m_assistEnabled;
    s.ftSamples = m_ftSamples;
    s.ft8Samples = m_ft8Samples;
    s.ft4Samples = m_ft4Samples;
    s.msk144Samples = m_msk144Samples;
    s.nativeFtSamples = m_nativeFtSamples;
    s.manualSamples = m_manualSamples;
    s.replayBufferSamples = static_cast<int>(m_samples.size());
    int rankerPos = 0;
    int rankerNeg = 0;
    for (const Sample &sample : m_samples) {
        if (!sample.target.isEmpty() && sample.target.constFirst() >= 0.5f) ++rankerPos;
        else if (!sample.target.isEmpty()) ++rankerNeg;
    }
    s.rankerPositiveSamples = rankerPos;
    s.rankerNegativeSamples = rankerNeg;
    s.mindExtraLastSlot = m_mindExtraLastSlot;
    s.mindExtraSession = m_mindExtraSession;
    s.mindLdpcSkippedLastSlot = m_mindLdpcSkippedLastSlot;
    s.mindLdpcSkippedSession = m_mindLdpcSkippedSession;
    s.mindScoredLastSlot = m_mindScoredLastSlot;
    s.mindAvgSuccessLastSlot = m_mindAvgSuccessLastSlot;
    s.sampleCount = qMax(static_cast<int>(m_samples.size()), m_ftSamples);
    s.trainingRuns = m_trainingRuns;
    s.validationCount = static_cast<int>(m_validationWindow.size());
    int msgOk = 0;
    for (bool v : m_validationWindow) if (v) ++msgOk;
    s.messageAccuracy = s.validationCount > 0 ? 100.0 * static_cast<double>(msgOk) / static_cast<double>(s.validationCount) : 0.0;
    double bitSum = 0.0;
    for (double v : m_bitAccuracyWindow) bitSum += v;
    s.bitAccuracy = !m_bitAccuracyWindow.empty() ? bitSum / static_cast<double>(m_bitAccuracyWindow.size()) : 0.0;
    s.bestBitAccuracy = m_bestBitAccuracy;
    s.estimatedExactFrameAccuracy = estimatedExactFramePercent(s.bitAccuracy);

    auto samplesForProfile = [this](const QString &profile) -> int {
        const QString d = normalizedDomain(profile);
        if (d == QStringLiteral("FT8")) return m_ft8Samples;
        if (d == QStringLiteral("FT4")) return m_ft4Samples;
        if (d == QStringLiteral("MSK144")) return m_msk144Samples;
        return 0;
    };
    auto validationStatsForProfile = [this](const QString &profile, int *count, double *avg, double *best) {
        const QString d = normalizedDomain(profile);
        int n = 0;
        double sum = 0.0;
        double maxv = 0.0;
        auto modeIt = m_validationModeWindow.begin();
        auto accIt = m_bitAccuracyWindow.begin();
        for (; modeIt != m_validationModeWindow.end() && accIt != m_bitAccuracyWindow.end(); ++modeIt, ++accIt) {
            if (normalizedDomain(*modeIt) != d) continue;
            ++n;
            sum += *accIt;
            if (*accIt > maxv) maxv = *accIt;
        }
        if (count != nullptr) *count = n;
        if (avg != nullptr) *avg = (n > 0) ? (sum / static_cast<double>(n)) : 0.0;
        if (best != nullptr) *best = maxv;
    };

    const QString statusProfile = normalizedDomain(m_activeProfile);
    s.activeProfileSamples = samplesForProfile(statusProfile);
    validationStatsForProfile(statusProfile, &s.activeProfileValidationCount,
                              &s.activeProfileRankerAccuracy,
                              &s.activeProfileBestRankerAccuracy);

    const bool mskProfile = (statusProfile == QStringLiteral("MSK144"));
    if (mskProfile) {
        s.assistReady = mindAssistReadyForMsk144(s.activeProfileSamples,
                                                 s.activeProfileValidationCount,
                                                 s.activeProfileRankerAccuracy,
                                                 s.activeProfileBestRankerAccuracy);
        s.readinessReason = mindAssistReadinessReasonForMsk144(s.activeProfileSamples,
                                                               s.activeProfileValidationCount,
                                                               s.activeProfileRankerAccuracy,
                                                               s.activeProfileBestRankerAccuracy);
    } else {
        s.assistReady = mindAssistReadyForFt(s.validationCount, s.replayBufferSamples, s.bitAccuracy, s.bestBitAccuracy);
        s.readinessReason = mindAssistReadinessReason(s.validationCount, s.replayBufferSamples, s.bitAccuracy, s.bestBitAccuracy);
    }
    s.activeProfileAssistReady = s.assistReady;
    s.activeProfileReadinessReason = s.readinessReason;
    s.validationAccuracy = s.messageAccuracy;
    if (mskProfile) {
        const double sampleReadiness = qMin(1.0, static_cast<double>(s.activeProfileSamples) / static_cast<double>(kMinMsk144AssistSamples));
        const double valReadiness = qMin(1.0, static_cast<double>(s.activeProfileValidationCount) / static_cast<double>(kMinMsk144ReadyValidation));
        const double classifierProgress = qBound(0.0, (s.activeProfileRankerAccuracy - 50.0) / 20.0, 1.0);
        s.trainingCompletionPercent = qMin(100.0, 100.0 * sampleReadiness * valReadiness * classifierProgress);
    } else {
        const double sampleReadiness = qMin(1.0, static_cast<double>(s.validationCount) / static_cast<double>(kMinReadyValidation));
        const double ftGoldReadiness = qMin(1.0, static_cast<double>(s.replayBufferSamples) / 768.0);
        const double classifierProgress = qBound(0.0, (s.bitAccuracy - 50.0) / 34.0, 1.0);
        s.trainingCompletionPercent = qMin(100.0, 100.0 * sampleReadiness * ftGoldReadiness * classifierProgress);
    }
    s.lastLoss = m_lastLoss;
    s.ftActivity = m_network != nullptr ? m_network->lastActivity() : QVector<float>();
    s.neuralActivity = s.ftActivity;
    s.ready = s.assistReady;
    s.assistEnabled = s.assistReady && s.assistRequested;
    s.checkpointPath = checkpointPath();
    s.statsPath = statsPath();
    s.goldDatasetPath = goldDatasetPath();
    s.architectureText = (normalizedDomain(m_activeProfile) == QStringLiteral("MSK144"))
        ? QStringLiteral("MSK144 58×8 ping/chunk ranker")
        : QStringLiteral("FT 58×8 Conv2D ranker");
    s.activeProfile = m_activeProfile;
    s.eigenThreads = Eigen::nbThreads();
    s.ftBatchSize = m_lastFtBatchSize > 0 ? m_lastFtBatchSize : m_ftBatchSize;
    s.trainStepsPerSecond = m_trainStepsPerSecond;
    s.trainSamplesPerSecond = m_trainSamplesPerSecond;
    s.adaptiveTrainingBudgetMs = m_lastAdaptiveBudgetMs;
    s.adaptiveBatchSize = m_lastAdaptiveBatchSize;
    s.loadedGoldSamples = m_loadedGoldSamples;
    s.activityHint = m_activityHint;
    s.backendText = QStringLiteral("CPU low-priority MIND ranker (%1 Eigen thread%2)")
                        .arg(s.eigenThreads)
                        .arg(s.eigenThreads == 1 ? QString() : QStringLiteral("s"));
    QFileInfo cpInfo(checkpointPath());
    if (m_checkpointLoaded) {
        s.modelStateText = QStringLiteral("Model loaded");
    } else if (cpInfo.exists() && cpInfo.size() > 0) {
        s.modelStateText = QStringLiteral("Model file present, not loaded");
    } else {
        s.modelStateText = QStringLiteral("Model missing");
    }
    s.lastCheckpointText = cpInfo.exists() ? cpInfo.lastModified().toString(Qt::ISODate) : QStringLiteral("never");
    if (!s.enabled) {
        s.stateText = QStringLiteral("Disabled");
    } else if (s.assistMode == QStringLiteral("off")) {
        s.stateText = s.ready ? QStringLiteral("Ready · Assist off")
                              : autonomousStateText(s.adaptiveTrainingBudgetMs);
    } else if (s.assistEnabled) {
        s.stateText = QStringLiteral("MIND Assist · ranker ready");
    } else if (s.assistRequested) {
        s.stateText = QStringLiteral("Assist queued · %1").arg(s.readinessReason);
    } else if (s.ready) {
        s.stateText = QStringLiteral("Ranker ready · Training");
    } else {
        s.stateText = autonomousStateText(s.adaptiveTrainingBudgetMs);
    }
    return s;
}

QString DeepDspController::checkpointPath() const
{
    return defaultCheckpointPath();
}

QString DeepDspController::statsPath() const
{
    return defaultStatsPath();
}

QString DeepDspController::goldDatasetPath() const
{
    return defaultGoldDatasetPath();
}


void DeepDspController::updateFtMindGainStats(int extraDecodes,
                                              int ldpcSkipped,
                                              int scoredCandidates,
                                              double avgSuccessPercent)
{
    {
        QMutexLocker locker(&m_mutex);
        m_mindExtraLastSlot = qMax(0, extraDecodes);
        m_mindExtraSession += qMax(0, extraDecodes);
        m_mindLdpcSkippedLastSlot = qMax(0, ldpcSkipped);
        m_mindLdpcSkippedSession += qMax(0, ldpcSkipped);
        m_mindScoredLastSlot = qMax(0, scoredCandidates);
        m_mindAvgSuccessLastSlot = qBound(0.0, avgSuccessPercent, 100.0);
    }
    emitStatusThrottled();
}

void DeepDspController::setEnabled(bool enabled)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_enabled == enabled) return;
        m_enabled = enabled;
    }
    emitStatus();
}

void DeepDspController::setAssistEnabled(bool enabled)
{
    setAssistMode(enabled ? QStringLiteral("assisted") : QStringLiteral("shadow"));
}

void DeepDspController::setAssistMode(const QString &mode)
{
    QString clean = normalizedAssistMode(mode);
    bool queued = false;
    bool active = false;
    QString readinessReason;
    QString profile;
    {
        QMutexLocker locker(&m_mutex);
        if (m_assistMode == clean) return;
        m_assistMode = clean;
        profile = normalizedDomain(m_activeProfile);

        bool readyNow = false;
        if (profile == QStringLiteral("MSK144")) {
            int mskValidationCount = 0;
            double mskAccuracySum = 0.0;
            double mskBestAccuracy = 0.0;
            auto modeIt = m_validationModeWindow.begin();
            auto accIt = m_bitAccuracyWindow.begin();
            for (; modeIt != m_validationModeWindow.end() && accIt != m_bitAccuracyWindow.end(); ++modeIt, ++accIt) {
                if (normalizedDomain(*modeIt) != QStringLiteral("MSK144")) continue;
                ++mskValidationCount;
                mskAccuracySum += *accIt;
                if (*accIt > mskBestAccuracy) mskBestAccuracy = *accIt;
            }
            const double mskClassifierAccuracy = mskValidationCount > 0
                ? mskAccuracySum / static_cast<double>(mskValidationCount)
                : 0.0;
            readyNow = mindAssistReadyForMsk144(m_msk144Samples,
                                                mskValidationCount,
                                                mskClassifierAccuracy,
                                                mskBestAccuracy);
            readinessReason = mindAssistReadinessReasonForMsk144(m_msk144Samples,
                                                                  mskValidationCount,
                                                                  mskClassifierAccuracy,
                                                                  mskBestAccuracy);
        } else {
            double bitSum = 0.0;
            for (double v : m_bitAccuracyWindow) bitSum += v;
            const double bitAccuracy = !m_bitAccuracyWindow.empty()
                ? bitSum / static_cast<double>(m_bitAccuracyWindow.size())
                : 0.0;
            readyNow = mindAssistReadyForFt(static_cast<int>(m_validationWindow.size()),
                                            static_cast<int>(m_samples.size()),
                                            bitAccuracy,
                                            m_bestBitAccuracy);
            readinessReason = mindAssistReadinessReason(static_cast<int>(m_validationWindow.size()),
                                                         static_cast<int>(m_samples.size()),
                                                         bitAccuracy,
                                                         m_bestBitAccuracy);
        }

        m_assistEnabled = (m_assistMode == QStringLiteral("assisted")) && readyNow;
        queued = (m_assistMode == QStringLiteral("assisted")) && !readyNow;
        active = m_assistEnabled;
        m_statsDirty = true;
    }
    if (clean == QStringLiteral("off")) {
        emit logMessage(QStringLiteral("MIND Assist mode: Off. Native weak-signal decoders run without MIND ranking."));
    } else if (clean == QStringLiteral("shadow")) {
        emit logMessage(QStringLiteral("MIND Assist mode: Training. MIND learns/scores but does not alter decoding."));
    } else if (queued) {
        emit logMessage(QStringLiteral("MIND Assist mode: Assist queued for %1; %2.").arg(profile, readinessReason));
    } else if (active) {
        emit logMessage(QStringLiteral("MIND Assist mode: Assist active for %1.").arg(profile));
    }
    emitStatus();
}

void DeepDspController::setTrainingBudgetMs(int ms)
{
    Q_UNUSED(ms);
    // Legacy slot kept for signal/ABI compatibility.  The user-facing trainer
    // budget has been retired: MIND now computes its own low-priority idle
    // budget from application state, mode and FT timing-critical sections.
    emitStatusThrottled();
}

void DeepDspController::setActivityHint(const QString &hint)
{
    const QString clean = hint.trimmed().toLower();
    QString normalized = QStringLiteral("normal");
    if (clean == QStringLiteral("log") || clean == QStringLiteral("logbook") ||
        clean == QStringLiteral("settings") || clean == QStringLiteral("configuration") ||
        clean == QStringLiteral("idle_heavy")) {
        normalized = QStringLiteral("idle_heavy");
    } else if (clean == QStringLiteral("interactive") || clean == QStringLiteral("realtime") ||
               clean == QStringLiteral("busy")) {
        normalized = QStringLiteral("interactive");
    } else if (clean == QStringLiteral("normal") || clean == QStringLiteral("idle") || clean.isEmpty()) {
        normalized = QStringLiteral("normal");
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_activityHint == normalized) return;
        m_activityHint = normalized;
        changed = true;
    }
    if (changed) emitStatusThrottled();
}


void DeepDspController::setRuntimeMode(const QString &mode)
{
    const QString domain = normalizedDomain(mode);
    if (domain != QStringLiteral("FT8") && domain != QStringLiteral("FT4") &&
        domain != QStringLiteral("MSK144")) {
        return;
    }
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_activeProfile == domain) return;
        m_activeProfile = domain;
        changed = true;
    }
    if (changed) emitStatusThrottled();
}

void DeepDspController::setDecodeCritical(bool active)
{
    // Explicit critical guard retained for future live-slot/TX edge protection.
    // AutoTest does not use this path: benchmarks must see the normal native
    // candidate-driven deferral behavior rather than a test-only freeze.
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_decodeCritical == active) {
            if (!active) {
                m_decodeCriticalCooldownUntilMs = QDateTime::currentMSecsSinceEpoch() + 3000;
            }
            return;
        }
        m_decodeCritical = active;
        if (!active) {
            m_decodeCriticalCooldownUntilMs = QDateTime::currentMSecsSinceEpoch() + 3000;
        }
        changed = true;
    }
    if (changed) {
        if (active) {
            emit logMessage(QStringLiteral("MIND deferred: FT realtime decode is timing-critical; labels are queued only."));
        } else {
            emit logMessage(QStringLiteral("MIND cooldown: training will resume after the native FT critical window is idle."));
        }
        emitStatus();
    }
}

void DeepDspController::observeFtActivity(const QString &mode)
{
    const QString domain = normalizedDomain(mode);
    if (domain != QStringLiteral("FT8") && domain != QStringLiteral("FT4")) return;

    QMutexLocker locker(&m_mutex);
    if (!m_enabled || normalizedAssistMode(m_assistMode) == QStringLiteral("off")) return;
    m_activeProfile = domain;
    m_lastRealtimeActivityMs = QDateTime::currentMSecsSinceEpoch();
}

void DeepDspController::observeMsk144Activity()
{
    QMutexLocker locker(&m_mutex);
    if (!m_enabled || normalizedAssistMode(m_assistMode) == QStringLiteral("off")) return;
    m_activeProfile = QStringLiteral("MSK144");
    m_lastRealtimeActivityMs = QDateTime::currentMSecsSinceEpoch();
}

void DeepDspController::submitNativeMsk144Sample(const QVector<float> &candidateFeatures,
                                                 bool decodeSucceeded,
                                                 const QString &message)
{
    if (candidateFeatures.size() != kInputCount) return;

    {
        QMutexLocker locker(&m_mutex);
        if (!m_enabled || normalizedAssistMode(m_assistMode) == QStringLiteral("off")) return;

        Sample sample;
        sample.input = candidateFeatures;
        sample.target = QVector<float>{decodeSucceeded ? 1.0f : 0.0f};
        sample.mode = QStringLiteral("MSK144");
        sample.label = message.trimmed().toUpper();
        sample.nativeFt = true; // legacy replay flag: native weak-signal candidate sample

        appendSample(sample);
        m_activeProfile = QStringLiteral("MSK144");
        // MSK144 pings are latency-sensitive.  Queue labels now, but defer heavy
        // trainer work briefly so the period decoder can finish first.
        m_neuralWorkDeferredUntilMs = QDateTime::currentMSecsSinceEpoch() + 250;
        ++m_msk144Samples;
        ++m_nativeFtSamples;
        m_statsDirty = true;
        m_goldDatasetDirty = true;
        if (m_decodeCritical) return;
    }
    emitStatusThrottled();
}


void DeepDspController::submitNativeFtSample(const QString &mode,
                                             const QVector<float> &candidateMagnitudes,
                                             const QVector<float> &targetBits,
                                             const QString &message)
{
    const QString domain = normalizedDomain(mode);
    if (domain != QStringLiteral("FT8") && domain != QStringLiteral("FT4")) return;
    if (candidateMagnitudes.size() != kInputCount || targetBits.isEmpty()) return;

    {
        QMutexLocker locker(&m_mutex);
        if (!m_enabled || normalizedAssistMode(m_assistMode) == QStringLiteral("off")) return;

        QVector<float> binaryTarget;
        binaryTarget.reserve(kOutputCount);
        binaryTarget.append(targetBits.constFirst() >= 0.5f ? 1.0f : 0.0f);

        Sample sample;
        sample.input = candidateMagnitudes;
        sample.target = binaryTarget;
        sample.mode = domain;
        sample.label = message.trimmed().toUpper();
        sample.nativeFt = true;

        appendSample(sample);
        m_activeProfile = domain;
        // Native FT deferral: every candidate/label burst postpones heavy
        // trainer GEMM work.  This is not an AutoTest special case; it is the
        // same behavior desired during live FT slots where decoding must finish
        // before background learning resumes.
        m_neuralWorkDeferredUntilMs = QDateTime::currentMSecsSinceEpoch() + 250;
        ++m_ftSamples;
        if (domain == QStringLiteral("FT8")) ++m_ft8Samples;
        else if (domain == QStringLiteral("FT4")) ++m_ft4Samples;
        ++m_nativeFtSamples;
        m_statsDirty = true;
        m_goldDatasetDirty = true;
        if (m_decodeCritical) {
            // Explicit live-critical guard only.  AutoTest uses the native
            // candidate-driven cooldown above, so benchmarks are not artificially
            // frozen.
            return;
        }
    }
    emitStatusThrottled();
}


void DeepDspController::resetModel()
{
    {
        QMutexLocker locker(&m_mutex);
        rebuildNetwork();
        m_samples.clear();
        m_validationWindow.clear();
        m_bitAccuracyWindow.clear();
        m_trainingRuns = 0;
        m_ftSamples = 0;
        m_ft8Samples = 0;
        m_ft4Samples = 0;
        m_msk144Samples = 0;
        m_nativeFtSamples = 0;
        m_manualSamples = 0;
        m_lastLoss = 0.0;
        m_bestBitAccuracy = 0.0;
        m_replayCursor = 0;
        m_loadedGoldSamples = 0;
        QFile::remove(checkpointPath());
        QFile::remove(statsPath());
        QFile::remove(goldDatasetPath());
        m_statsDirty = false;
        m_goldDatasetDirty = false;
        m_lastStatsSaveMs = 0;
        m_lastStatsSavedSampleCount = 0;
    }
    emit logMessage(QStringLiteral("MIND ranker model and weak-signal candidate replay buffer reset."));
    emitStatus();
}


void DeepDspController::saveCheckpoint()
{
    bool modelOk = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_network == nullptr) return;
        modelOk = m_network->save(checkpointPath());
        saveStats();
        m_statsDirty = false;
        m_goldDatasetDirty = false;
        m_lastStatsSaveMs = QDateTime::currentMSecsSinceEpoch();
        m_lastStatsSavedSampleCount = m_nativeFtSamples + m_manualSamples;
    }
    if (modelOk) {
        emit logMessage(QStringLiteral("MIND ranker checkpoint and weak-signal candidate replay buffer saved: %1").arg(checkpointPath()));
    } else {
        emit logMessage(QStringLiteral("MIND checkpoint save failed."));
    }
    emitStatus();
}


void DeepDspController::loadCheckpoint()
{
    bool ftLoaded = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_network == nullptr) return;

        QFileInfo ftInfo(checkpointPath());
        if (ftInfo.exists() && ftInfo.size() > 0 && m_network->load(checkpointPath())) {
            m_checkpointLoaded = true;
            ftLoaded = true;
        } else {
            m_checkpointLoaded = false;
        }

        loadStats();
        loadGoldDataset();
    }
    if (ftLoaded) {
        emit logMessage(QStringLiteral("MIND weak-signal ranker checkpoint loaded."));
    } else {
        emit logMessage(QStringLiteral("MIND weak-signal ranker checkpoint missing; starting fresh."));
    }
    emitStatus();
}


void DeepDspController::saveStats()
{
    if (m_goldDatasetDirty) {
        saveGoldDataset();
    }

    int msgOk = 0;
    for (bool v : m_validationWindow) if (v) ++msgOk;
    const int validationCount = static_cast<int>(m_validationWindow.size());
    const double messageAccuracy = validationCount > 0
        ? 100.0 * static_cast<double>(msgOk) / static_cast<double>(validationCount)
        : 0.0;
    double bitSum = 0.0;
    for (double v : m_bitAccuracyWindow) bitSum += v;
    const double bitAccuracy = !m_bitAccuracyWindow.empty()
        ? bitSum / static_cast<double>(m_bitAccuracyWindow.size())
        : 0.0;
    const double sampleReadiness = qMin(1.0, static_cast<double>(validationCount) / static_cast<double>(kMinReadyValidation));
    double readiness = qMin(100.0, (0.75 * bitAccuracy + 0.25 * messageAccuracy) * sampleReadiness);
    QJsonObject obj;
    obj.insert(QStringLiteral("engine"), QStringLiteral("MIND"));
    obj.insert(QStringLiteral("version"), 12);
    obj.insert(QStringLiteral("architecture"), QStringLiteral("weak_signal_candidate_ranker_v2"));
    obj.insert(QStringLiteral("trainer_thread"), QStringLiteral("autonomous_low_priority_qthread"));
    obj.insert(QStringLiteral("math_backend"), QStringLiteral("LowPriority CPU weak-signal ranker / Eigen threads capped during training"));
    obj.insert(QStringLiteral("eigen_threads"), Eigen::nbThreads());
    obj.insert(QStringLiteral("ft_batch_size"), m_lastFtBatchSize > 0 ? m_lastFtBatchSize : m_ftBatchSize);
    obj.insert(QStringLiteral("train_steps_per_second"), m_trainStepsPerSecond);
    obj.insert(QStringLiteral("train_samples_per_second"), m_trainSamplesPerSecond);
    obj.insert(QStringLiteral("trainer_budget_ms"), m_lastAdaptiveBudgetMs);
    obj.insert(QStringLiteral("trainer_budget_mode"), QStringLiteral("adaptive_autonomous"));
    obj.insert(QStringLiteral("activity_hint"), m_activityHint);
    obj.insert(QStringLiteral("mind_assist_mode"), normalizedAssistMode(m_assistMode));
    obj.insert(QStringLiteral("gold_dataset_path"), goldDatasetPath());
    obj.insert(QStringLiteral("replay_buffer_samples"), static_cast<int>(m_samples.size()));
    obj.insert(QStringLiteral("samples"), static_cast<int>(m_samples.size()));
    obj.insert(QStringLiteral("ft_samples"), m_ftSamples);
    obj.insert(QStringLiteral("ft8_samples"), m_ft8Samples);
    obj.insert(QStringLiteral("ft4_samples"), m_ft4Samples);
    obj.insert(QStringLiteral("msk144_samples"), m_msk144Samples);
    obj.insert(QStringLiteral("active_profile"), m_activeProfile);
    obj.insert(QStringLiteral("native_ft_samples"), m_nativeFtSamples);
    obj.insert(QStringLiteral("manual_samples"), m_manualSamples);
    obj.insert(QStringLiteral("training_runs"), m_trainingRuns);
    obj.insert(QStringLiteral("last_loss"), m_lastLoss);
    obj.insert(QStringLiteral("saved_utc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    obj.insert(QStringLiteral("validation_count"), validationCount);
    obj.insert(QStringLiteral("ranker_accuracy_percent"), bitAccuracy);
    obj.insert(QStringLiteral("bit_accuracy_percent"), bitAccuracy); // legacy UI/stat key
    obj.insert(QStringLiteral("best_ranker_accuracy_percent"), m_bestBitAccuracy);
    obj.insert(QStringLiteral("best_bit_accuracy_percent"), m_bestBitAccuracy); // legacy UI/stat key
    int mskValidationCount = 0;
    double mskAccuracySum = 0.0;
    double mskBestAccuracy = 0.0;
    auto modeIt = m_validationModeWindow.begin();
    auto accIt = m_bitAccuracyWindow.begin();
    for (; modeIt != m_validationModeWindow.end() && accIt != m_bitAccuracyWindow.end(); ++modeIt, ++accIt) {
        if (normalizedDomain(*modeIt) != QStringLiteral("MSK144")) continue;
        ++mskValidationCount;
        mskAccuracySum += *accIt;
        if (*accIt > mskBestAccuracy) mskBestAccuracy = *accIt;
    }
    const double mskClassifierAccuracy = mskValidationCount > 0
        ? mskAccuracySum / static_cast<double>(mskValidationCount)
        : 0.0;
    const QString activeStatsProfile = normalizedDomain(m_activeProfile);
    const bool assistReady = (activeStatsProfile == QStringLiteral("MSK144"))
        ? mindAssistReadyForMsk144(m_msk144Samples, mskValidationCount, mskClassifierAccuracy, mskBestAccuracy)
        : mindAssistReadyForFt(validationCount, static_cast<int>(m_samples.size()), bitAccuracy, m_bestBitAccuracy);
    obj.insert(QStringLiteral("msk144_validation_count"), mskValidationCount);
    obj.insert(QStringLiteral("msk144_ranker_accuracy_percent"), mskClassifierAccuracy);
    obj.insert(QStringLiteral("msk144_best_ranker_accuracy_percent"), mskBestAccuracy);
    obj.insert(QStringLiteral("message_accuracy_percent"), messageAccuracy);
    obj.insert(QStringLiteral("exact_frame_accuracy_percent"), messageAccuracy);
    obj.insert(QStringLiteral("estimated_exact_frame_from_bit_percent"), estimatedExactFramePercent(bitAccuracy));
    obj.insert(QStringLiteral("candidate_success_probability_model"), true);
    obj.insert(QStringLiteral("validation_success_percent"), messageAccuracy);
    obj.insert(QStringLiteral("assist_ready"), assistReady);
    obj.insert(QStringLiteral("assist_readiness_reason"),
               activeStatsProfile == QStringLiteral("MSK144")
                   ? mindAssistReadinessReasonForMsk144(m_msk144Samples, mskValidationCount, mskClassifierAccuracy, mskBestAccuracy)
                   : mindAssistReadinessReason(validationCount, static_cast<int>(m_samples.size()), bitAccuracy, m_bestBitAccuracy));
    obj.insert(QStringLiteral("readiness_percent"), readiness);
    writeJsonAtomically(statsPath(), obj);
}


void DeepDspController::loadStats()
{
    QFile f(statsPath());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;
    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("engine")).toString() != QStringLiteral("MIND")) return;

    const int version = obj.value(QStringLiteral("version")).toInt();
    const QString arch = obj.value(QStringLiteral("architecture")).toString();
    const bool supportedStats = (version == 11 && arch == QStringLiteral("ft_candidate_ranker_v1")) ||
                                (version == 12 && arch == QStringLiteral("weak_signal_candidate_ranker_v2"));
    if (!supportedStats) {
        const QString legacyPath = statsPath() + QStringLiteral(".legacy_v%1").arg(version);
        QFile::remove(legacyPath);
        QFile::rename(statsPath(), legacyPath);
        emit logMessage(QStringLiteral("MIND ignored legacy stats file and moved it to: %1").arg(legacyPath));
        m_statsDirty = true;
        return;
    }

    m_ftSamples = obj.value(QStringLiteral("ft_samples")).toInt(m_ftSamples);
    m_ft8Samples = obj.value(QStringLiteral("ft8_samples")).toInt(m_ft8Samples);
    m_ft4Samples = obj.value(QStringLiteral("ft4_samples")).toInt(m_ft4Samples);
    m_msk144Samples = obj.value(QStringLiteral("msk144_samples")).toInt(m_msk144Samples);
    const QString loadedProfile = obj.value(QStringLiteral("active_profile")).toString();
    if (!loadedProfile.trimmed().isEmpty()) m_activeProfile = normalizedDomain(loadedProfile);
    m_assistMode = normalizedAssistMode(obj.value(QStringLiteral("mind_assist_mode")).toString(m_assistMode));
    m_assistEnabled = false;
    m_nativeFtSamples = obj.value(QStringLiteral("native_ft_samples")).toInt(m_nativeFtSamples);
    m_manualSamples = obj.value(QStringLiteral("manual_samples")).toInt(m_manualSamples);
    m_trainingRuns = obj.value(QStringLiteral("training_runs")).toInt(m_trainingRuns);
    m_lastLoss = obj.value(QStringLiteral("last_loss")).toDouble(m_lastLoss);
    m_bestBitAccuracy = obj.value(QStringLiteral("best_ranker_accuracy_percent"))
                            .toDouble(obj.value(QStringLiteral("best_bit_accuracy_percent")).toDouble(m_bestBitAccuracy));

    // Restore enough validation/classifier history from persisted stats to avoid
    // misleading cold-start UI and to preserve the ranker assist-ready gate
    // across restarts. The exact per-sample window is not serialized; this
    // compact reconstruction is only used for readiness/status.
    m_loadedBitAccuracyPercent = obj.value(QStringLiteral("ranker_accuracy_percent"))
                                 .toDouble(obj.value(QStringLiteral("bit_accuracy_percent")).toDouble(0.0));
    m_loadedMessageAccuracyPercent = obj.value(QStringLiteral("exact_frame_accuracy_percent"))
                                      .toDouble(obj.value(QStringLiteral("message_accuracy_percent"))
                                      .toDouble(obj.value(QStringLiteral("validation_success_percent")).toDouble(0.0)));
    const int loadedValidationCount = obj.value(QStringLiteral("validation_count")).toInt(0);
    if (m_loadedBitAccuracyPercent > 0.0) {
        m_bitAccuracyWindow.clear();
        m_bitAccuracyWindow.push_back(m_loadedBitAccuracyPercent);
    }
    if (loadedValidationCount > 0) {
        m_validationWindow.clear();
        const int cappedCount = qMin(loadedValidationCount, 1000);
        const int okCount = qBound(0, static_cast<int>(std::lround((m_loadedMessageAccuracyPercent / 100.0) * cappedCount)), cappedCount);
        m_validationModeWindow.clear();
        for (int i = 0; i < cappedCount; ++i) {
            m_validationWindow.push_back(i < okCount);
            m_validationModeWindow.push_back(QStringLiteral("FT8")); // legacy compact stats belong to the FT ranker, not MSK144
        }
    }

    m_lastStatsSavedSampleCount = m_nativeFtSamples + m_manualSamples;
    m_lastStatsSaveMs = QDateTime::currentMSecsSinceEpoch();
}

bool DeepDspController::saveGoldDataset()
{
    const QString path = goldDatasetPath();
    const QFileInfo info(path);
    if (!info.dir().exists() && !QDir().mkpath(info.dir().absolutePath())) return false;

    const QString tmpPath = path + QStringLiteral(".tmp");
    QFile::remove(tmpPath);
    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

    QVector<Sample> nativeSamples;
    nativeSamples.reserve(static_cast<int>(m_samples.size()));
    for (const Sample &s : m_samples) {
        if (s.nativeFt && s.input.size() == kInputCount && s.target.size() == kOutputCount) {
            nativeSamples.append(s);
        }
    }

    QDataStream ds(&f);
    ds.setVersion(QDataStream::Qt_5_12);
    ds << QStringLiteral("MM_MIND_WEAK_SIGNAL_RANKER_V2") << static_cast<qint32>(nativeSamples.size());
    for (const Sample &s : nativeSamples) {
        ds << s.mode << s.label << s.input << s.target;
    }
    if (ds.status() != QDataStream::Ok) {
        f.close();
        QFile::remove(tmpPath);
        return false;
    }
    f.flush();
    f.close();
    QFile::remove(path);
    if (!QFile::rename(tmpPath, path)) {
        QFile::remove(tmpPath);
        return false;
    }
    m_loadedGoldSamples = nativeSamples.size();
    m_goldDatasetDirty = false;
    return true;
}

bool DeepDspController::loadGoldDataset()
{
    QFile f(goldDatasetPath());
    if (!f.open(QIODevice::ReadOnly)) return false;

    QDataStream ds(&f);
    ds.setVersion(QDataStream::Qt_5_12);
    QString magic;
    qint32 count = 0;
    ds >> magic >> count;
    if ((magic != QStringLiteral("MM_MIND_FT_RANKER_V1") &&
         magic != QStringLiteral("MM_MIND_WEAK_SIGNAL_RANKER_V2")) || count < 0) return false;

    std::deque<Sample> loaded;
    int loadedFt8 = 0;
    int loadedFt4 = 0;
    int loadedMsk144 = 0;
    const int maxToLoad = qMin(static_cast<int>(count), kMaxSamples);
    for (int i = 0; i < count; ++i) {
        QString mode;
        QString label;
        QVector<float> input;
        QVector<float> target;
        ds >> mode >> label >> input >> target;
        if (ds.status() != QDataStream::Ok) return false;
        if (i < count - maxToLoad) continue; // keep newest records when file is larger than buffer
        const QString domain = normalizedDomain(mode);
        if ((domain == QStringLiteral("FT8") || domain == QStringLiteral("FT4") ||
             domain == QStringLiteral("MSK144")) &&
            input.size() == kInputCount && target.size() == kOutputCount) {
            Sample s;
            s.mode = domain;
            s.label = label;
            s.input = input;
            s.target = target;
            s.nativeFt = true;
            loaded.push_back(s);
            if (domain == QStringLiteral("FT8")) ++loadedFt8;
            else if (domain == QStringLiteral("FT4")) ++loadedFt4;
            else if (domain == QStringLiteral("MSK144")) ++loadedMsk144;
        }
    }

    if (loaded.empty()) return false;
    m_samples = loaded;
    m_nativeFtSamples = static_cast<int>(m_samples.size());
    m_ft8Samples = loadedFt8;
    m_ft4Samples = loadedFt4;
    m_msk144Samples = loadedMsk144;
    m_ftSamples = loadedFt8 + loadedFt4;
    m_loadedGoldSamples = m_nativeFtSamples;
    m_replayCursor = 0;
    m_goldDatasetDirty = false;
    m_statsDirty = true;
    return true;
}


QString DeepDspController::autonomousStateText(int budgetMs) const
{
    if (m_decodeCritical) {
        return QStringLiteral("Autonomous · FT protected");
    }
    const QString profile = normalizedDomain(m_activeProfile);
    if (profile == QStringLiteral("FT8") || profile == QStringLiteral("FT4")) {
        return budgetMs > 0 ? QStringLiteral("Autonomous · FT idle training")
                            : QStringLiteral("Autonomous · FT idle");
    }
    if (profile == QStringLiteral("MSK144")) {
        return budgetMs > 0 ? QStringLiteral("Autonomous · MSK144 idle training")
                            : QStringLiteral("Autonomous · MSK144 idle");
    }
    return budgetMs > 0 ? QStringLiteral("Autonomous training")
                        : QStringLiteral("Autonomous · waiting for idle time");
}

int DeepDspController::adaptiveTrainingBudgetMs(qint64 nowMs, int *batchCap) const
{
    int cap = 0;
    int budget = 0;

    if (!m_enabled || normalizedAssistMode(m_assistMode) == QStringLiteral("off") ||
        m_decodeCritical || nowMs < m_decodeCriticalCooldownUntilMs ||
        m_network == nullptr) {
        if (batchCap != nullptr) *batchCap = 0;
        return 0;
    }

    const bool recentRealtime = (m_lastRealtimeActivityMs > 0) &&
                                (nowMs - m_lastRealtimeActivityMs < 1500);

    if (m_activityHint == QStringLiteral("idle_heavy")) {
        budget = 32;
        cap = m_ftBatchSize;
    } else if (m_activityHint == QStringLiteral("interactive")) {
        budget = 0;
        cap = 0;
    } else if (nowMs < m_neuralWorkDeferredUntilMs) {
        // Keep only a very small post-candidate quiet window.  Older builds
        // postponed training for seconds after every FT candidate burst, so the
        // trainer appeared to run only while transmitting.
        budget = 2;
        cap = 8;
    } else if (recentRealtime) {
        // Continuous low-priority learning during RX: small enough to protect
        // FT timing, but not frozen until the operator transmits.
        budget = 4;
        cap = 16;
    } else {
        budget = 12;
        cap = 64;
    }

    if (batchCap != nullptr) *batchCap = qBound(0, cap, m_ftBatchSize);
    return qBound(0, budget, 40);
}

void DeepDspController::trainIdleSlice()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 sliceStartMs = nowMs;
    bool shouldEmit = false;
    bool checkpointSaved = false;
    int ftBatches = 0;

    {
        QMutexLocker locker(&m_mutex);

        if (!m_enabled || normalizedAssistMode(m_assistMode) == QStringLiteral("off") ||
            m_decodeCritical || nowMs < m_decodeCriticalCooldownUntilMs ||
            m_network == nullptr) {
            const bool changed = (m_lastAdaptiveBudgetMs != 0 || m_lastAdaptiveBatchSize != 0);
            m_lastAdaptiveBudgetMs = 0;
            m_lastAdaptiveBatchSize = 0;
            // Latency-first: while explicit FT critical guard or native
            // candidate-driven cooldown is active, do not wake the GUI every
            // trainer tick.
            shouldEmit = changed;
        } else {
            const int persistedSampleCount = m_nativeFtSamples + m_manualSamples;
            const bool enoughStatsTime = (m_lastStatsSaveMs == 0) || (nowMs - m_lastStatsSaveMs >= 15000);
            const bool enoughNewSamples = (persistedSampleCount - m_lastStatsSavedSampleCount) >= 32;

            int adaptiveBatchCap = 0;
            const int budgetMs = adaptiveTrainingBudgetMs(nowMs, &adaptiveBatchCap);
            m_lastAdaptiveBudgetMs = budgetMs;
            m_lastAdaptiveBatchSize = adaptiveBatchCap;
            const int savedEigenThreads = Eigen::nbThreads();
            Eigen::setNbThreads(1);
            qint64 elapsedMs = 0;
            while (budgetMs > 0 && elapsedMs <= budgetMs) {
                bool trainedSomething = false;

                if (m_samples.size() >= 8) {
                    // Replay buffer: keep training on the persistent FT gold dataset,
                    // not only on samples just collected during the current run.
                    QVector<Sample> batch;
                    const int take = qMin(qMax(1, adaptiveBatchCap), static_cast<int>(m_samples.size()));
                    batch.reserve(take);
                    for (int i = 0; i < take; ++i) {
                        const int size = static_cast<int>(m_samples.size());
                        const int pos = (m_replayCursor + i * 37 + m_trainingRuns * 11) % size;
                        batch.append(m_samples[static_cast<size_t>(pos)]);
                    }
                    m_replayCursor = (m_replayCursor + take * 17 + 1) % static_cast<int>(m_samples.size());
                    m_lastLoss = trainOnSamples(batch);
                    m_lastFtBatchSize = take;
                    ++m_trainingRuns;
                    m_trainedFtSamplesTotal += take;
                    ++ftBatches;
                    trainedSomething = true;
                    m_statsDirty = true;
                }

                if (!trainedSomething) break;
                elapsedMs = QDateTime::currentMSecsSinceEpoch() - sliceStartMs;
            }

            if (savedEigenThreads > 0) {
                Eigen::setNbThreads(savedEigenThreads);
            }

            const qint64 metricNowMs = QDateTime::currentMSecsSinceEpoch();
            if (m_lastTrainMetricMs == 0) {
                m_lastTrainMetricMs = metricNowMs;
                m_metricTrainingRuns = m_trainingRuns;
                m_metricTrainedFtSamplesTotal = m_trainedFtSamplesTotal;
            } else if (metricNowMs - m_lastTrainMetricMs >= 2000) {
                const double dt = static_cast<double>(metricNowMs - m_lastTrainMetricMs) / 1000.0;
                if (dt > 0.0) {
                    m_trainStepsPerSecond = static_cast<double>(m_trainingRuns - m_metricTrainingRuns) / dt;
                    m_trainSamplesPerSecond = static_cast<double>(m_trainedFtSamplesTotal - m_metricTrainedFtSamplesTotal) / dt;
                }
                m_lastTrainMetricMs = metricNowMs;
                m_metricTrainingRuns = m_trainingRuns;
                m_metricTrainedFtSamplesTotal = m_trainedFtSamplesTotal;
            }

            // Save JSON stats and the FT gold dataset soon after samples arrive.
            // The deterministic decoder path never does disk I/O; this dedicated
            // trainer thread owns the persistence work.
            if (m_statsDirty && (enoughStatsTime || enoughNewSamples || m_goldDatasetDirty)) {
                saveStats();
                m_statsDirty = false;
                m_lastStatsSaveMs = QDateTime::currentMSecsSinceEpoch();
                m_lastStatsSavedSampleCount = persistedSampleCount;
            }

            if (m_lastAutoCheckpointMs == 0) {
                m_lastAutoCheckpointMs = QDateTime::currentMSecsSinceEpoch();
            }
            if ((m_trainingRuns > 0) &&
                QDateTime::currentMSecsSinceEpoch() - m_lastAutoCheckpointMs >= 5 * 60 * 1000) {
                if (m_network != nullptr) m_network->save(checkpointPath());
                saveStats();
                m_statsDirty = false;
                m_goldDatasetDirty = false;
                m_lastAutoCheckpointMs = QDateTime::currentMSecsSinceEpoch();
                checkpointSaved = true;
            }
            shouldEmit = (ftBatches > 0 || m_statsDirty);
        }
    }

    if (checkpointSaved) {
        emit logMessage(QStringLiteral("MIND auto-checkpoint saved by dedicated trainer thread."));
    }
    if (shouldEmit) emitStatusThrottled();
}


void DeepDspController::appendSample(const Sample &sample)
{
    if (sample.input.size() != kInputCount || sample.target.size() != kOutputCount) return;
    m_samples.push_back(sample);

    // Keep the replay buffer useful for a binary classifier.  The decoder emits
    // many more failures than CRC-valid decodes; retain a forced 30-40% failed
    // candidate quota without allowing negatives to drown positives.
    auto countNegatives = [this]() {
        int neg = 0;
        for (const Sample &s : m_samples) {
            if (!s.target.isEmpty() && s.target.constFirst() < 0.5f) ++neg;
        }
        return neg;
    };

    while (m_samples.size() > kMaxSamples) m_samples.pop_front();
    while (m_samples.size() >= 100) {
        const int total = static_cast<int>(m_samples.size());
        const int neg = countNegatives();
        if (neg * 100 <= total * 42) break;
        auto it = std::find_if(m_samples.begin(), m_samples.end(), [](const Sample &s) {
            return !s.target.isEmpty() && s.target.constFirst() < 0.5f;
        });
        if (it == m_samples.end()) break;
        m_samples.erase(it);
    }
}

void DeepDspController::updateValidation(const QVector<float> &prediction, const QVector<float> &target, const QString &mode)
{
    if (prediction.size() != target.size()) return;
    int bitOk = 0;
    for (int i = 0; i < target.size(); ++i) {
        const bool p = prediction[i] >= 0.5f;
        const bool t = target[i] >= 0.5f;
        if (p == t) ++bitOk;
    }
    const double bitAccuracy = target.isEmpty() ? 0.0 : 100.0 * static_cast<double>(bitOk) / static_cast<double>(target.size());
    const bool exact = (bitOk == target.size());
    if (bitAccuracy > m_bestBitAccuracy) m_bestBitAccuracy = bitAccuracy;
    m_bitAccuracyWindow.push_back(bitAccuracy);
    m_validationWindow.push_back(exact);
    m_validationModeWindow.push_back(normalizedDomain(mode));
    while (m_bitAccuracyWindow.size() > 1000) m_bitAccuracyWindow.pop_front();
    while (m_validationWindow.size() > 1000) m_validationWindow.pop_front();
    while (m_validationModeWindow.size() > 1000) m_validationModeWindow.pop_front();
}

void DeepDspController::emitStatus()
{
    {
        QMutexLocker locker(&m_mutex);
        m_lastStatusEmitMs = QDateTime::currentMSecsSinceEpoch();
    }
    emit statusChanged(status());
}

void DeepDspController::emitStatusThrottled()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    bool due = false;
    {
        QMutexLocker locker(&m_mutex);
        due = (nowMs - m_lastStatusEmitMs >= 250);
    }
    if (due) {
        emitStatus();
    }
}

void DeepDspController::rebuildNetwork()
{
    delete m_network;
    m_network = new DeepDspTinyNet();
    m_network->reset();
}


double DeepDspController::trainOnSamples(const QVector<Sample> &batch)
{
    if (batch.isEmpty() || m_network == nullptr) return 0.0;
    QVector<QVector<float>> inputs;
    QVector<QVector<float>> targets;
    inputs.reserve(batch.size());
    targets.reserve(batch.size());
    for (const Sample &s : batch) {
        if (s.input.size() == kInputCount && s.target.size() == kOutputCount) {
            inputs.append(s.input);
            targets.append(s.target);
        }
    }
    if (inputs.isEmpty()) return 0.0;

    // Validation is computed from idle slices, not from the decoder callback.
    const QVector<float> prediction = m_network->predict(inputs.constFirst());
    if (prediction.size() == targets.constFirst().size()) {
        updateValidation(prediction, targets.constFirst(), batch.constFirst().mode);
    }
    return m_network->trainBatch(inputs, targets, 0.003f);
}


QVector<float> DeepDspController::predict(const QVector<float> &input)
{
    if (m_network == nullptr || input.size() != kInputCount) return {};
    return m_network->predict(input);
}

bool DeepDspController::scoreNativeFtCandidate(const QVector<float> &candidateMagnitudes,
                                               QVector<float> *predictedBits,
                                               double *confidencePercent)
{
    if (predictedBits != nullptr) {
        predictedBits->clear();
    }
    if (confidencePercent != nullptr) {
        *confidencePercent = 0.0;
    }
    if (candidateMagnitudes.size() != kInputCount) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    const QString mode = normalizedAssistMode(m_assistMode);
    if (!m_enabled || mode == QStringLiteral("off") || m_network == nullptr) {
        return false;
    }

    double accSum = 0.0;
    for (double v : m_bitAccuracyWindow) accSum += v;
    const double classifierAccuracy = !m_bitAccuracyWindow.empty()
        ? accSum / static_cast<double>(m_bitAccuracyWindow.size())
        : 0.0;
    const bool readyNow = mindAssistReadyForFt(static_cast<int>(m_validationWindow.size()),
                                               static_cast<int>(m_samples.size()),
                                               classifierAccuracy,
                                               m_bestBitAccuracy);
    m_assistEnabled = (mode == QStringLiteral("assisted")) && readyNow;

    // 0.5.6: scoring is useful even before the ranker is allowed to prune.
    // Training and not-yet-ready Active still run inference for telemetry and
    // deep-candidate ordering; the returned mayPrune flag below remains false
    // until the assist-ready gate is reached.
    QVector<float> prediction = m_network->predict(candidateMagnitudes);
    if (prediction.size() != kOutputCount) {
        return false;
    }

    const double probability = qBound(0.0, static_cast<double>(prediction.constFirst()), 1.0);
    if (predictedBits != nullptr) {
        QVector<float> out;
        out.reserve(2);
        out.append(static_cast<float>(probability));
        out.append((mode == QStringLiteral("assisted") && readyNow) ? 1.0f : 0.0f);
        *predictedBits = out; // legacy argument now carries [probability, mayPrune]
    }
    if (confidencePercent != nullptr) {
        *confidencePercent = 100.0 * probability;
    }
    return true;
}

bool DeepDspController::scoreMsk144Candidate(const QVector<float> &candidateFeatures,
                                             double *confidencePercent,
                                             bool *mayPromote)
{
    if (confidencePercent != nullptr) *confidencePercent = 0.0;
    if (mayPromote != nullptr) *mayPromote = false;
    if (candidateFeatures.size() != kInputCount) return false;

    QMutexLocker locker(&m_mutex);
    const QString mode = normalizedAssistMode(m_assistMode);
    if (!m_enabled || mode == QStringLiteral("off") || m_network == nullptr) {
        return false;
    }

    int mskValidationCount = 0;
    double mskAccuracySum = 0.0;
    double mskBestAccuracy = 0.0;
    auto modeIt = m_validationModeWindow.begin();
    auto accIt = m_bitAccuracyWindow.begin();
    for (; modeIt != m_validationModeWindow.end() && accIt != m_bitAccuracyWindow.end(); ++modeIt, ++accIt) {
        if (normalizedDomain(*modeIt) != QStringLiteral("MSK144")) continue;
        ++mskValidationCount;
        mskAccuracySum += *accIt;
        if (*accIt > mskBestAccuracy) mskBestAccuracy = *accIt;
    }
    const double mskClassifierAccuracy = mskValidationCount > 0
        ? mskAccuracySum / static_cast<double>(mskValidationCount)
        : 0.0;
    const bool readyNow = mindAssistReadyForMsk144(m_msk144Samples,
                                                   mskValidationCount,
                                                   mskClassifierAccuracy,
                                                   mskBestAccuracy);
    m_assistEnabled = (mode == QStringLiteral("assisted")) && readyNow;

    const QVector<float> prediction = m_network->predict(candidateFeatures);
    if (prediction.size() != kOutputCount) {
        return false;
    }
    const double probability = qBound(0.0, static_cast<double>(prediction.constFirst()), 1.0);
    if (confidencePercent != nullptr) *confidencePercent = 100.0 * probability;
    if (mayPromote != nullptr) *mayPromote = (mode == QStringLiteral("assisted") && readyNow);
    m_activeProfile = QStringLiteral("MSK144");
    return true;
}



