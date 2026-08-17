#include "Q65Decoder.h"

#include <QtGlobal>
#include <QtMath>
#include <QStringList>
#include <QDate>
#include <QTime>
#include <algorithm>
#include <cmath>
#ifdef MADMODEM_Q65_FULL_MSHV_DECODER
#include "../../third_party/mshv_gpl/port/HvDecoderMs/decoderq65.h"
#endif

namespace {
constexpr int kInternalRate = 12000;
}

Q65Decoder::Q65Decoder(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<Q65Decode>("Q65Decode");
    m_resampler.configure(kInternalRate);
    ensureMshvBackend();
    reset();
}


Q65Decoder::~Q65Decoder()
{
#ifdef MADMODEM_Q65_FULL_MSHV_DECODER
    delete m_mshv;
    m_mshv = nullptr;
#endif
}

bool Q65Decoder::fullRxAvailable()
{
#ifdef MADMODEM_Q65_FULL_MSHV_DECODER
    return true;
#else
    return false;
#endif
}

void Q65Decoder::setPeriodSeconds(int seconds)
{
    int bounded = 60;
    if (seconds == 15 || seconds == 30 || seconds == 60 || seconds == 120) bounded = seconds;
    if (m_periodSeconds == bounded) return;
    m_periodSeconds = bounded;
    reset();
}

void Q65Decoder::setDecodeDepth(int depth) { m_decodeDepth = qBound(1, depth, 3); configureMshvBackend(); }
void Q65Decoder::setSubmode(Q65Mode::Submode submode) { m_submode = submode; configureMshvBackend(); }
void Q65Decoder::setRxFrequencyHz(int hz) { m_rxFrequencyHz = qBound(300, hz, 2700); }
void Q65Decoder::setDfToleranceHz(int hz) { m_dfToleranceHz = qBound(10, hz, 1000); }
void Q65Decoder::setAveragingEnabled(bool enabled) { m_averaging = enabled; configureMshvBackend(); }
void Q65Decoder::setAutoClearAverages(bool enabled) { m_autoClearAverages = enabled; configureMshvBackend(); }
void Q65Decoder::setSingleDecode(bool enabled) { m_singleDecode = enabled; configureMshvBackend(); }
void Q65Decoder::setApDecodeEnabled(bool enabled) { m_apDecode = enabled; configureMshvBackend(); }
void Q65Decoder::setMaxDriftEnabled(bool enabled) { m_maxDrift = enabled; configureMshvBackend(); }
void Q65Decoder::setEmeDelayEnabled(bool enabled) { m_emeDelay = enabled; configureMshvBackend(); }
void Q65Decoder::setMyCall(const QString &call) { m_myCall = call.trimmed().toUpper(); configureMshvBackend(); }
void Q65Decoder::setDxCall(const QString &call) { m_dxCall = call.trimmed().toUpper(); configureMshvBackend(); }
void Q65Decoder::setDxGrid(const QString &grid) { m_dxGrid = grid.trimmed().left(4).toUpper(); configureMshvBackend(); }

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
    m_avgUsable = 0;
    m_avgAll = 0;
    m_lastStatus.clear();
    emit averageStatusChanged(m_avgUsable, m_avgAll);
    emit statusChanged(backendStatusText());
}

void Q65Decoder::clearAverages()
{
    m_avgUsable = 0;
    m_avgAll = 0;
#ifdef MADMODEM_Q65_FULL_MSHV_DECODER
    if (m_mshv) m_mshv->SetClearAvgQ65();
#endif
    emit averageStatusChanged(m_avgUsable, m_avgAll);
    emit statusChanged(QStringLiteral("Q65 averages cleared."));
}

QString Q65Decoder::backendStatusText() const
{
    if (!fullRxAvailable()) {
        return QStringLiteral("Q65 RX unavailable: build with the FFTW-backed MSHV decoder.");
    }
    return QStringLiteral("%1 RX: %2 s, %3, RX %4 Hz, DF tol ±%5 Hz%6%7%8%9")
        .arg(submodeName())
        .arg(m_periodSeconds)
        .arg(depthName())
        .arg(m_rxFrequencyHz)
        .arg(m_dfToleranceHz)
        .arg(m_averaging ? QStringLiteral(", Avg") : QString())
        .arg(m_apDecode ? QStringLiteral(", AP") : QString())
        .arg(m_maxDrift ? QStringLiteral(", max drift") : QString())
        .arg(m_emeDelay ? QStringLiteral(", EME delay") : QString());
}

