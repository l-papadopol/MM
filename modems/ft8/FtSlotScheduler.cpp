#include "FtSlotScheduler.h"

#include <QDateTime>
#include <QTimeZone>
#include <QTimer>
#include <QTime>
#include <QtGlobal>
#include "utils/QtCompat.h"

FtSlotScheduler::FtSlotScheduler(QObject *parent)
    : QObject(parent)
{
}

void FtSlotScheduler::ensureTimer()
{
    if (m_tickTimer != nullptr) {
        return;
    }
    m_tickTimer = new QTimer(this);
    m_tickTimer->setTimerType(Qt::PreciseTimer);
    m_tickTimer->setInterval(20);
    connect(m_tickTimer, &QTimer::timeout,
            this, &FtSlotScheduler::tick);
}

void FtSlotScheduler::startClock()
{
    ensureTimer();
    if (m_tickTimer != nullptr && !m_tickTimer->isActive()) {
        m_tickTimer->start();
    }
    emitSlotSnapshot(nowUtcMs(), true);
}

void FtSlotScheduler::stopClock()
{
    if (m_tickTimer != nullptr) {
        m_tickTimer->stop();
    }
}

void FtSlotScheduler::configure(const QString &modeName, bool txFirstPeriod)
{
    const QString key = modeName.trimmed().isEmpty()
        ? QStringLiteral("FT8")
        : modeName.trimmed().toUpper();
    m_modeName = Ft8Mode::isFamilyMode(key) ? key : QStringLiteral("FT8");
    m_txFirstPeriod = txFirstPeriod;
    emitSlotSnapshot(nowUtcMs(), true);
}

void FtSlotScheduler::armTransmission(const QString &token,
                                      qint64 slotBoundaryUtcMs,
                                      int audioTargetDelayMs,
                                      int pttLeadMs)
{
    m_pendingToken = token.trimmed().isEmpty()
        ? QString::number(slotBoundaryUtcMs)
        : token.trimmed();
    m_pendingBoundaryUtcMs = slotBoundaryUtcMs;
    m_pendingAudioTargetDelayMs = qMax(0, audioTargetDelayMs);
    m_pendingPttLeadMs = qBound(0, pttLeadMs, 2000);
    m_pending = (m_pendingBoundaryUtcMs > 0);
    m_prearmEmitted = false;
    m_audioStartEmitted = false;
    emit pendingStateChanged(m_pending, m_pendingBoundaryUtcMs);
    tick();
}

void FtSlotScheduler::clearPending()
{
    m_pending = false;
    m_prearmEmitted = false;
    m_audioStartEmitted = false;
    m_pendingToken.clear();
    m_pendingBoundaryUtcMs = 0;
    m_pendingAudioTargetDelayMs = 0;
    m_pendingPttLeadMs = 0;
}

void FtSlotScheduler::cancelTransmission()
{
    if (!m_pending && m_pendingToken.isEmpty()) {
        return;
    }
    clearPending();
    emit pendingStateChanged(false, 0);
}

qint64 FtSlotScheduler::nowUtcMs() const
{
    return QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
}

void FtSlotScheduler::emitSlotSnapshot(qint64 nowMs, bool force)
{
    const Ft8Mode::Profile profile = Ft8Mode::profileForMode(m_modeName);
    const int slotMs = qMax(1000, profile.slotMs);
    const int cycleMs = qMax(slotMs * 2, profile.cycleMs);

    const QDateTime now = mmqt::fromMSecsSinceEpochUtc(nowMs);
    const QTime t = now.time();
    const int msOfDay = (((t.hour() * 60 + t.minute()) * 60) + t.second()) * 1000 + t.msec();
    const int cyclePosMs = msOfDay % cycleMs;
    const int slotIndexInCycle = qBound(0, cyclePosMs / slotMs, 1);
    const int slotElapsedMs = cyclePosMs - slotIndexInCycle * slotMs;
    const bool firstPeriodNow = (slotIndexInCycle == 0);
    const bool txWindow = (firstPeriodNow == m_txFirstPeriod);
    const int remainMs = qMax(0, slotMs - slotElapsedMs);

    const bool crossedSlot = (m_lastCyclePosMs >= 0 && cyclePosMs < m_lastCyclePosMs);
    if (force || crossedSlot || (nowMs - m_lastSlotUiEmitMs) >= 250) {
        m_lastSlotUiEmitMs = nowMs;
        emit slotUpdated(profile.shortLabel,
                         slotMs,
                         cycleMs,
                         firstPeriodNow,
                         txWindow,
                         cyclePosMs,
                         slotElapsedMs,
                         remainMs,
                         nowMs);
    }
    m_lastCyclePosMs = cyclePosMs;
}

void FtSlotScheduler::tick()
{
    const qint64 nowMs = nowUtcMs();
    emitSlotSnapshot(nowMs, false);

    if (!m_pending || m_pendingBoundaryUtcMs <= 0) {
        return;
    }

    const QString token = m_pendingToken;
    const qint64 boundary = m_pendingBoundaryUtcMs;
    const int audioDelay = m_pendingAudioTargetDelayMs;
    const int pttLead = m_pendingPttLeadMs;
    const qint64 pttDueMs = boundary - static_cast<qint64>(pttLead);
    // WSJT-X Modulator::start() opens the audio stream at the TX slot and
    // emits pre-start silence until the protocol useful-tone delay (FT8 500 ms,
    // FT4 300 ms).  Do the same here: the scheduler wakes the audio path at
    // the UTC boundary; Ft8Transmitter then inserts silence or skips samples
    // according to the exact current time.
    const qint64 audioDueMs = boundary;

    if (!m_prearmEmitted && nowMs >= pttDueMs) {
        m_prearmEmitted = true;
        emit txPttPrearmDue(token, boundary, audioDelay, pttLead, nowMs);
    }

    if (!m_audioStartEmitted && nowMs >= audioDueMs) {
        m_audioStartEmitted = true;
        emit txAudioStartDue(token, boundary, audioDelay, pttLead, nowMs);
        clearPending();
        emit pendingStateChanged(false, 0);
    }
}
