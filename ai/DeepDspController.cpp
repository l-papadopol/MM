#include "DeepDspController.h"

#include <QCoreApplication>
#include <QCryptographicHash>
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

QString normalizedDomain(QString mode)
{
    mode = mode.trimmed().toUpper();
    if (mode == QStringLiteral("FT4") || mode == QStringLiteral("FT8") ||
        mode == QStringLiteral("RTTY") || mode == QStringLiteral("CW")) {
        return mode;
    }
    if (mode.contains(QStringLiteral("FT4"))) return QStringLiteral("FT4");
    if (mode.contains(QStringLiteral("FT8"))) return QStringLiteral("FT8");
    if (mode.contains(QStringLiteral("RTTY"))) return QStringLiteral("RTTY");
    if (mode.contains(QStringLiteral("CW")) || mode.contains(QStringLiteral("MORSE"))) return QStringLiteral("CW");
    return QStringLiteral("OTHER");
}

int domainIndex(const QString &mode)
{
    const QString d = normalizedDomain(mode);
    if (d == QStringLiteral("FT8")) return 0;
    if (d == QStringLiteral("FT4")) return 1;
    if (d == QStringLiteral("RTTY")) return 2;
    if (d == QStringLiteral("CW")) return 3;
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
        return QStringLiteral("collecting positive/negative FT candidates");
    }
    if (classifierAccuracy < 84.0 || bestClassifierAccuracy < 88.0) {
        return QStringLiteral("waiting for ranker classifier stability");
    }
    return QStringLiteral("candidate ranker assist-ready");
}

bool mindProfileReady(int trainingRuns, int validationSamples, double accuracy)
{
    /* CW/RTTY MIND is not a text generator.  It assists low-level decisions.
     * For CW Active, the user explicitly wants heavy human-fist recovery, so the
     * profile must become usable quickly after the synthetic bootcamp and the
     * first real-event validation decisions.  Conservative gates remain in the
     * decoder itself: only event/timing decisions are affected.
     */
    return trainingRuns >= 1 && validationSamples >= 2 && accuracy >= 25.0;
}

