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
constexpr int kOutputCount = 174;
constexpr int kMaxSamples = 20000;
constexpr int kMinReadyValidation = 200;

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
    return QDir(madnnessBaseDir()).filePath(QStringLiteral("mind_native_ft_eigen_v2.model"));
}

QString defaultStatsPath()
{
    return QDir(madnnessBaseDir()).filePath(QStringLiteral("mind_stats.json"));
}

QString defaultGoldDatasetPath()
{
    return QDir(madnnessBaseDir()).filePath(QStringLiteral("mind_ft_gold_samples_v1.dat"));
}

QString defaultCwCheckpointPath()
{
    return QDir(madnnessBaseDir()).filePath(QStringLiteral("mind_cw_symbols_eigen_v1.model"));
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
    m_trainTimer->setInterval(20);
    m_trainTimer->moveToThread(&m_trainingThread);
    connect(&m_trainingThread, &QThread::started, m_trainTimer, QOverload<>::of(&QTimer::start));
    connect(m_trainTimer, &QTimer::timeout, this, &DeepDspController::trainIdleSlice, Qt::DirectConnection);
    connect(&m_trainingThread, &QThread::finished, m_trainTimer, &QObject::deleteLater);
    m_trainingThread.setObjectName(QStringLiteral("MIND trainer"));
    m_trainingThread.start(QThread::NormalPriority);

    emit logMessage(QStringLiteral("MIND dedicated trainer thread started in full-speed mode; FT/CAT/audio threads are not used for training."));
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
}

DeepDspController::Status DeepDspController::status() const
{
    QMutexLocker locker(&m_mutex);
    Status s;
    s.enabled = m_enabled;
    s.assistEnabled = m_assistEnabled;
    s.ftSamples = m_ftSamples;
    s.ft8Samples = m_ft8Samples;
    s.ft4Samples = m_ft4Samples;
    s.rttySamples = m_rttySamples;
    s.cwSamples = m_cwSamples;
    s.nativeFtSamples = m_nativeFtSamples;
    s.manualSamples = m_manualSamples;
    s.replayBufferSamples = static_cast<int>(m_samples.size());
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
    double cwSum = 0.0;
    for (double v : m_cwAccuracyWindow) cwSum += v;
    s.cwAccuracy = !m_cwAccuracyWindow.empty() ? cwSum / static_cast<double>(m_cwAccuracyWindow.size()) : 0.0;
    s.validationAccuracy = s.messageAccuracy;
    const double sampleReadiness = qMin(1.0, static_cast<double>(s.validationCount) / static_cast<double>(kMinReadyValidation));
    s.trainingCompletionPercent = qMin(100.0, (0.75 * s.bitAccuracy + 0.25 * s.messageAccuracy) * sampleReadiness);
    if (m_activeProfile == QStringLiteral("CW")) {
        const double cwReadiness = qMin(1.0, static_cast<double>(m_cwBootcampSamples) / 5000.0);
        s.trainingCompletionPercent = qMin(100.0, s.cwAccuracy * cwReadiness);
    }
    s.lastLoss = (m_activeProfile == QStringLiteral("CW")) ? m_cwLastLoss : m_lastLoss;
    s.ftActivity = m_network != nullptr ? m_network->lastActivity() : QVector<float>();
    s.cwActivity = m_cwNetwork != nullptr ? m_cwNetwork->lastActivity() : QVector<float>();
    if (m_activeProfile == QStringLiteral("CW") && !s.cwActivity.isEmpty()) s.neuralActivity = s.cwActivity;
    else s.neuralActivity = s.ftActivity;
    s.ready = s.validationCount >= kMinReadyValidation && s.messageAccuracy >= 99.999;
    s.checkpointPath = checkpointPath();
    s.statsPath = statsPath();
    s.goldDatasetPath = goldDatasetPath();
    s.architectureText = QStringLiteral("464 → 128 → 64 → 174");
    s.activeProfile = m_activeProfile;
    s.eigenThreads = Eigen::nbThreads();
    s.ftBatchSize = m_lastFtBatchSize > 0 ? m_lastFtBatchSize : m_ftBatchSize;
    s.trainStepsPerSecond = m_trainStepsPerSecond;
    s.trainSamplesPerSecond = m_trainSamplesPerSecond;
    s.backendText = QStringLiteral("CPU Eigen/OpenMP (%1 thread%2)")
                        .arg(s.eigenThreads)
                        .arg(s.eigenThreads == 1 ? QString() : QStringLiteral("s"));
    QFileInfo cpInfo(checkpointPath());
    s.modelStateText = cpInfo.exists() ? QStringLiteral("checkpoint loaded/saved") : QStringLiteral("new model");
    s.lastCheckpointText = cpInfo.exists() ? cpInfo.lastModified().toString(Qt::ISODate) : QStringLiteral("never");
    if (!s.enabled) {
        s.stateText = QStringLiteral("Disabled");
    } else if (s.ready && s.assistEnabled) {
        s.stateText = QStringLiteral("Assist enabled");
    } else if (s.ready) {
        s.stateText = QStringLiteral("Ready");
    } else if (m_decodeCritical) {
        s.stateText = QStringLiteral("Queued during FT decode");
    } else {
        s.stateText = QStringLiteral("Shadow training");
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
    bool refused = false;
    {
        QMutexLocker locker(&m_mutex);
        const bool readyNow = static_cast<int>(m_validationWindow.size()) >= kMinReadyValidation;
        m_assistEnabled = enabled && readyNow;
        refused = enabled && !readyNow;
    }
    if (refused) {
        emit logMessage(QStringLiteral("MIND assist refused: model is still in shadow training."));
    }
    emitStatus();
}

void DeepDspController::setTrainingBudgetMs(int ms)
{
    {
        QMutexLocker locker(&m_mutex);
        m_trainingBudgetMs = qBound(0, ms, 250);
    }
    emitStatus();
}

void DeepDspController::setDecodeCritical(bool active)
{
    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_decodeCritical == active) return;
        m_decodeCritical = active;
        changed = true;
    }
    if (changed) {
        if (active) {
            emit logMessage(QStringLiteral("MIND deferred: FT decode/auto-test is timing-critical; labels are queued only."));
        } else {
            emit logMessage(QStringLiteral("MIND resumed: queued labels will be trained by the dedicated MIND trainer thread."));
        }
        emitStatus();
    }
}

