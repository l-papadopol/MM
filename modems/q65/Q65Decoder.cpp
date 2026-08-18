#include "Q65Decoder.h"

#include <QtGlobal>

namespace {
constexpr int kInternalRate = 12000;
}

Q65Decoder::Q65Decoder(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<Q65Decode>("Q65Decode");
    m_resampler.configure(kInternalRate);
    reset();
}

Q65Decoder::~Q65Decoder() = default;

void Q65Decoder::setPeriodSeconds(int seconds)
{
    int bounded = 60;
    if (seconds == 15 || seconds == 30 || seconds == 60 || seconds == 120) bounded = seconds;
    if (m_periodSeconds == bounded) return;
    m_periodSeconds = bounded;
    m_rxFrequencyHz = qBound(Q65Mode::minimumBaseToneHz(m_submode, m_periodSeconds), m_rxFrequencyHz,
                             Q65Mode::maximumBaseToneHz(m_submode, m_periodSeconds));
    reset();
}

void Q65Decoder::setDecodeDepth(int depth) { m_decodeDepth = qBound(1, depth, 3); }
void Q65Decoder::setSubmode(Q65Mode::Submode submode)
{
    if (m_submode == submode) return;
    m_submode = submode;
    m_rxFrequencyHz = qBound(Q65Mode::minimumBaseToneHz(m_submode, m_periodSeconds), m_rxFrequencyHz,
                             Q65Mode::maximumBaseToneHz(m_submode, m_periodSeconds));
    m_engine.clearAverages();
}
void Q65Decoder::setRxFrequencyHz(int hz)
{
    m_rxFrequencyHz = qBound(Q65Mode::minimumBaseToneHz(m_submode, m_periodSeconds), hz,
                             Q65Mode::maximumBaseToneHz(m_submode, m_periodSeconds));
}
void Q65Decoder::setDfToleranceHz(int hz) { m_dfToleranceHz = qBound(10, hz, 1000); }
void Q65Decoder::setAveragingEnabled(bool enabled) { m_averaging = enabled; }
void Q65Decoder::setAutoClearAverages(bool enabled) { m_autoClearAverages = enabled; }
void Q65Decoder::setSingleDecode(bool enabled) { m_singleDecode = enabled; }
void Q65Decoder::setApDecodeEnabled(bool enabled) { m_apDecode = enabled; }
void Q65Decoder::setMaxDriftEnabled(bool enabled) { m_maxDrift = enabled; }
void Q65Decoder::setEmeDelayEnabled(bool enabled) { m_emeDelay = enabled; }
void Q65Decoder::setMyCall(const QString &call) { m_myCall = call.trimmed().toUpper(); }
void Q65Decoder::setDxCall(const QString &call) { m_dxCall = call.trimmed().toUpper(); }
void Q65Decoder::setDxGrid(const QString &grid) { m_dxGrid = grid.trimmed().left(4).toUpper(); }

QString Q65Decoder::depthName() const
{
    if (m_decodeDepth <= 1) return QStringLiteral("Fast");
    if (m_decodeDepth == 2) return QStringLiteral("Normal");
    return QStringLiteral("Deep");
}

QString Q65Decoder::submodeName() const { return Q65Mode::modeName(m_submode); }

void Q65Decoder::reset()
{
    m_samples12k.clear();
    m_resampler.reset();
    m_periodStartUtc = QDateTime();
    m_currentPeriodId = -1;
    m_nextOutputUtcNs = 0;
    m_outputTimeRemainder = 0;
    m_lastInputEndUtcNs = 0;
    m_captureGeneration = 0;
    m_periodTimelineValid = false;
    m_engine.clearAverages();
    m_avgUsable = 0;
    m_avgAll = 0;
    m_lastStatus.clear();
    emit averageStatusChanged(m_avgUsable, m_avgAll);
    emit statusChanged(backendStatusText());
}

void Q65Decoder::clearAverages()
{
    m_engine.clearAverages();
    m_avgUsable = 0;
    m_avgAll = 0;
    emit averageStatusChanged(m_avgUsable, m_avgAll);
    emit statusChanged(QStringLiteral("Q65 averages cleared."));
}

QString Q65Decoder::backendStatusText() const
{
    return QStringLiteral("%1 RX: native, %2 s, %3, RX %4 Hz, DF tol ±%5 Hz%6%7%8%9")
        .arg(submodeName())
        .arg(m_periodSeconds)
        .arg(depthName())
        .arg(m_rxFrequencyHz)
        .arg(m_dfToleranceHz)
        .arg(m_averaging ? QStringLiteral(", Avg") : QString())
        .arg(m_apDecode ? QStringLiteral(", AP") : QString())
        .arg(m_maxDrift ? QStringLiteral(", drift") : QString())
        .arg(m_emeDelay ? QStringLiteral(", EME delay") : QString());
}

void Q65Decoder::processAudioBlock(const AudioBlock &block)
{
    if (block.samples.isEmpty() || block.sampleRate <= 0) return;
    m_inputSampleRate = block.sampleRate;
    appendResampledTo12k(block);
}

void Q65Decoder::flushPeriod() { finishUtcPeriod(true); }