QString mindProfileReason(const QString &profile, int trainingRuns, int validationSamples, double accuracy)
{
    if (trainingRuns < 1) {
        return QStringLiteral("%1 waiting for first training batch").arg(profile);
    }
    if (validationSamples < 2) {
        return QStringLiteral("%1 collecting first validation decisions").arg(profile);
    }
    if (accuracy < 25.0) {
        return QStringLiteral("%1 waiting for usable event confidence").arg(profile);
    }
    return QStringLiteral("%1 low-level assist-ready").arg(profile);
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

QString defaultCwCheckpointPath()
{
    return QDir(madnnessBaseDir()).filePath(QStringLiteral("mind_cw_symbols_eigen_v1.model"));
}

QString defaultRttyCheckpointPath()
{
    return QDir(madnnessBaseDir()).filePath(QStringLiteral("mind_rtty_slicer_eigen_v1.model"));
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

    // Multi-mode foundation: seed the CW event profile with synthetic dit/dah/gap
    // examples so CW has a useful shadow model without exposing manual bootcamp
    // buttons.  Training still runs only in the autonomous low-priority idle path.
    if (m_cwBootcampSamples < 2400 && m_cwNetwork != nullptr) {
        const QVector<ProfileSample> cwSeed = generateCwBootcampSamples(2400 - m_cwBootcampSamples);
        for (const ProfileSample &sample : cwSeed) {
            m_cwSamplesQueue.push_back(sample);
        }
        m_cwSamples += cwSeed.size();
        m_cwBootcampSamples += cwSeed.size();
        m_statsDirty = true;
    }

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
    if (m_trainTimer != nullptr) {
        QMetaObject::invokeMethod(m_trainTimer, "stop", Qt::BlockingQueuedConnection);
    }
    m_trainingThread.quit();
    m_trainingThread.wait(3000);
    saveCheckpoint();
    delete m_network;
    delete m_cwNetwork;
    delete m_rttyNetwork;
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
    s.rttySamples = m_rttySamples;
    s.cwSamples = m_cwSamples;
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
    s.sampleCount = qMax(static_cast<int>(m_samples.size()), m_ftSamples + m_rttySamples + m_cwSamples);
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
    s.assistReady = mindAssistReadyForFt(s.validationCount, s.replayBufferSamples, s.bitAccuracy, s.bestBitAccuracy);
    s.readinessReason = mindAssistReadinessReason(s.validationCount, s.replayBufferSamples, s.bitAccuracy, s.bestBitAccuracy);
    double cwSum = 0.0;
    for (double v : m_cwAccuracyWindow) cwSum += v;
    s.cwAccuracy = !m_cwAccuracyWindow.empty() ? cwSum / static_cast<double>(m_cwAccuracyWindow.size()) : 0.0;
    double rttySum = 0.0;
    for (double v : m_rttyAccuracyWindow) rttySum += v;
    s.rttyAccuracy = !m_rttyAccuracyWindow.empty() ? rttySum / static_cast<double>(m_rttyAccuracyWindow.size()) : 0.0;
    s.cwTrainingRuns = m_cwTrainingRuns;
    s.rttyTrainingRuns = m_rttyTrainingRuns;
    s.cwAssistReady = mindProfileReady(m_cwTrainingRuns, static_cast<int>(m_cwAccuracyWindow.size()), s.cwAccuracy);
    s.rttyAssistReady = mindProfileReady(m_rttyTrainingRuns, static_cast<int>(m_rttyAccuracyWindow.size()), s.rttyAccuracy);
    s.cwAssistReason = mindProfileReason(QStringLiteral("CW"), m_cwTrainingRuns, static_cast<int>(m_cwAccuracyWindow.size()), s.cwAccuracy);
    s.rttyAssistReason = mindProfileReason(QStringLiteral("RTTY"), m_rttyTrainingRuns, static_cast<int>(m_rttyAccuracyWindow.size()), s.rttyAccuracy);
    s.validationAccuracy = s.messageAccuracy;
    const double sampleReadiness = qMin(1.0, static_cast<double>(s.validationCount) / static_cast<double>(kMinReadyValidation));
    const double ftGoldReadiness = qMin(1.0, static_cast<double>(s.replayBufferSamples) / 768.0);
    const double classifierProgress = qBound(0.0, (s.bitAccuracy - 50.0) / 34.0, 1.0);
    s.trainingCompletionPercent = qMin(100.0, 100.0 * sampleReadiness * ftGoldReadiness * classifierProgress);
    if (m_activeProfile == QStringLiteral("CW")) {
        const double cwReadiness = qMin(1.0, static_cast<double>(m_cwBootcampSamples) / 5000.0);
        s.trainingCompletionPercent = qMin(100.0, s.cwAccuracy * cwReadiness);
    } else if (m_activeProfile == QStringLiteral("RTTY")) {
        const double rttyReadiness = qMin(1.0, static_cast<double>(m_rttySamples) / 2000.0);
        s.trainingCompletionPercent = qMin(100.0, s.rttyAccuracy * rttyReadiness);
    }
    if (m_activeProfile == QStringLiteral("CW")) s.lastLoss = m_cwLastLoss;
    else if (m_activeProfile == QStringLiteral("RTTY")) s.lastLoss = m_rttyLastLoss;
    else s.lastLoss = m_lastLoss;
    s.ftActivity = m_network != nullptr ? m_network->lastActivity() : QVector<float>();
    s.cwActivity = m_cwNetwork != nullptr ? m_cwNetwork->lastActivity() : QVector<float>();
    s.rttyActivity = m_rttyNetwork != nullptr ? m_rttyNetwork->lastActivity() : QVector<float>();
    if (m_activeProfile == QStringLiteral("CW") && !s.cwActivity.isEmpty()) s.neuralActivity = s.cwActivity;
    else if (m_activeProfile == QStringLiteral("RTTY") && !s.rttyActivity.isEmpty()) s.neuralActivity = s.rttyActivity;
    else s.neuralActivity = s.ftActivity;
    s.ready = s.assistReady;
    s.assistEnabled = s.assistReady && s.assistRequested;
    s.checkpointPath = checkpointPath();
    s.statsPath = statsPath();
    s.goldDatasetPath = goldDatasetPath();
    s.architectureText = QStringLiteral("FT 58×8 Conv2D ranker · RTTY 96→64→32→8 soft slicer · CW 256→96→48→6 event model");
    s.activeProfile = m_activeProfile;
    s.eigenThreads = Eigen::nbThreads();
    s.ftBatchSize = m_lastFtBatchSize > 0 ? m_lastFtBatchSize : m_ftBatchSize;
    s.trainStepsPerSecond = m_trainStepsPerSecond;
    s.trainSamplesPerSecond = m_trainSamplesPerSecond;
    s.adaptiveTrainingBudgetMs = m_lastAdaptiveBudgetMs;
    s.adaptiveBatchSize = m_lastAdaptiveBatchSize;
    s.loadedGoldSamples = m_loadedGoldSamples;
    s.activityHint = m_activityHint;
    s.backendText = QStringLiteral("CPU low-priority multi-mode MIND (%1 Eigen thread%2)")
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
        s.stateText = QStringLiteral("MIND Active · ranker ready");
    } else if (s.assistRequested) {
        s.stateText = QStringLiteral("Active queued · %1").arg(s.readinessReason);
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
    {
        QMutexLocker locker(&m_mutex);
        if (m_assistMode == clean) return;
        m_assistMode = clean;
        double bitSum = 0.0;
        for (double v : m_bitAccuracyWindow) bitSum += v;
        const double bitAccuracy = !m_bitAccuracyWindow.empty()
            ? bitSum / static_cast<double>(m_bitAccuracyWindow.size())
            : 0.0;
        const bool readyNow = mindAssistReadyForFt(static_cast<int>(m_validationWindow.size()),
                                                   static_cast<int>(m_samples.size()),
                                                   bitAccuracy,
                                                   m_bestBitAccuracy);
        m_assistEnabled = (m_assistMode == QStringLiteral("assisted")) && readyNow;
        queued = (m_assistMode == QStringLiteral("assisted")) && !readyNow;
        active = m_assistEnabled;
        m_statsDirty = true;
    }
    if (clean == QStringLiteral("off")) {
        emit logMessage(QStringLiteral("MIND Assist mode: Off. Native FT decoder runs without MIND ranking."));
    } else if (clean == QStringLiteral("shadow")) {
        emit logMessage(QStringLiteral("MIND Assist mode: Training. MIND learns/scores but does not alter decoding."));
    } else if (queued) {
        emit logMessage(QStringLiteral("MIND Assist mode: Active requested; staying training-only until the candidate ranker assist-ready gate is reached. Final FT messages remain CRC/unpack/parser guarded."));
    } else if (active) {
        emit logMessage(QStringLiteral("MIND Assist mode: Active. MIND may rank/prioritize candidates, but final FT text remains CRC/unpack/parser guarded."));
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
        domain != QStringLiteral("RTTY") && domain != QStringLiteral("CW")) {
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

void DeepDspController::observeAudioBlock(const QString &mode, const AudioBlock &block)
{
    const QString domain = normalizedDomain(mode);
    const int idx = domainIndex(domain);
    if (idx < 0) return;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_enabled || normalizedAssistMode(m_assistMode) == QStringLiteral("off")) {
            return;
        }
    }
    const QVector<float> features = audioFeatures(block);
    QMutexLocker locker(&m_mutex);
    if (!m_enabled || normalizedAssistMode(m_assistMode) == QStringLiteral("off")) return;
    m_activeProfile = domain;
    m_lastFeaturesByMode.insert(domain, features);
    m_lastRealtimeActivityMs = QDateTime::currentMSecsSinceEpoch();
}

void DeepDspController::submitConfirmedText(const QString &mode, const QString &text)
{
    const QString domain = normalizedDomain(mode);
    const int idx = domainIndex(domain);
    if (idx < 0) return;
    const QString label = text.trimmed();
    if (label.isEmpty()) return;

    {
        QMutexLocker locker(&m_mutex);
        if (!m_enabled || normalizedAssistMode(m_assistMode) == QStringLiteral("off")) return;
        const QVector<float> baseFeatures = m_lastFeaturesByMode.value(domain);
        if (baseFeatures.size() != kInputCount) return;

        m_activeProfile = domain;
        if (domain == QStringLiteral("RTTY")) {
            ProfileSample sample;
            sample.input = resampleFeatureVector(baseFeatures, 96);
            sample.target = makeRttyTarget(label);
            sample.mode = domain;
            sample.label = label;
            if (sample.input.size() == 96 && sample.target.size() == 8) {
                m_rttySamplesQueue.push_back(sample);
                while (m_rttySamplesQueue.size() > 8000) m_rttySamplesQueue.pop_front();
                ++m_rttySamples;
                ++m_manualSamples;
                m_statsDirty = true;
            }
        } else if (domain == QStringLiteral("CW")) {
            // CW decoded text is not used as final neural truth: CW Assist is an
            // event detector (key-down / dit / dah / gaps), not an audio-to-text
            // generator.  Keep lightweight profile accounting here; real event
            // samples are generated by the synthetic bootcamp and future marker
            // detector hooks.
            ++m_cwSamples;
            ++m_manualSamples;
            m_statsDirty = true;
        } else if (domain == QStringLiteral("FT8") || domain == QStringLiteral("FT4")) {
            // Manual FT text is not a valid ranker label.  The FT ranker learns
            // only from native CRC-valid positives and native LDPC/CRC/message
            // drop negatives emitted by Ft8RxDecoder.
            return;
        }
    }
    emitStatusThrottled();
}


void DeepDspController::submitCwEventSample(const QVector<float> &input, const QVector<float> &target, const QString &label)
{
    if (input.size() != 256 || target.size() != 6) {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        if (!m_enabled || normalizedAssistMode(m_assistMode) == QStringLiteral("off") || m_cwNetwork == nullptr) {
            return;
        }

        ProfileSample sample;
        sample.input = input;
        sample.target = target;
        sample.mode = QStringLiteral("CW");
        sample.label = label.trimmed();
        m_cwSamplesQueue.push_back(sample);
        while (m_cwSamplesQueue.size() > 12000) {
            m_cwSamplesQueue.pop_front();
        }
        ++m_cwSamples;
        m_activeProfile = QStringLiteral("CW");
        m_lastRealtimeActivityMs = QDateTime::currentMSecsSinceEpoch();
        m_statsDirty = true;
    }

    emitStatusThrottled();
}


void DeepDspController::submitRttyBitSample(const QVector<float> &input, const QVector<float> &target, const QString &label)
{
    if (input.size() != 96 || target.size() != 8) {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        if (!m_enabled || normalizedAssistMode(m_assistMode) == QStringLiteral("off") || m_rttyNetwork == nullptr) {
            return;
        }

        ProfileSample sample;
        sample.input = input;
        sample.target = target;
        sample.mode = QStringLiteral("RTTY");
        sample.label = label.trimmed();
        m_rttySamplesQueue.push_back(sample);
        while (m_rttySamplesQueue.size() > 12000) {
            m_rttySamplesQueue.pop_front();
        }
        ++m_rttySamples;
        m_activeProfile = QStringLiteral("RTTY");
        m_lastRealtimeActivityMs = QDateTime::currentMSecsSinceEpoch();
        m_statsDirty = true;
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
        m_neuralWorkDeferredUntilMs = QDateTime::currentMSecsSinceEpoch() + 3000;
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
        m_rttySamples = 0;
        m_cwSamples = 0;
        m_nativeFtSamples = 0;
        m_manualSamples = 0;
        m_cwBootcampSamples = 0;
        m_cwTrainingRuns = 0;
        m_rttyTrainingRuns = 0;
        m_cwLastLoss = 0.0;
        m_rttyLastLoss = 0.0;
        m_cwSamplesQueue.clear();
        m_rttySamplesQueue.clear();
        m_cwAccuracyWindow.clear();
        m_rttyAccuracyWindow.clear();
        m_lastLoss = 0.0;
        m_bestBitAccuracy = 0.0;
        m_replayCursor = 0;
        m_loadedGoldSamples = 0;
        QFile::remove(checkpointPath());
        QFile::remove(defaultCwCheckpointPath());
        QFile::remove(defaultRttyCheckpointPath());
        QFile::remove(statsPath());
        QFile::remove(goldDatasetPath());
        m_statsDirty = false;
        m_goldDatasetDirty = false;
        m_lastStatsSaveMs = 0;
        m_lastStatsSavedSampleCount = 0;
    }
    emit logMessage(QStringLiteral("MIND ranker model and FT candidate replay buffer reset."));
    emitStatus();
}


void DeepDspController::saveCheckpoint()
{
    bool modelOk = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_network == nullptr) return;
        modelOk = m_network->save(checkpointPath());
        if (m_cwNetwork != nullptr) m_cwNetwork->save(defaultCwCheckpointPath());
        if (m_rttyNetwork != nullptr) m_rttyNetwork->save(defaultRttyCheckpointPath());
        saveStats();
        m_statsDirty = false;
        m_goldDatasetDirty = false;
        m_lastStatsSaveMs = QDateTime::currentMSecsSinceEpoch();
        m_lastStatsSavedSampleCount = m_nativeFtSamples + m_manualSamples + m_rttySamples + m_cwSamples + m_cwBootcampSamples;
    }
    if (modelOk) {
        emit logMessage(QStringLiteral("MIND ranker checkpoint and FT candidate replay buffer saved: %1").arg(checkpointPath()));
    } else {
        emit logMessage(QStringLiteral("MIND checkpoint save failed."));
    }
    emitStatus();
}