void DeepDspController::observeAudioBlock(const QString &mode, const AudioBlock &block)
{
    const QString domain = normalizedDomain(mode);
    const int idx = domainIndex(domain);
    if (idx < 0) return;
    const QVector<float> features = audioFeatures(block);
    QMutexLocker locker(&m_mutex);
    if (!m_enabled) return;
    m_activeProfile = domain;
    m_lastFeaturesByMode.insert(domain, features);
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
        if (!m_enabled) return;
        const QVector<float> baseFeatures = m_lastFeaturesByMode.value(domain);
        if (baseFeatures.size() != kInputCount) return;

        Sample sample;
        sample.input = baseFeatures;
        sample.target = targetFingerprint(domain, label);
        sample.mode = domain;
        sample.label = label;
        sample.nativeFt = false;

        // Keep the decoder path strictly light-weight: collecting a label must not
        // run neural inference/training or force a repaint while FT decode timing is
        // being measured.  Validation is updated later by the low-priority trainer.
        appendSample(sample);
        m_activeProfile = domain;
        if (domain == QStringLiteral("FT8") || domain == QStringLiteral("FT4")) {
            ++m_ftSamples;
            if (domain == QStringLiteral("FT8")) ++m_ft8Samples;
            else ++m_ft4Samples;
            ++m_manualSamples;
            m_goldDatasetDirty = true;
        } else if (domain == QStringLiteral("RTTY")) {
            ++m_rttySamples;
            ++m_manualSamples;
        } else if (domain == QStringLiteral("CW")) {
            ++m_cwSamples;
            ++m_manualSamples;
        }
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
    if (candidateMagnitudes.size() != kInputCount || targetBits.size() != kOutputCount) return;

    {
        QMutexLocker locker(&m_mutex);
        if (!m_enabled) return;

        Sample sample;
        sample.input = candidateMagnitudes;
        sample.target = targetBits;
        sample.mode = domain;
        sample.label = message.trimmed().toUpper();
        sample.nativeFt = true;

        appendSample(sample);
        m_activeProfile = domain;
        ++m_ftSamples;
        if (domain == QStringLiteral("FT8")) ++m_ft8Samples;
        else if (domain == QStringLiteral("FT4")) ++m_ft4Samples;
        ++m_nativeFtSamples;
        m_statsDirty = true;
        m_goldDatasetDirty = true;
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
        m_cwLastLoss = 0.0;
        m_cwSamplesQueue.clear();
        m_cwAccuracyWindow.clear();
        m_lastLoss = 0.0;
        m_bestBitAccuracy = 0.0;
        m_replayCursor = 0;
        m_loadedGoldSamples = 0;
        QFile::remove(checkpointPath());
        QFile::remove(defaultCwCheckpointPath());
        QFile::remove(statsPath());
        QFile::remove(goldDatasetPath());
        m_statsDirty = false;
        m_goldDatasetDirty = false;
        m_lastStatsSaveMs = 0;
        m_lastStatsSavedSampleCount = 0;
    }
    emit logMessage(QStringLiteral("MIND model and FT gold replay buffer reset."));
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
        saveStats();
        m_statsDirty = false;
        m_goldDatasetDirty = false;
        m_lastStatsSaveMs = QDateTime::currentMSecsSinceEpoch();
        m_lastStatsSavedSampleCount = m_nativeFtSamples + m_manualSamples + m_rttySamples + m_cwSamples + m_cwBootcampSamples;
    }
    if (modelOk) {
        emit logMessage(QStringLiteral("MIND checkpoint and FT gold replay buffer saved: %1").arg(checkpointPath()));
    } else {
        emit logMessage(QStringLiteral("MIND checkpoint save failed."));
    }
    emitStatus();
}