void Q65Decoder::processAudioBlock(const AudioBlock &block)
{
    if (!fullRxAvailable()) return;
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
                               qAbs(blockStartUtcNs - m_lastInputEndUtcNs) > qMax<qint64>(qint64{5000000}, inputSampleNs * qint64{4});
    if (generationChanged || timestampJump) {
        m_resampler.reset();
        m_samples12k.clear();
        m_currentPeriodId = -1;
        m_nextOutputUtcNs = 0;
        m_outputTimeRemainder = 0;
        m_periodTimelineValid = false;
    }
    if (block.captureGeneration != 0) {
        m_captureGeneration = block.captureGeneration;
    }

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


void Q65Decoder::ensureMshvBackend()
{
#ifdef MADMODEM_Q65_FULL_MSHV_DECODER
    if (m_mshv) return;
    m_mshv = new DecoderQ65(QString());
    connect(m_mshv, &DecoderQ65::EmitDecodetText, this, [this](const QStringList &list) {
        handleMshvDecodeList(list);
    });
    connect(m_mshv, &DecoderQ65::EmitAvgSavesQ65, this, [this](int usable, int all) {
        m_avgUsable = usable;
        m_avgAll = all;
        emit averageStatusChanged(m_avgUsable, m_avgAll);
    });
    configureMshvBackend();
#endif
}

void Q65Decoder::configureMshvBackend()
{
#ifdef MADMODEM_Q65_FULL_MSHV_DECODER
    if (!m_mshv) return;
    m_mshv->SetPeriod(m_periodSeconds);
    m_mshv->SetStDecoderDeep(m_decodeDepth);
    m_mshv->AvgDecodeChanged(m_averaging);
    m_mshv->AutoClrAvgChanged(m_autoClearAverages);
    m_mshv->SetSingleDecQ65(m_singleDecode);
    m_mshv->SetStApDecode(m_apDecode);
    m_mshv->SetMaxDrift(m_maxDrift);
    m_mshv->SetDecAftEMEDelay(m_emeDelay);
    m_mshv->SetTxFreq(static_cast<double>(m_rxFrequencyHz));
    const QString my = m_myCall.isEmpty() ? QStringLiteral("MYCALL") : m_myCall;
    m_mshv->SetStWords(my, my, 0, 0, QStringLiteral("CQ"));
    m_mshv->SetStHisCallGrid(m_dxCall, m_dxGrid);
#endif
}

void Q65Decoder::handleMshvDecodeList(const QStringList &list)
{
    // MSHV DecoderQ65 PrintMsg format:
    // 0 UTC, 1 SNR, 2 DT, 3 DF, 4 message, 5 decode id, 6 info, 7 frequency.
    if (list.size() < 5) return;
    Q65Decode d;
    d.utc = m_periodStartUtc;
    const QString tmm = list.value(0).trimmed();
    if (tmm.size() >= 6) {
        bool ok = false;
        const int hh = tmm.mid(0, 2).toInt(&ok);
        if (ok) {
            const int mm = tmm.mid(2, 2).toInt(&ok);
            const int ss = tmm.mid(4, 2).toInt(&ok);
            if (ok) {
                const QDate baseDate = m_periodStartUtc.isValid()
                    ? m_periodStartUtc.toUTC().date()
                    : QDateTime::currentDateTimeUtc().date();
                QDateTime candidate(baseDate, QTime(hh, mm, ss), Qt::UTC);
                if (m_periodStartUtc.isValid() && candidate.secsTo(m_periodStartUtc) > 12 * 3600) {
                    candidate = candidate.addDays(1);
                } else if (m_periodStartUtc.isValid() && m_periodStartUtc.secsTo(candidate) > 12 * 3600) {
                    candidate = candidate.addDays(-1);
                }
                d.utc = candidate;
            }
        }
    }
    d.snrDb = list.value(1).toInt();
    d.dtSeconds = list.value(2).toDouble();
    d.dfHz = list.value(3).toInt();
    d.message = list.value(4).trimmed();
    d.averageCount = m_avgUsable;
    d.submode = submodeName();
    bool freqOk = false;
    const int f = list.value(7).toInt(&freqOk);
    d.frequencyHz = freqOk ? f : (m_rxFrequencyHz + d.dfHz);
    if (!d.message.isEmpty()) {
        emit decoded(d);
    }
}

void Q65Decoder::tryPeriodDecode(bool force)
{
    const int secondsBuffered = m_samples12k.size() / kInternalRate;
    if (!force && secondsBuffered < m_periodSeconds) return;
    emit periodReady(secondsBuffered, m_periodSeconds);

bool haveDecode = false;
#ifdef MADMODEM_Q65_FULL_MSHV_DECODER
    ensureMshvBackend();
    configureMshvBackend();
    QVector<double> work;
    const int periodSamples = qMin(m_periodSeconds * kInternalRate, m_samples12k.size());
    work.resize(periodSamples);
    for (int i = 0; i < periodSamples; ++i) work[i] = m_samples12k.at(i);
    if (!work.isEmpty() && m_mshv) {
        const QString periodTime = (m_periodStartUtc.isValid() ? m_periodStartUtc : QDateTime::currentDateTimeUtc())
                                       .toUTC().toString(QStringLiteral("hhmmss"));
        m_mshv->SetStDecode(periodTime, 0, false);
        const int modeId = 14 + static_cast<int>(m_submode);
        const double fa = qMax(0, m_rxFrequencyHz - m_dfToleranceHz);
        const double fb = qMin(3000, m_rxFrequencyHz + m_dfToleranceHz);
        m_mshv->q65_decode(work.data(), fa, fb, static_cast<double>(m_rxFrequencyHz), modeId, haveDecode);
    }
#else
    Q_UNUSED(force);
#endif

    if (!haveDecode) {
        ++m_avgAll;
        if (m_averaging) ++m_avgUsable;
        emit averageStatusChanged(m_avgUsable, m_avgAll);
    }
    const QString status = QStringLiteral("%1 period decoded: %2/%3 s%4")
                               .arg(submodeName())
                               .arg(secondsBuffered)
                               .arg(m_periodSeconds)
                               .arg(haveDecode ? QStringLiteral("; MSHV decode") : QStringLiteral("; no decode"));
    if (status != m_lastStatus) {
        m_lastStatus = status;
        emit statusChanged(status);
    }
}