void DeepDspController::loadCheckpoint()
{
    bool ftLoaded = false;
    bool cwLoaded = false;
    bool rttyLoaded = false;
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

        QFileInfo cwInfo(defaultCwCheckpointPath());
        if (m_cwNetwork != nullptr && cwInfo.exists() && cwInfo.size() > 0) {
            cwLoaded = m_cwNetwork->load(defaultCwCheckpointPath());
        }

        QFileInfo rttyInfo(defaultRttyCheckpointPath());
        if (m_rttyNetwork != nullptr && rttyInfo.exists() && rttyInfo.size() > 0) {
            rttyLoaded = m_rttyNetwork->load(defaultRttyCheckpointPath());
        }

        loadStats();
        loadGoldDataset();
    }
    if (ftLoaded || cwLoaded || rttyLoaded) {
        emit logMessage(QStringLiteral("MIND checkpoint loaded: FT %1, CW %2, RTTY %3")
                        .arg(ftLoaded ? QStringLiteral("yes") : QStringLiteral("no"))
                        .arg(cwLoaded ? QStringLiteral("yes") : QStringLiteral("no"))
                        .arg(rttyLoaded ? QStringLiteral("yes") : QStringLiteral("no")));
    } else {
        emit logMessage(QStringLiteral("MIND checkpoints missing; starting with fresh FT/RTTY/CW models."));
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
    double cwSum = 0.0;
    for (double v : m_cwAccuracyWindow) cwSum += v;
    const double cwAccuracy = !m_cwAccuracyWindow.empty()
        ? cwSum / static_cast<double>(m_cwAccuracyWindow.size())
        : 0.0;
    double rttySum = 0.0;
    for (double v : m_rttyAccuracyWindow) rttySum += v;
    const double rttyAccuracy = !m_rttyAccuracyWindow.empty()
        ? rttySum / static_cast<double>(m_rttyAccuracyWindow.size())
        : 0.0;
    const double sampleReadiness = qMin(1.0, static_cast<double>(validationCount) / static_cast<double>(kMinReadyValidation));
    double readiness = qMin(100.0, (0.75 * bitAccuracy + 0.25 * messageAccuracy) * sampleReadiness);
    if (m_activeProfile == QStringLiteral("CW")) {
        const double cwReadiness = qMin(1.0, static_cast<double>(m_cwBootcampSamples) / 5000.0);
        readiness = qMin(100.0, cwAccuracy * cwReadiness);
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("engine"), QStringLiteral("MIND"));
    obj.insert(QStringLiteral("version"), 11);
    obj.insert(QStringLiteral("architecture"), QStringLiteral("multi_mode_ft_ranker_rtty_slicer_cw_event_v1"));
    obj.insert(QStringLiteral("trainer_thread"), QStringLiteral("autonomous_low_priority_qthread"));
    obj.insert(QStringLiteral("math_backend"), QStringLiteral("LowPriority CPU multi-mode MIND / Eigen threads capped during training"));
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
    obj.insert(QStringLiteral("active_profile"), m_activeProfile);
    obj.insert(QStringLiteral("rtty_samples"), m_rttySamples);
    obj.insert(QStringLiteral("rtty_training_runs"), m_rttyTrainingRuns);
    obj.insert(QStringLiteral("rtty_loss"), m_rttyLastLoss);
    obj.insert(QStringLiteral("rtty_accuracy_percent"), rttyAccuracy);
    obj.insert(QStringLiteral("cw_samples"), m_cwSamples);
    obj.insert(QStringLiteral("cw_bootcamp_samples"), m_cwBootcampSamples);
    obj.insert(QStringLiteral("cw_training_runs"), m_cwTrainingRuns);
    obj.insert(QStringLiteral("cw_loss"), m_cwLastLoss);
    obj.insert(QStringLiteral("cw_accuracy_percent"), cwAccuracy);
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
    const bool assistReady = mindAssistReadyForFt(validationCount, static_cast<int>(m_samples.size()), bitAccuracy, m_bestBitAccuracy);
    obj.insert(QStringLiteral("message_accuracy_percent"), messageAccuracy);
    obj.insert(QStringLiteral("exact_frame_accuracy_percent"), messageAccuracy);
    obj.insert(QStringLiteral("estimated_exact_frame_from_bit_percent"), estimatedExactFramePercent(bitAccuracy));
    obj.insert(QStringLiteral("candidate_success_probability_model"), true);
    obj.insert(QStringLiteral("validation_success_percent"), messageAccuracy);
    obj.insert(QStringLiteral("assist_ready"), assistReady);
    obj.insert(QStringLiteral("assist_readiness_reason"), mindAssistReadinessReason(validationCount, static_cast<int>(m_samples.size()), bitAccuracy, m_bestBitAccuracy));
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
    if (version != 11 || arch != QStringLiteral("multi_mode_ft_ranker_rtty_slicer_cw_event_v1")) {
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
    const QString loadedProfile = obj.value(QStringLiteral("active_profile")).toString();
    if (!loadedProfile.trimmed().isEmpty()) m_activeProfile = normalizedDomain(loadedProfile);
    m_assistMode = normalizedAssistMode(obj.value(QStringLiteral("mind_assist_mode")).toString(m_assistMode));
    m_assistEnabled = false;
    m_rttySamples = obj.value(QStringLiteral("rtty_samples")).toInt(m_rttySamples);
    m_rttyTrainingRuns = obj.value(QStringLiteral("rtty_training_runs")).toInt(m_rttyTrainingRuns);
    m_rttyLastLoss = obj.value(QStringLiteral("rtty_loss")).toDouble(m_rttyLastLoss);
    const double rttyLoadedAccuracy = obj.value(QStringLiteral("rtty_accuracy_percent")).toDouble(0.0);
    if (rttyLoadedAccuracy > 0.0) {
        m_rttyAccuracyWindow.clear();
        m_rttyAccuracyWindow.push_back(rttyLoadedAccuracy);
    }
    m_cwSamples = obj.value(QStringLiteral("cw_samples")).toInt(m_cwSamples);
    m_cwBootcampSamples = obj.value(QStringLiteral("cw_bootcamp_samples")).toInt(m_cwBootcampSamples);
    m_cwTrainingRuns = obj.value(QStringLiteral("cw_training_runs")).toInt(m_cwTrainingRuns);
    m_cwLastLoss = obj.value(QStringLiteral("cw_loss")).toDouble(m_cwLastLoss);
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
        for (int i = 0; i < cappedCount; ++i) {
            m_validationWindow.push_back(i < okCount);
        }
    }

    m_lastStatsSavedSampleCount = m_nativeFtSamples + m_manualSamples + m_rttySamples + m_cwSamples + m_cwBootcampSamples;
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
    ds << QStringLiteral("MM_MIND_FT_RANKER_V1") << static_cast<qint32>(nativeSamples.size());
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
    if (magic != QStringLiteral("MM_MIND_FT_RANKER_V1") || count < 0) return false;

    std::deque<Sample> loaded;
    int loadedFt8 = 0;
    int loadedFt4 = 0;
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
        if ((domain == QStringLiteral("FT8") || domain == QStringLiteral("FT4")) &&
            input.size() == kInputCount && target.size() == kOutputCount) {
            Sample s;
            s.mode = domain;
            s.label = label;
            s.input = input;
            s.target = target;
            s.nativeFt = true;
            loaded.push_back(s);
            if (domain == QStringLiteral("FT8")) ++loadedFt8;
            else ++loadedFt4;
        }
    }

    if (loaded.empty()) return false;
    m_samples = loaded;
    m_nativeFtSamples = static_cast<int>(m_samples.size());
    m_ft8Samples = loadedFt8;
    m_ft4Samples = loadedFt4;
    m_ftSamples = m_nativeFtSamples;
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
    if (profile == QStringLiteral("CW")) {
        return QStringLiteral("Autonomous · CW protected");
    }
    if (profile == QStringLiteral("FT8") || profile == QStringLiteral("FT4")) {
        return budgetMs > 0 ? QStringLiteral("Autonomous · FT idle training")
                            : QStringLiteral("Autonomous · FT idle");
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
        nowMs < m_neuralWorkDeferredUntilMs || m_network == nullptr) {
        if (batchCap != nullptr) *batchCap = 0;
        return 0;
    }

    const bool recentRealtime = (m_lastRealtimeActivityMs > 0) &&
                                (nowMs - m_lastRealtimeActivityMs < 1500);
    const bool activeCw = (m_activeProfile == QStringLiteral("CW"));
    const bool activeTextProfile = activeCw || m_activeProfile == QStringLiteral("RTTY");

    // CW/RTTY profile training is very small compared with FT candidate
    // training.  Do not run it while fresh realtime audio is arriving, but do
    // let it catch up a moment after RX activity stops; otherwise the panel
    // shows samples forever and Active never becomes useful.
    if (activeCw && recentRealtime) {
        // CW event profile is tiny.  Let it keep up during real CW copy with a
        // small low-priority slice; otherwise Active appears to do nothing while
        // operators are actually sending.  FT decode-critical guards still win.
        if (batchCap != nullptr) *batchCap = 8;
        return 8;
    }

    if (m_activityHint == QStringLiteral("idle_heavy")) {
        budget = activeTextProfile ? 12 : 32;
        cap = activeTextProfile ? 32 : m_ftBatchSize;
    } else if (m_activityHint == QStringLiteral("interactive")) {
        budget = 0;
        cap = 0;
    } else if (recentRealtime) {
        budget = 0;
        cap = 0;
    } else {
        budget = activeTextProfile ? 8 : 12;
        cap = activeTextProfile ? 24 : 64;
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
    int cwBatches = 0;
    int rttyBatches = 0;

    {
        QMutexLocker locker(&m_mutex);

        if (!m_enabled || normalizedAssistMode(m_assistMode) == QStringLiteral("off") ||
            m_decodeCritical || nowMs < m_decodeCriticalCooldownUntilMs ||
            nowMs < m_neuralWorkDeferredUntilMs || m_network == nullptr) {
            const bool changed = (m_lastAdaptiveBudgetMs != 0 || m_lastAdaptiveBatchSize != 0);
            m_lastAdaptiveBudgetMs = 0;
            m_lastAdaptiveBatchSize = 0;
            // Latency-first: while explicit FT critical guard or native
            // candidate-driven cooldown is active, do not wake the GUI every
            // trainer tick.
            shouldEmit = changed;
        } else {
            const int persistedSampleCount = m_nativeFtSamples + m_manualSamples + m_rttySamples + m_cwSamples + m_cwBootcampSamples;
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

                if (!m_cwSamplesQueue.empty() && m_cwNetwork != nullptr &&
                    m_activityHint != QStringLiteral("interactive") && budgetMs >= 8) {
                    QVector<ProfileSample> cwBatch;
                    const int take = qMin((m_activityHint == QStringLiteral("idle_heavy")) ? 64 : 24,
                                          static_cast<int>(m_cwSamplesQueue.size()));
                    cwBatch.reserve(take);
                    for (int i = 0; i < take; ++i) {
                        cwBatch.append(m_cwSamplesQueue.front());
                        m_cwSamplesQueue.pop_front();
                    }
                    m_cwLastLoss = trainCwSamples(cwBatch);
                    ++m_cwTrainingRuns;
                    ++cwBatches;
                    trainedSomething = true;
                    m_statsDirty = true;
                }

                if (!m_rttySamplesQueue.empty() && m_rttyNetwork != nullptr &&
                    m_activityHint != QStringLiteral("interactive") && budgetMs >= 8) {
                    QVector<ProfileSample> rttyBatch;
                    const int take = qMin((m_activityHint == QStringLiteral("idle_heavy")) ? 64 : 24,
                                          static_cast<int>(m_rttySamplesQueue.size()));
                    rttyBatch.reserve(take);
                    for (int i = 0; i < take; ++i) {
                        rttyBatch.append(m_rttySamplesQueue.front());
                        m_rttySamplesQueue.pop_front();
                    }
                    m_rttyLastLoss = trainRttySamples(rttyBatch);
                    ++m_rttyTrainingRuns;
                    ++rttyBatches;
                    trainedSomething = true;
                    m_statsDirty = true;
                }

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
            if ((m_trainingRuns > 0 || m_cwTrainingRuns > 0 || m_rttyTrainingRuns > 0) &&
                QDateTime::currentMSecsSinceEpoch() - m_lastAutoCheckpointMs >= 5 * 60 * 1000) {
                if (m_network != nullptr) m_network->save(checkpointPath());
                if (m_cwNetwork != nullptr) m_cwNetwork->save(defaultCwCheckpointPath());
                if (m_rttyNetwork != nullptr) m_rttyNetwork->save(defaultRttyCheckpointPath());
                saveStats();
                m_statsDirty = false;
                m_goldDatasetDirty = false;
                m_lastAutoCheckpointMs = QDateTime::currentMSecsSinceEpoch();
                checkpointSaved = true;
            }
            shouldEmit = (ftBatches > 0 || cwBatches > 0 || rttyBatches > 0 || m_statsDirty);
        }
    }

    if (checkpointSaved) {
        emit logMessage(QStringLiteral("MIND auto-checkpoint saved by dedicated trainer thread."));
    }
    if (shouldEmit) emitStatusThrottled();
}


