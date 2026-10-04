#include "TxAudioEngine.h"

#include <QByteArray>
#include <QIODevice>
#include <QMetaObject>
#include <QtGlobal>
#include <QtMath>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QMediaDevices>
#else
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QAudioOutput>
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
namespace AudioNs = QtAudio;
#else
namespace AudioNs = QAudio;
#endif

#include <atomic>
#include <algorithm>
#include <cstring>

#include "TxOutputDevice.h"

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

TxAudioEngine::TxAudioEngine(QObject *parent)
    : QObject(parent)
{
    m_outputMonitor.setParent(this);
    m_outputMonitor.setInterval(50);
    connect(&m_outputMonitor, &QTimer::timeout, this, &TxAudioEngine::checkOutputState);
}

TxAudioEngine::~TxAudioEngine()
{
    stopOutput();
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

bool TxAudioEngine::startOutput(const QString &deviceName, std::unique_ptr<TxModulator> modulator,
                                qint64 latestStartUtcMs, std::function<bool()> authorized)
{
    stopOutput();

    if (!modulator) {
        emit errorOccurred("TX start failed: no modulator available.");
        return false;
    }

    m_sampleRate = modulator->sampleRate();
    m_modulator = std::move(modulator);
    m_totalSamples = 0;
    m_finishedEmitted = false;
    m_playbackStarted = false;
    m_lastProcessedUs = 0;
    m_lastProducedSamples = 0;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

    QAudioDevice selectedDevice = QMediaDevices::defaultAudioOutput();
    bool exactDeviceMatch = false;

    const QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
    for (const QAudioDevice &device : devices) {
        if (device.description() == deviceName) {
            selectedDevice = device;
            exactDeviceMatch = true;
            break;
        }
    }

    const bool defaultRequested = deviceName.isEmpty() || deviceName == QStringLiteral("default");
    if (!defaultRequested && !exactDeviceMatch) {
        emit errorOccurred(QStringLiteral("Configured TX audio output '%1' is not available; automatic device fallback is disabled.")
                               .arg(deviceName));
        m_modulator.reset();
        return false;
    }

    QAudioFormat format;
    format.setSampleRate(m_sampleRate);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    if (!selectedDevice.isFormatSupported(format)) {
        emit errorOccurred(QString("Audio output does not support the requested TX format: %1 Hz mono signed 16-bit PCM.")
                               .arg(m_sampleRate));
        m_modulator.reset();
        return false;
    }

    m_audioOutput = new QAudioSink(selectedDevice, format, this);
    const bool lowLatencyTx = (m_modulator && m_modulator->lowLatencyTx());
    m_audioOutput->setBufferSize(lowLatencyTx ? (4096 * 2) : (4096 * 2 * 8));

#else

    QAudioDeviceInfo selectedDevice = QAudioDeviceInfo::defaultOutputDevice();
    bool exactDeviceMatch = false;

    const QList<QAudioDeviceInfo> devices =
        QAudioDeviceInfo::availableDevices(AudioNs::AudioOutput);

    for (const QAudioDeviceInfo &device : devices) {
        if (device.deviceName() == deviceName) {
            selectedDevice = device;
            exactDeviceMatch = true;
            break;
        }
    }

    const bool defaultRequested = deviceName.isEmpty() || deviceName == QStringLiteral("default");
    if (!defaultRequested && !exactDeviceMatch) {
        emit errorOccurred(QStringLiteral("Configured TX audio output '%1' is not available; automatic device fallback is disabled.")
                               .arg(deviceName));
        m_modulator.reset();
        return false;
    }

    QAudioFormat format;
    format.setSampleRate(m_sampleRate);
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    if (!selectedDevice.isFormatSupported(format)) {
        emit errorOccurred(QString("Audio output does not support the requested TX format: %1 Hz mono signed 16-bit PCM.")
                               .arg(m_sampleRate));
        m_modulator.reset();
        return false;
    }

    m_audioOutput = new QAudioOutput(selectedDevice, format, this);
    const bool lowLatencyTx = (m_modulator && m_modulator->lowLatencyTx());
    m_audioOutput->setBufferSize(lowLatencyTx ? (4096 * 2) : (4096 * 2 * 8));

#endif

    TxOutputDevice *device = new TxOutputDevice(m_modulator.get(), m_sampleRate, m_outputVolumePercent, this);

    device->setStartGuard(latestStartUtcMs, std::move(authorized));
    connect(device, &TxOutputDevice::audioBlockReady,
            this, &TxAudioEngine::audioBlockReady);

    connect(device, &TxOutputDevice::rttyToneStateChanged,
            this, &TxAudioEngine::rttyToneStateChanged);

    connect(device, &TxOutputDevice::progressChanged,
            this, &TxAudioEngine::progressChanged);

    connect(device, &TxOutputDevice::finished,
            this, &TxAudioEngine::handleDeviceFinished);

    if (!device->start()) {
        delete device;
        releaseAudioOutput();
        emit errorOccurred("Unable to open TX output device.");
        return false;
    }

    m_outputDevice = device;

    const quint64 generation = ++m_outputGeneration;
    connect(m_audioOutput, & 
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QAudioSink::stateChanged,
#else
            QAudioOutput::stateChanged,
#endif
            this, [this, generation]() {
                if (generation == m_outputGeneration) checkOutputState();
            }, Qt::QueuedConnection);
    m_running = true;
    m_audioOutput->start(m_outputDevice);
    if (m_audioOutput->error() != AudioNs::NoError &&
        m_audioOutput->state() == AudioNs::StoppedState) {
        const int error = static_cast<int>(m_audioOutput->error());
        releaseAudioOutput();
        m_running = false;
        m_modulator.reset();
        emit errorOccurred(QStringLiteral("TX audio backend failed to start (error %1).").arg(error));
        return false;
    }
    // Opening a sink does not prove it has pulled or played any PCM.
    m_outputProgressClock.start();
    m_outputMonitor.start();
    emit logMessage(QStringLiteral("TX audio opened: %1 Hz, mono PCM16, buffer %2 B, source available %3 B, state %4.")
                    .arg(m_sampleRate).arg(m_audioOutput->bufferSize())
                    .arg(device->bytesAvailable()).arg(static_cast<int>(m_audioOutput->state())));
    emit progressChanged(0.0);

    return true;
}

void TxAudioEngine::stopOutput()
{
    if (!m_running && m_audioOutput == nullptr && m_outputDevice == nullptr) {
        return;
    }

    /*
     * Manual stop can race with a queued natural-finish notification from the
     * pull device.  Mark the finish as already handled before destroying the
     * device so a late queued slot cannot emit finished()/stopped() again.
     */
    m_finishedEmitted = true;

    releaseAudioOutput();

    m_running = false;
    m_modulator.reset();

    emit stopped();
}

bool TxAudioEngine::isRunning() const
{
    return m_running;
}

int TxAudioEngine::sampleRate() const
{
    return m_sampleRate;
}

void TxAudioEngine::setOutputVolumePercent(int percent)
{
    m_outputVolumePercent = qBound(0, percent, 100);
    TxOutputDevice *device = qobject_cast<TxOutputDevice *>(m_outputDevice);
    if (device != nullptr) {
        device->setVolumePercent(m_outputVolumePercent);
    }
}

int TxAudioEngine::outputVolumePercent() const
{
    return m_outputVolumePercent;
}

// -----------------------------------------------------------------------------
// Private slots
// -----------------------------------------------------------------------------

void TxAudioEngine::handleDeviceFinished()
{
    // Source EOF only means the last PCM buffer has been handed to Qt.
    // The sink's IdleState is the acknowledgement that its queue drained.
    checkOutputState();
}

void TxAudioEngine::checkOutputState()
{
    if (!m_running || !m_audioOutput || m_finishedEmitted) return;
    const quint64 generation=m_outputGeneration;
    const auto state = m_audioOutput->state();
    const auto error = m_audioOutput->error();
    const auto *source = static_cast<TxOutputDevice *>(m_outputDevice);
    const qint64 produced = source ? source->samplesProduced() : 0;
    const qint64 processed = m_audioOutput->processedUSecs();
    if (source && source->aborted()) {
        m_finishedEmitted = true;
        releaseAudioOutput();
        m_running = false;
        m_modulator.reset();
        emit errorOccurred(QStringLiteral("TX audio cancelled before PCM pull: authorization revoked or start deadline expired."));
        return;
    }
    if (!m_playbackStarted && produced > 0 && processed > 0) {
        m_playbackStarted = true;
        emit logMessage(QStringLiteral("TX audio playback confirmed: %1 samples pulled, %2 us processed.").arg(produced).arg(processed));
        emit started();
        if (!m_running || !m_audioOutput || generation!=m_outputGeneration) return;
    }
    if (produced != m_lastProducedSamples || processed != m_lastProcessedUs) {
        m_lastProducedSamples = produced;
        m_lastProcessedUs = processed;
        m_outputProgressClock.restart();
    }
    if (state == AudioNs::IdleState && source && source->atEnd() &&
        (error == AudioNs::NoError || error == AudioNs::UnderrunError)) {
        m_finishedEmitted = true;
        emit progressChanged(1.0);
        emit finished();
        stopOutput();
        return;
    }
    const bool stalled = m_outputProgressClock.isValid() && m_outputProgressClock.elapsed() > 1000;
    if (state == AudioNs::StoppedState || error != AudioNs::NoError || stalled) {
        m_finishedEmitted = true;
        const int errorCode = static_cast<int>(error);
        releaseAudioOutput();
        m_running = false;
        m_modulator.reset();
        // One terminal event; MainWindow's error handler releases PTT.
        emit errorOccurred(QStringLiteral("TX audio backend %1 (state %2, error %3, pulled %4 samples, processed %5 us).")
                           .arg(stalled ? QStringLiteral("stalled") : QStringLiteral("stopped unexpectedly"))
                           .arg(static_cast<int>(state)).arg(errorCode).arg(produced).arg(processed));
    }
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

void TxAudioEngine::releaseAudioOutput()
{
    ++m_outputGeneration;
    m_outputMonitor.stop();
    if (m_audioOutput != nullptr) {
        disconnect(m_audioOutput, nullptr, this, nullptr);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_audioOutput->stop();
#else
        m_audioOutput->stop();
#endif
        delete m_audioOutput;
        m_audioOutput = nullptr;
    }

    if (m_outputDevice != nullptr) {
        m_outputDevice->close();
        delete m_outputDevice;
        m_outputDevice = nullptr;
    }
}


