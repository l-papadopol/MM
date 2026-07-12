#include "FtTxWorker.h"
#include "TxAudioEngine.h"

#include <memory>

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

void FtTxWorker::startOutput(const QString &deviceName, TxModulator *modulator)
{
    std::unique_ptr<TxModulator> owned(modulator);

    if (!owned) {
        emit errorOccurred(QStringLiteral("FT TX worker start failed: no modulator."));
        emit stopped();
        return;
    }

    ensureEngine();

    if (m_engine == nullptr) {
        emit errorOccurred(QStringLiteral("FT TX worker start failed: audio engine unavailable."));
        emit stopped();
        return;
    }

    if (m_engine->isRunning()) {
        m_engine->stopOutput();
    }

    emit logMessage(QStringLiteral("FT TX worker: starting dedicated low-latency audio output."));
    m_running = m_engine->startOutput(deviceName, std::move(owned));
    if (!m_running) {
        emit stopped();
    }
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
        emit stopped();
    }
}

void FtTxWorker::handleStarted()
{
    m_running = true;
    emit started();
}

void FtTxWorker::handleStopped()
{
    m_running = false;
    emit stopped();
}

void FtTxWorker::handleFinished()
{
    emit finished();
}

void FtTxWorker::handleError(const QString &message)
{
    emit errorOccurred(message);
}
