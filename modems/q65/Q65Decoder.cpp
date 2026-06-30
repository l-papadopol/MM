#include "Q65Decoder.h"

#include <QtGlobal>
#include <QtMath>
#include <QStringList>
#include <QDate>
#include <QTime>
#include <QTimeZone>
#include <algorithm>
#include <cmath>
#ifdef MADMODEM_Q65_FULL_MSHV_DECODER
#include "../../third_party/mshv_gpl/port/HvDecoderMs/decoderq65.h"
#include "utils/QtCompat.h"
#endif

namespace {
constexpr int kInternalRate = 12000;
}

Q65Decoder::Q65Decoder(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<Q65Decode>("Q65Decode");
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
    m_periodStartUtc = QDateTime::currentDateTimeUtc();
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
    if (block.samples.isEmpty() || block.sampleRate <= 0) return;
    m_inputSampleRate = block.sampleRate;
    appendResampledTo12k(block);
    const int periodSamples = m_periodSeconds * kInternalRate;
    if (m_samples12k.size() >= periodSamples) {
        tryPeriodDecode(false);
        const int keep = qMin(kInternalRate, m_samples12k.size());
        QVector<double> tail;
        tail.reserve(keep);
        for (int i = m_samples12k.size() - keep; i < m_samples12k.size(); ++i) {
            if (i >= 0) tail.append(m_samples12k.at(i));
        }
        m_samples12k.swap(tail);
        m_periodStartUtc = QDateTime::currentDateTimeUtc();
    }
}

void Q65Decoder::flushPeriod() { tryPeriodDecode(true); }

void Q65Decoder::appendResampledTo12k(const AudioBlock &block)
{
    const int inRate = block.sampleRate;
    if (inRate == kInternalRate) {
        for (float v : block.samples) m_samples12k.append(static_cast<double>(v));
        return;
    }
    const double ratio = static_cast<double>(kInternalRate) / static_cast<double>(inRate);
    const int outCount = qMax(1, qRound(static_cast<double>(block.samples.size()) * ratio));
    const int oldSize = m_samples12k.size();
    m_samples12k.resize(oldSize + outCount);
    for (int i = 0; i < outCount; ++i) {
        const double src = static_cast<double>(i) / ratio;
        const int i0 = qBound(0, static_cast<int>(std::floor(src)), block.samples.size() - 1);
        const int i1 = qBound(0, i0 + 1, block.samples.size() - 1);
        const double frac = src - static_cast<double>(i0);
        const double v = (1.0 - frac) * block.samples.at(i0) + frac * block.samples.at(i1);
        m_samples12k[oldSize + i] = qBound(-1.0, v, 1.0);
    }
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
                QDate date = QDateTime::currentDateTimeUtc().date();
                d.utc = mmqt::makeUtcDateTime(QDate(date.year(), date.month(), date.day()), QTime(hh, mm, ss));
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
        const QString now = QDateTime::currentDateTimeUtc().toString(QStringLiteral("hhmmss"));
        m_mshv->SetStDecode(now, 0, false);
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
