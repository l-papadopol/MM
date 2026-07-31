#ifndef FTSLOTSCHEDULER_H
#define FTSLOTSCHEDULER_H

#include "Ft8Mode.h"

#include <QObject>
#include <QString>

class QTimer;

/**
 * @brief UTC-locked FT4/FT8 slot scheduler.
 *
 * Separate from MainWindow: it is the single authority for FT4/FT8 slot phase,
 * selected T/R period and pending TX timing.  v2.00 separates PTT pre-arm from
 * audio start so slow CAT/PTT cannot shift the waveform start decision.
 */
class FtSlotScheduler final : public QObject
{
    Q_OBJECT

public:
    explicit FtSlotScheduler(QObject *parent = nullptr);
    ~FtSlotScheduler() override = default;

public slots:
    void startClock();
    void stopClock();
    void configure(const QString &modeName, bool txFirstPeriod);
    void armTransmission(const QString &token,
                         qint64 slotBoundaryUtcMs,
                         int audioTargetDelayMs,
                         int pttLeadMs);
    void cancelTransmission();

signals:
    void slotUpdated(const QString &modeLabel,
                     int slotMs,
                     int cycleMs,
                     bool firstPeriodNow,
                     bool txWindow,
                     int cyclePosMs,
                     int slotElapsedMs,
                     int remainMs,
                     qint64 nowUtcMs);

    void txPttPrearmDue(const QString &token,
                        qint64 slotBoundaryUtcMs,
                        int audioTargetDelayMs,
                        int pttLeadMs,
                        qint64 nowUtcMs);

    void txAudioStartDue(const QString &token,
                         qint64 slotBoundaryUtcMs,
                         int audioTargetDelayMs,
                         int pttLeadMs,
                         qint64 nowUtcMs);

    // Backwards-compatible name kept for older MainWindow branches; v2.00 uses
    // txAudioStartDue instead.
    void txBackendStartDue(const QString &token,
                           qint64 slotBoundaryUtcMs,
                           int audioTargetDelayMs,
                           int pttLeadMs,
                           qint64 nowUtcMs);

    void pendingStateChanged(bool pending, qint64 slotBoundaryUtcMs);

private slots:
    void tick();

private:
    qint64 nowUtcMs() const;
    void emitSlotSnapshot(qint64 nowMs, bool force = false);
    void clearPending();
    void ensureTimer();

    QTimer *m_tickTimer = nullptr;
    QString m_modeName = QStringLiteral("FT8");
    bool m_txFirstPeriod = true;
    bool m_pending = false;
    bool m_prearmEmitted = false;
    bool m_audioStartEmitted = false;
    QString m_pendingToken;
    qint64 m_pendingBoundaryUtcMs = 0;
    int m_pendingAudioTargetDelayMs = 0;
    int m_pendingPttLeadMs = 0;
    qint64 m_lastSlotUiEmitMs = 0;
    int m_lastCyclePosMs = -1;
};

#endif // FTSLOTSCHEDULER_H