void DeepDspController::loadCheckpoint()
{
    bool loaded = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_network == nullptr) return;
        QFileInfo fi(checkpointPath());
        if (!fi.exists() || fi.size() <= 0) return;
        if (m_network->load(checkpointPath())) {
            if (m_cwNetwork != nullptr) m_cwNetwork->load(defaultCwCheckpointPath());
            loadStats();
            loadGoldDataset();
            loaded = true;
        } else {
            rebuildNetwork();
        }
    }
    if (loaded) {
        emit logMessage(QStringLiteral("MIND checkpoint and FT gold replay buffer loaded: %1").arg(checkpointPath()));
    } else {
        emit logMessage(QStringLiteral("MIND checkpoint load failed; starting with a fresh model."));
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
    const double sampleReadiness = qMin(1.0, static_cast<double>(validationCount) / static_cast<double>(kMinReadyValidation));
    double readiness = qMin(100.0, (0.75 * bitAccuracy + 0.25 * messageAccuracy) * sampleReadiness);
    if (m_activeProfile == QStringLiteral("CW")) {
        const double cwReadiness = qMin(1.0, static_cast<double>(m_cwBootcampSamples) / 5000.0);
        readiness = qMin(100.0, cwAccuracy * cwReadiness);
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("engine"), QStringLiteral("MIND"));
    obj.insert(QStringLiteral("version"), 7);
    obj.insert(QStringLiteral("architecture"), QStringLiteral("464->128->64->174"));
    obj.insert(QStringLiteral("trainer_thread"), QStringLiteral("dedicated_fullspeed_qthread"));
    obj.insert(QStringLiteral("math_backend"), QStringLiteral("Eigen/OpenMP batched Matrix-Matrix"));
    obj.insert(QStringLiteral("eigen_threads"), Eigen::nbThreads());
    obj.insert(QStringLiteral("ft_batch_size"), m_lastFtBatchSize > 0 ? m_lastFtBatchSize : m_ftBatchSize);
    obj.insert(QStringLiteral("train_steps_per_second"), m_trainStepsPerSecond);
    obj.insert(QStringLiteral("train_samples_per_second"), m_trainSamplesPerSecond);
    obj.insert(QStringLiteral("trainer_budget_ms"), m_trainingBudgetMs);
    obj.insert(QStringLiteral("gold_dataset_path"), goldDatasetPath());
    obj.insert(QStringLiteral("replay_buffer_samples"), static_cast<int>(m_samples.size()));
    obj.insert(QStringLiteral("samples"), static_cast<int>(m_samples.size()));
    obj.insert(QStringLiteral("ft_samples"), m_ftSamples);
    obj.insert(QStringLiteral("ft8_samples"), m_ft8Samples);
    obj.insert(QStringLiteral("ft4_samples"), m_ft4Samples);
    obj.insert(QStringLiteral("active_profile"), m_activeProfile);
    obj.insert(QStringLiteral("rtty_samples"), m_rttySamples);
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
    obj.insert(QStringLiteral("bit_accuracy_percent"), bitAccuracy);
    obj.insert(QStringLiteral("best_bit_accuracy_percent"), m_bestBitAccuracy);
    obj.insert(QStringLiteral("message_accuracy_percent"), messageAccuracy);
    obj.insert(QStringLiteral("validation_success_percent"), messageAccuracy);
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
    if ((version != 4 && version != 5 && version != 6 && version != 7) || arch != QStringLiteral("464->128->64->174")) {
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
    m_rttySamples = obj.value(QStringLiteral("rtty_samples")).toInt(m_rttySamples);
    m_cwSamples = obj.value(QStringLiteral("cw_samples")).toInt(m_cwSamples);
    m_cwBootcampSamples = obj.value(QStringLiteral("cw_bootcamp_samples")).toInt(m_cwBootcampSamples);
    m_cwTrainingRuns = obj.value(QStringLiteral("cw_training_runs")).toInt(m_cwTrainingRuns);
    m_cwLastLoss = obj.value(QStringLiteral("cw_loss")).toDouble(m_cwLastLoss);
    m_nativeFtSamples = obj.value(QStringLiteral("native_ft_samples")).toInt(m_nativeFtSamples);
    m_manualSamples = obj.value(QStringLiteral("manual_samples")).toInt(m_manualSamples);
    m_trainingRuns = obj.value(QStringLiteral("training_runs")).toInt(m_trainingRuns);
    m_lastLoss = obj.value(QStringLiteral("last_loss")).toDouble(m_lastLoss);
    m_bestBitAccuracy = obj.value(QStringLiteral("best_bit_accuracy_percent")).toDouble(m_bestBitAccuracy);
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
    ds << QStringLiteral("MM_MIND_FT_GOLD_V1") << static_cast<qint32>(nativeSamples.size());
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
    if (magic != QStringLiteral("MM_MIND_FT_GOLD_V1") || count < 0) return false;

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


void DeepDspController::trainIdleSlice()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 sliceStartMs = nowMs;
    bool shouldEmit = false;
    bool checkpointSaved = false;
    int ftBatches = 0;
    int cwBatches = 0;

    {
        QMutexLocker locker(&m_mutex);

        if (!m_enabled || m_decodeCritical || m_network == nullptr) {
            shouldEmit = true;
        } else {
            const int persistedSampleCount = m_nativeFtSamples + m_manualSamples + m_rttySamples + m_cwSamples + m_cwBootcampSamples;
            const bool enoughStatsTime = (m_lastStatsSaveMs == 0) || (nowMs - m_lastStatsSaveMs >= 15000);
            const bool enoughNewSamples = (persistedSampleCount - m_lastStatsSavedSampleCount) >= 32;

            const int budgetMs = qBound(0, m_trainingBudgetMs, 250);
            qint64 elapsedMs = 0;
            while (budgetMs > 0 && elapsedMs <= budgetMs) {
                bool trainedSomething = false;

                if (!m_cwSamplesQueue.empty() && m_cwNetwork != nullptr) {
                    QVector<ProfileSample> cwBatch;
                    const int take = qMin(64, static_cast<int>(m_cwSamplesQueue.size()));
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

                if (m_samples.size() >= 8) {
                    // Replay buffer: keep training on the persistent FT gold dataset,
                    // not only on samples just collected during the current run.
                    QVector<Sample> batch;
                    const int take = qMin(m_ftBatchSize, static_cast<int>(m_samples.size()));
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
            if ((m_trainingRuns > 0 || m_cwTrainingRuns > 0) &&
                QDateTime::currentMSecsSinceEpoch() - m_lastAutoCheckpointMs >= 5 * 60 * 1000) {
                if (m_network != nullptr) m_network->save(checkpointPath());
                if (m_cwNetwork != nullptr) m_cwNetwork->save(defaultCwCheckpointPath());
                saveStats();
                m_statsDirty = false;
                m_goldDatasetDirty = false;
                m_lastAutoCheckpointMs = QDateTime::currentMSecsSinceEpoch();
                checkpointSaved = true;
            }
            shouldEmit = (ftBatches > 0 || cwBatches > 0 || m_statsDirty);
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
    while (m_samples.size() > kMaxSamples) m_samples.pop_front();
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

QVector<float> DeepDspController::predict(const QVector<float> &input)
{
    if (m_network == nullptr || input.size() != kInputCount) return {};
    return m_network->predict(input);
}