QVector<float> DeepDspController::audioFeatures(const AudioBlock &block) const
{
    QVector<float> out(kInputCount, 0.0f);
    if (block.samples.isEmpty()) return out;

    double sum = 0.0;
    double sum2 = 0.0;
    double peak = 0.0;
    int zeroCross = 0;
    float last = block.samples.first();
    for (float x : block.samples) {
        sum += x;
        sum2 += static_cast<double>(x) * static_cast<double>(x);
        peak = std::max(peak, std::abs(static_cast<double>(x)));
        if ((x >= 0.0f) != (last >= 0.0f)) ++zeroCross;
        last = x;
    }
    const double n = static_cast<double>(block.samples.size());
    const double mean = sum / n;
    const double rms = std::sqrt(sum2 / n);
    out[0] = static_cast<float>(qBound(0.0, rms * 8.0, 1.0));
    out[1] = static_cast<float>(qBound(0.0, peak * 4.0, 1.0));
    out[2] = static_cast<float>(qBound(0.0, std::abs(mean) * 16.0, 1.0));
    out[3] = static_cast<float>(qBound(0.0, static_cast<double>(zeroCross) / n * 20.0, 1.0));

    const int bins = kInputCount - 4;
    for (int b = 0; b < bins; ++b) {
        const int a = (block.samples.size() * b) / bins;
        const int z = (block.samples.size() * (b + 1)) / bins;
        double e = 0.0;
        for (int i = a; i < z; ++i) {
            const double x = block.samples[i];
            e += x * x;
        }
        const int len = qMax(1, z - a);
        out[4 + b] = static_cast<float>(qBound(0.0, std::sqrt(e / static_cast<double>(len)) * 10.0, 1.0));
    }
    return out;
}

