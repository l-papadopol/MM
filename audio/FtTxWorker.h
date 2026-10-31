#ifndef FTTXWORKER_H
#define FTTXWORKER_H

#include "AudioBlock.h"
#include "../core/tx/TxModulator.h"

#include <QObject>
#include <atomic>
#include <QString>

class TxAudioEngine;

Q_DECLARE_METATYPE(TxModulator *)

/**
 * @brief Dedicated worker for time-critical FT4/FT8 transmit audio.
 *
 * The object lives in its own QThread.  It owns a TxAudioEngine created in that
 * thread, so FT slot transmit start/stop and Qt audio pulls do not depend on
 * main-window repaint, map, logbook or CAT UI work.  The main thread passes an
 * already prepared TxModulator pointer by queued signal; ownership transfers to
 * the worker immediately.
 */
class FtTxWorker final : public QObject
{
    Q_OBJECT

public:
    explicit FtTxWorker(QObject *parent = nullptr);
    ~FtTxWorker() override;
    // Thread-safe revocation: STOP takes effect even before queued work runs.
    quint64 newRequest() { return ++m_authorizedRequest; }
    void cancelPendingStart() { ++m_authorizedRequest; }

public slots:
    void startOutput(const QString &deviceName, TxModulator *modulator, qint64 latestStartUtcMs = 0);
    void startScheduledOutput(const QString &deviceName, TxModulator *modulator, qint64 latestStartUtcMs, quint64 requestId);
    void stopOutput();

signals:
    void audioBlockReady(const AudioBlock &block);
    void rttyToneStateChanged(bool mark, double progress);
    void progressChanged(double progress);
    void started(quint64 requestId);
    void stopped(quint64 requestId);
    void finished(quint64 requestId);
    void errorOccurred(quint64 requestId, const QString &message);
    void logMessage(const QString &message);

private slots:
    void handleStarted();
    void handleStopped();
    void handleFinished();
    void handleError(const QString &message);

private:
    void ensureEngine();

private:
    TxAudioEngine *m_engine = nullptr;
    bool m_running = false;
    std::atomic<quint64> m_authorizedRequest{0};
    quint64 m_activeRequest = 0;
};

#endif // FTTXWORKER_H
