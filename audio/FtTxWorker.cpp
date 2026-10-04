#include "FtTxWorker.h"
#include "TxAudioEngine.h"

#include <memory>
#include <QDateTime>

FtTxWorker::FtTxWorker(QObject *parent)
    : QObject(parent)
{
}

FtTxWorker::~FtTxWorker()
{
    stopOutput();
}

void FtTxWorker::ensureEngine()
{
    if (m_engine != nullptr) {
        return;
    }

    m_engine = new TxAudioEngine(this);

    connect(m_engine, &TxAudioEngine::logMessage, this, &FtTxWorker::logMessage);
    connect(m_engine, &TxAudioEngine::audioBlockReady,
            this, &FtTxWorker::audioBlockReady);
    connect(m_engine, &TxAudioEngine::rttyToneStateChanged,
            this, &FtTxWorker::rttyToneStateChanged);
    connect(m_engine, &TxAudioEngine::progressChanged,
            this, &FtTxWorker::progressChanged);
    connect(m_engine, &TxAudioEngine::started,
            this, &FtTxWorker::handleStarted);
    connect(m_engine, &TxAudioEngine::stopped,
            this, &FtTxWorker::handleStopped);
    connect(m_engine, &TxAudioEngine::finished,
            this, &FtTxWorker::handleFinished);
    connect(m_engine, &TxAudioEngine::errorOccurred,
            this, &FtTxWorker::handleError);
}

void FtTxWorker::startOutput(const QString &deviceName, TxModulator *modulator, qint64 latestStartUtcMs)
{
    std::unique_ptr<TxModulator> owned(modulator);

    if (!owned) {
        emit errorOccurred(m_activeRequest, QStringLiteral("FT TX worker start failed: no modulator."));
        emit stopped(m_activeRequest);
        return;
    }

    ensureEngine();

    if (m_engine == nullptr) {
        emit errorOccurred(m_activeRequest, QStringLiteral("FT TX worker start failed: audio engine unavailable."));
        emit stopped(m_activeRequest);
        return;
    }

    if (m_engine->isRunning()) {
        m_engine->stopOutput();
    }

    emit logMessage(QStringLiteral("FT TX worker: starting dedicated low-latency audio output."));
    const auto request = m_activeRequest;
    m_running = m_engine->startOutput(deviceName, std::move(owned), latestStartUtcMs,
        [this, request]() { return request == m_authorizedRequest.load(); });

}

void FtTxWorker::startScheduledOutput(const QString &deviceName, TxModulator *modulator, qint64 latestStartUtcMs, quint64 requestId)
{
    if (requestId != m_authorizedRequest.load()) {
        delete modulator;
        emit stopped(requestId);
        return;
    }
    // Complete the previous sink under its own id before installing the new id.
    if (m_engine && m_engine->isRunning()) m_engine->stopOutput();
    m_activeRequest = requestId;
    if (latestStartUtcMs > 0 && QDateTime::currentMSecsSinceEpoch() > latestStartUtcMs) {
        delete modulator;
        emit errorOccurred(m_activeRequest, QStringLiteral("FT TX cancelled: worker audio-start deadline expired."));
        return;
    }
    startOutput(deviceName, modulator, latestStartUtcMs);
    if (requestId != m_authorizedRequest.load()) stopOutput();
}

void FtTxWorker::stopOutput()
{
    if (m_engine != nullptr && m_engine->isRunning()) {
        emit logMessage(QStringLiteral("FT TX worker: stop requested."));
        m_engine->stopOutput();
        return;
    }

    if (m_running) {
        m_running = false;
        emit stopped(m_activeRequest);
    }
}

void FtTxWorker::handleStarted()
{
    if (m_activeRequest != m_authorizedRequest.load()) {stopOutput();return;}
    m_running = true;
    emit started(m_activeRequest);
}

void FtTxWorker::handleStopped()
{
    m_running = false;
    emit stopped(m_activeRequest);
}

void FtTxWorker::handleFinished()
{
    emit finished(m_activeRequest);
}

void FtTxWorker::handleError(const QString &message)
{
    m_running = false;
    if (m_activeRequest != m_authorizedRequest.load()) emit stopped(m_activeRequest);
    else emit errorOccurred(m_activeRequest, message);
}