QVector<float> DeepDspController::targetFingerprint(const QString &mode, const QString &text) const
{
    const QByteArray data = (normalizedDomain(mode) + QStringLiteral("|") + text.trimmed().toUpper()).toUtf8();
    const QByteArray digest = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    QVector<float> out;
    out.reserve(kOutputCount);
    for (int i = 0; i < kOutputCount; ++i) {
        const int byte = static_cast<unsigned char>(digest.at(i % digest.size()));
        out.append(((byte >> (i % 8)) & 0x01) ? 1.0f : 0.0f);
    }
    return out;
}


QVector<float> DeepDspController::makeOneHot(int n, int index) const
{
    QVector<float> out(qMax(1, n), 0.0f);
    if (index >= 0 && index < out.size()) out[index] = 1.0f;
    return out;
}

QVector<float> DeepDspController::resampleFeatureVector(const QVector<float> &input, int n) const
{
    QVector<float> out(qMax(1, n), 0.0f);
    if (input.isEmpty()) return out;
    if (out.size() == input.size()) return input;
    for (int i = 0; i < out.size(); ++i) {
        const int a = (input.size() * i) / out.size();
        const int z = qMax(a + 1, (input.size() * (i + 1)) / out.size());
        double sum = 0.0;
        int count = 0;
        for (int j = a; j < z && j < input.size(); ++j) {
            sum += input[j];
            ++count;
        }
        out[i] = static_cast<float>(qBound(0.0, count > 0 ? sum / static_cast<double>(count) : 0.0, 1.0));
    }
    return out;
}