void Q65Decoder::appendResampledTo12k(const AudioBlock &block)
{
    qint64 blockStartUtcNs = block.firstSampleUtcNs;
    if (blockStartUtcNs <= 0) {
        const qint64 durationNs = (static_cast<qint64>(block.samples.size()) * 1000000000LL) /
                                 qMax(1, block.sampleRate);
        blockStartUtcNs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() * 1000000LL - durationNs;
    }

    const bool generationChanged = block.captureGeneration != 0 &&
                                   m_captureGeneration != 0 &&
                                   block.captureGeneration != m_captureGeneration;
    const qint64 inputSampleNs = 1000000000LL / qMax(1, block.sampleRate);
    const bool timestampJump = m_lastInputEndUtcNs > 0 &&
                               qAbs(blockStartUtcNs - m_lastInputEndUtcNs) >
                                   qMax<qint64>(qint64{5000000}, inputSampleNs * qint64{4});
    if (generationChanged || timestampJump) {
        m_resampler.reset();
        m_samples12k.clear();
        m_currentPeriodId = -1;
        m_nextOutputUtcNs = 0;
        m_outputTimeRemainder = 0;
        m_periodTimelineValid = false;
        m_engine.clearAverages();
    }
    if (block.captureGeneration != 0) m_captureGeneration = block.captureGeneration;

    const QVector<double> resampled = m_resampler.process(block.samples, block.sampleRate);
    m_lastInputEndUtcNs = blockStartUtcNs +
        (static_cast<qint64>(block.samples.size()) * 1000000000LL) / qMax(1, block.sampleRate);
    if (resampled.isEmpty()) return;

    if (m_nextOutputUtcNs <= 0) {
        m_nextOutputUtcNs = blockStartUtcNs;
        m_outputTimeRemainder = 0;
    }
    const qint64 periodNs = static_cast<qint64>(m_periodSeconds) * 1000000000LL;
    for (double value : resampled) {
        const qint64 periodId = m_nextOutputUtcNs / periodNs;
        if (m_currentPeriodId != periodId) {
            if (m_currentPeriodId >= 0) finishUtcPeriod(false);
            beginUtcPeriod(periodId, m_nextOutputUtcNs);
        }
        m_samples12k.append(qBound(-1.0, value, 1.0));
        m_outputTimeRemainder += 1000000000LL;
        m_nextOutputUtcNs += m_outputTimeRemainder / kInternalRate;
        m_outputTimeRemainder %= kInternalRate;
    }
}

void Q65Decoder::beginUtcPeriod(qint64 periodId, qint64 firstSampleUtcNs)
{
    m_currentPeriodId = periodId;
    const qint64 periodNs = static_cast<qint64>(m_periodSeconds) * 1000000000LL;
    const qint64 periodStartNs = periodId * periodNs;
    m_periodStartUtc = QDateTime::fromMSecsSinceEpoch(periodStartNs / 1000000LL, Qt::UTC);
    m_samples12k.clear();
    const qint64 offsetNs = qMax<qint64>(qint64{0}, firstSampleUtcNs - periodStartNs);
    const int missingSamples = static_cast<int>(qMin<qint64>(
        static_cast<qint64>(m_periodSeconds * kInternalRate),
        (offsetNs * kInternalRate) / 1000000000LL));
    m_periodTimelineValid = missingSamples <= kInternalRate / 20;
    if (missingSamples > 0) m_samples12k.fill(0.0, missingSamples);
}

void Q65Decoder::finishUtcPeriod(bool force)
{
    if (m_currentPeriodId < 0 || m_samples12k.isEmpty()) return;
    const int periodSamples = m_periodSeconds * kInternalRate;
    if (!force && (!m_periodTimelineValid || m_samples12k.size() < periodSamples - 2)) {
        emit statusChanged(QStringLiteral("Q65 period skipped: incomplete or discontinuous UTC audio window."));
        return;
    }
    if (m_samples12k.size() > periodSamples) m_samples12k.resize(periodSamples);
    tryPeriodDecode(force);
}

void Q65Decoder::tryPeriodDecode(bool force)
{
    const int secondsBuffered = m_samples12k.size() / kInternalRate;
    if (!force && secondsBuffered < m_periodSeconds) return;
    emit periodReady(secondsBuffered, m_periodSeconds);

    Q65NativeEngine::Configuration configuration;
    configuration.periodSeconds = m_periodSeconds;
    configuration.decodeDepth = m_decodeDepth;
    configuration.submode = m_submode;
    configuration.rxFrequencyHz = m_rxFrequencyHz;
    configuration.dfToleranceHz = m_dfToleranceHz;
    configuration.averaging = m_averaging;
    configuration.autoClearAverages = m_autoClearAverages;
    configuration.singleDecode = m_singleDecode;
    configuration.apDecode = m_apDecode;
    configuration.maxDrift = m_maxDrift;
    configuration.emeDelay = m_emeDelay;
    configuration.myCall = m_myCall;
    configuration.dxCall = m_dxCall;
    configuration.dxGrid = m_dxGrid;

    const QVector<Q65NativeEngine::Result> results =
        m_engine.decode(m_samples12k, m_currentPeriodId, configuration);
    for (const Q65NativeEngine::Result &result : results) {
        Q65Decode decode;
        decode.utc = m_periodStartUtc.addMSecs(qRound64(result.dtSeconds * 1000.0));
        decode.dtSeconds = result.dtSeconds;
        decode.snrDb = qBound(-50, qRound(result.snrDb), 49);
        decode.frequencyHz = result.frequencyHz;
        decode.dfHz = qRound(result.frequencyHz - m_rxFrequencyHz);
        decode.message = result.message;
        decode.averageCount = result.averageCount;
        decode.submode = submodeName();
        emit decoded(decode);
    }

    m_avgUsable = m_engine.usableAverageCount();
    m_avgAll = m_engine.allAverageCount();
    emit averageStatusChanged(m_avgUsable, m_avgAll);
    const QString status = QStringLiteral("%1 native period: %2/%3 s, %4 decode(s), avg %5/%6")
                               .arg(submodeName())
                               .arg(secondsBuffered)
                               .arg(m_periodSeconds)
                               .arg(results.size())
                               .arg(m_avgUsable)
                               .arg(m_avgAll);
    if (status != m_lastStatus) {
        m_lastStatus = status;
        emit statusChanged(status);
    }
}