QVector<float> DeepDspController::makeRttyTarget(const QString &text) const
{
    const QString t = text.trimmed().toUpper();
    int klass = 7; // unknown/noise/other
    if (t.isEmpty()) {
        klass = 7;
    } else {
        const QChar c = t.at(0);
        if (c.isLetter()) klass = 0;
        else if (c.isDigit()) klass = 1;
        else if (c.isSpace()) klass = 2;
        else if (QStringLiteral(".,;:/?-_'").contains(c)) klass = 3;
        else if (QStringLiteral("+=$@#&*!()").contains(c)) klass = 4;
        else if (c == QChar(0x000A) || c == QChar(0x000D)) klass = 5;
        else klass = 6;
    }
    return makeOneHot(8, klass);
}

QVector<float> DeepDspController::makeCwFeature(const QString &token, int klass, quint32 seed) const
{
    constexpr int kCwInput = 256;
    QVector<float> out(kCwInput, 0.0f);
    std::mt19937 rng(seed ^ static_cast<quint32>(token.size() * 2654435761u) ^ static_cast<quint32>(klass * 1013904223u));
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);
    std::normal_distribution<float> noise(0.0f, 0.045f);

    auto addPulse = [&](int start, int len, float level) {
        const int a = qBound(0, start, kCwInput - 1);
        const int z = qBound(0, start + len, kCwInput);
        for (int i = a; i < z; ++i) {
            const float edgeIn = qMin(1.0f, static_cast<float>(i - a + 1) / 8.0f);
            const float edgeOut = qMin(1.0f, static_cast<float>(z - i) / 8.0f);
            out[i] = qMax(out[i], level * qMin(edgeIn, edgeOut));
        }
    };

    switch (klass) {
    case 0: { // dit
        const int len = 28 + static_cast<int>(uni(rng) * 18.0f);
        const int start = 84 + static_cast<int>(uni(rng) * 28.0f);
        addPulse(start, len, 0.82f + 0.16f * uni(rng));
        break;
    }
    case 1: { // dah
        const int len = 82 + static_cast<int>(uni(rng) * 36.0f);
        const int start = 55 + static_cast<int>(uni(rng) * 32.0f);
        addPulse(start, len, 0.82f + 0.16f * uni(rng));
        break;
    }
    case 2: { // intra-character gap: short quiet notch between two elements
        addPulse(24, 34 + static_cast<int>(uni(rng) * 18.0f), 0.65f);
        addPulse(168, 34 + static_cast<int>(uni(rng) * 18.0f), 0.65f);
        break;
    }
    case 3: { // letter gap: mostly quiet with weak shoulders
        addPulse(4, 18 + static_cast<int>(uni(rng) * 10.0f), 0.35f);
        addPulse(224, 20 + static_cast<int>(uni(rng) * 12.0f), 0.35f);
        break;
    }
    case 4: { // word gap: quiet window
        break;
    }
    default: { // noise/QRM
        for (int k = 0; k < 3; ++k) {
            if (uni(rng) < 0.55f) {
                const int start = static_cast<int>(uni(rng) * 220.0f);
                const int len = 8 + static_cast<int>(uni(rng) * 34.0f);
                addPulse(start, len, 0.18f + 0.45f * uni(rng));
            }
        }
        break;
    }
    }

    // Add slow fading/AGC and realistic receiver noise.
    const float gain = 0.75f + 0.5f * uni(rng);
    const float floor = 0.015f + 0.025f * uni(rng);
    for (int i = 0; i < kCwInput; ++i) {
        const float fade = 0.90f + 0.10f * std::sin(6.2831853f * static_cast<float>(i) / static_cast<float>(kCwInput) + uni(rng));
        float v = out[i] * gain * fade + floor + noise(rng);
        out[i] = qBound(0.0f, v, 1.0f);
    }
    return out;
}

QVector<DeepDspController::ProfileSample> DeepDspController::generateCwBootcampSamples(int count) const
{
    QVector<ProfileSample> out;
    out.reserve(qMax(1, count));
    const QStringList tokens = {
        QStringLiteral("CQ"), QStringLiteral("DE"), QStringLiteral("K"), QStringLiteral("KN"),
        QStringLiteral("5NN"), QStringLiteral("599"), QStringLiteral("TU"), QStringLiteral("TNX"),
        QStringLiteral("FER"), QStringLiteral("QSO"), QStringLiteral("73"), QStringLiteral("RST"),
        QStringLiteral("NAME"), QStringLiteral("QTH"), QStringLiteral("PWR"), QStringLiteral("ANT")
    };
    for (int i = 0; i < count; ++i) {
        const int klass = i % 6;
        const QString token = tokens.at(i % tokens.size());
        ProfileSample s;
        s.input = makeCwFeature(token, klass, static_cast<quint32>(0xC0FFEEu + i * 7919u));
        s.target = makeOneHot(6, klass);
        s.mode = QStringLiteral("CW");
        s.label = token;
        out.append(s);
    }
    return out;
}

void DeepDspController::runCwBootcamp()
{
    const QVector<ProfileSample> generated = generateCwBootcampSamples(2400);
    {
        QMutexLocker locker(&m_mutex);
        if (!m_enabled || m_cwNetwork == nullptr) return;
        for (const ProfileSample &s : generated) {
            m_cwSamplesQueue.push_back(s);
            while (m_cwSamplesQueue.size() > 8000) m_cwSamplesQueue.pop_front();
        }
        m_cwSamples += generated.size();
        m_cwBootcampSamples += generated.size();
        m_activeProfile = QStringLiteral("CW");
        m_statsDirty = true;
    }
    emit logMessage(QStringLiteral("MIND CW bootcamp queued: %1 synthetic symbol/gap sample(s).").arg(generated.size()));
    emitStatus();
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

void DeepDspController::updateValidation(const QVector<float> &prediction, const QVector<float> &target)
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
    while (m_bitAccuracyWindow.size() > 1000) m_bitAccuracyWindow.pop_front();
    while (m_validationWindow.size() > 1000) m_validationWindow.pop_front();
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
    delete m_cwNetwork;
    m_cwNetwork = new DeepDspProfileNet(QVector<int>{256, 96, 48, 6}, 1u);
    delete m_rttyNetwork;
    m_rttyNetwork = new DeepDspProfileNet(QVector<int>{96, 64, 32, 8}, 1u);
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
        updateValidation(prediction, targets.constFirst());
    }
    return m_network->trainBatch(inputs, targets, 0.003f);
}


double DeepDspController::trainCwSamples(const QVector<ProfileSample> &batch)
{
    if (batch.isEmpty() || m_cwNetwork == nullptr) return 0.0;
    QVector<QVector<float>> inputs;
    QVector<QVector<float>> targets;
    inputs.reserve(batch.size());
    targets.reserve(batch.size());
    for (const ProfileSample &s : batch) {
        if (s.input.size() == 256 && s.target.size() == 6) {
            inputs.append(s.input);
            targets.append(s.target);
        }
    }
    if (inputs.isEmpty()) return 0.0;

    const QVector<float> prediction = m_cwNetwork->predict(inputs.constFirst());
    if (prediction.size() == targets.constFirst().size()) {
        int bestP = 0;
        int bestT = 0;
        for (int i = 1; i < prediction.size(); ++i) if (prediction[i] > prediction[bestP]) bestP = i;
        for (int i = 1; i < targets.constFirst().size(); ++i) if (targets.constFirst()[i] > targets.constFirst()[bestT]) bestT = i;
        m_cwAccuracyWindow.push_back(bestP == bestT ? 100.0 : 0.0);
        while (m_cwAccuracyWindow.size() > 1000) m_cwAccuracyWindow.pop_front();
    }
    return m_cwNetwork->trainBatch(inputs, targets, 0.006f);
}


double DeepDspController::trainRttySamples(const QVector<ProfileSample> &batch)
{
    if (batch.isEmpty() || m_rttyNetwork == nullptr) return 0.0;
    QVector<QVector<float>> inputs;
    QVector<QVector<float>> targets;
    inputs.reserve(batch.size());
    targets.reserve(batch.size());
    for (const ProfileSample &s : batch) {
        if (s.input.size() == 96 && s.target.size() == 8) {
            inputs.append(s.input);
            targets.append(s.target);
        }
    }
    if (inputs.isEmpty()) return 0.0;

    const QVector<float> prediction = m_rttyNetwork->predict(inputs.constFirst());
    if (prediction.size() == targets.constFirst().size()) {
        int bestP = 0;
        int bestT = 0;
        for (int i = 1; i < prediction.size(); ++i) if (prediction[i] > prediction[bestP]) bestP = i;
        for (int i = 1; i < targets.constFirst().size(); ++i) if (targets.constFirst()[i] > targets.constFirst()[bestT]) bestT = i;
        m_rttyAccuracyWindow.push_back(bestP == bestT ? 100.0 : 0.0);
        while (m_rttyAccuracyWindow.size() > 1000) m_rttyAccuracyWindow.pop_front();
    }
    return m_rttyNetwork->trainBatch(inputs, targets, 0.005f);
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


bool DeepDspController::scoreRttyBitFeature(const QVector<float> &bitFeature,
                                           QVector<float> *bitProbabilities,
                                           double *confidencePercent)
{
    if (bitProbabilities != nullptr) {
        bitProbabilities->clear();
    }
    if (confidencePercent != nullptr) {
        *confidencePercent = 0.0;
    }
    if (bitFeature.size() != 96) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    const QString mode = normalizedAssistMode(m_assistMode);
    if (!m_enabled || mode != QStringLiteral("assisted") || m_rttyNetwork == nullptr) {
        return false;
    }

    double rttyAccSum = 0.0;
    for (double v : m_rttyAccuracyWindow) {
        rttyAccSum += v;
    }
    const double rttyAccuracy = !m_rttyAccuracyWindow.empty()
        ? rttyAccSum / static_cast<double>(m_rttyAccuracyWindow.size())
        : 0.0;
    if (!mindProfileReady(m_rttyTrainingRuns, static_cast<int>(m_rttyAccuracyWindow.size()), rttyAccuracy)) {
        return false;
    }

    QVector<float> prediction = m_rttyNetwork->predict(bitFeature);
    if (prediction.size() != 8) {
        return false;
    }

    int best = 0;
    for (int i = 1; i < prediction.size(); ++i) {
        if (prediction.at(i) > prediction.at(best)) {
            best = i;
        }
    }

    if (bitProbabilities != nullptr) {
        *bitProbabilities = prediction;
    }
    if (confidencePercent != nullptr) {
        *confidencePercent = 100.0 * qBound(0.0, static_cast<double>(prediction.at(best)), 1.0);
    }

    m_activeProfile = QStringLiteral("RTTY");
    m_lastRealtimeActivityMs = QDateTime::currentMSecsSinceEpoch();
    return true;
}

bool DeepDspController::scoreCwEventFeature(const QVector<float> &eventFeature,
                                          QVector<float> *eventProbabilities,
                                          double *confidencePercent)
{
    if (eventProbabilities != nullptr) {
        eventProbabilities->clear();
    }
    if (confidencePercent != nullptr) {
        *confidencePercent = 0.0;
    }
    if (eventFeature.size() != 256) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    const QString mode = normalizedAssistMode(m_assistMode);
    if (!m_enabled || mode != QStringLiteral("assisted") || m_cwNetwork == nullptr) {
        return false;
    }

    // Do not let a random/untrained profile steer the CW state machine.  The
    // synthetic bootcamp plus real event samples must have produced at least a
    // small validation history.  Until then Active behaves like classic CW.
    double cwAccSum = 0.0;
    for (double v : m_cwAccuracyWindow) {
        cwAccSum += v;
    }
    const double cwAccuracy = !m_cwAccuracyWindow.empty()
        ? cwAccSum / static_cast<double>(m_cwAccuracyWindow.size())
        : 0.0;
    if (!mindProfileReady(m_cwTrainingRuns, static_cast<int>(m_cwAccuracyWindow.size()), cwAccuracy)) {
        return false;
    }

    QVector<float> prediction = m_cwNetwork->predict(eventFeature);
    if (prediction.size() != 6) {
        return false;
    }

    int best = 0;
    for (int i = 1; i < prediction.size(); ++i) {
        if (prediction.at(i) > prediction.at(best)) {
            best = i;
        }
    }

    if (eventProbabilities != nullptr) {
        *eventProbabilities = prediction;
    }
    if (confidencePercent != nullptr) {
        *confidencePercent = 100.0 * qBound(0.0, static_cast<double>(prediction.at(best)), 1.0);
    }

    m_activeProfile = QStringLiteral("CW");
    m_lastRealtimeActivityMs = QDateTime::currentMSecsSinceEpoch();
    return true;
}

