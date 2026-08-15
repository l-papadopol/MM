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

#include <algorithm>
#include <cstring>

/**
 * @brief Pull-device used by Qt audio output to read generated PCM samples.
 */
class TxOutputDevice final : public QIODevice
{
    Q_OBJECT

public:
    /**
     * @brief Creates a pull-device connected to one modulator.
     */
    TxOutputDevice(TxModulator *modulator,
                   int sampleRate,
                   int volumePercent,
                   QObject *parent = nullptr)
        : QIODevice(parent),
          m_modulator(modulator),
          m_sampleRate(sampleRate),
          m_volumePercent(qBound(0, volumePercent, 100))
    {
    }

    /**
     * @brief Returns sequential device status.
     */
    bool isSequential() const override
    {
        return true;
    }

    /**
     * @brief Opens the device for audio-output reads.
     */
    bool start()
    {
        m_totalSamples = 0;
        m_finishQueued = false;
        return open(QIODevice::ReadOnly);
    }

    void setVolumePercent(int percent)
    {
        m_volumePercent = qBound(0, percent, 100);
    }

signals:
    void audioBlockReady(const AudioBlock &block);
    void rttyToneStateChanged(bool mark, double progress);
    void progressChanged(double progress);
    void finished();

protected:
    /**
     * @brief Provides signed 16-bit little-endian PCM to Qt audio output.
     */
    qint64 readData(char *data, qint64 maxSize) override
    {
        if (data == nullptr || maxSize <= 0 || m_modulator == nullptr) {
            return 0;
        }

        const qint64 alignedBytes = maxSize - (maxSize % 2);
        const int requestedSamples = static_cast<int>(alignedBytes / 2);

        if (requestedSamples <= 0) {
            return 0;
        }

        QVector<float> samples(requestedSamples, 0.0f);
        int generatedSamples = 0;

        if (!m_modulator->isFinished()) {
            generatedSamples = m_modulator->generate(samples.data(), requestedSamples);
            generatedSamples = qBound(0, generatedSamples, requestedSamples);
        }

        /*
         * Do not stop the Qt audio output immediately after the last generated
         * sample.  Some backends still have the final pull buffer in flight; an
         * immediate stop can audibly truncate the last Hell/RTTY/BPSK symbols.
         * Once the modulator ends, keep returning a short run of silence before
         * emitting finished().
         */
        if (m_modulator->isFinished() && !m_finishQueued && m_tailSamplesRemaining < 0) {
            const int requestedTail = m_modulator->trailingSilenceSamples();
            m_tailSamplesRemaining = (requestedTail >= 0) ? requestedTail : qMax(1, m_sampleRate / 3);
        }

        if (m_tailSamplesRemaining > 0) {
            m_tailSamplesRemaining -= requestedSamples;
        }

        qint16 *pcm = reinterpret_cast<qint16 *>(data);

        for (int i = 0; i < requestedSamples; ++i) {
            const double gain = static_cast<double>(qBound(0, m_volumePercent, 100)) / 100.0;
            const double bounded = qBound(-1.0, static_cast<double>(samples.at(i)) * gain, 1.0);
            pcm[i] = static_cast<qint16>(qRound(bounded * 32767.0));
        }

        if (generatedSamples > 0) {
            AudioBlock block;
            block.sampleRate = m_sampleRate;
            block.firstSampleIndex = m_totalSamples;
            block.samples = samples.mid(0, generatedSamples);
            m_totalSamples += generatedSamples;

            emit audioBlockReady(block);
            const double progress = m_modulator->progress();
            emit progressChanged(progress);

            m_samplesSinceRttyStateEmit += generatedSamples;
            const int stateEmitInterval = qMax(1, m_sampleRate / 15);
            if (m_samplesSinceRttyStateEmit >= stateEmitInterval) {
                m_samplesSinceRttyStateEmit = 0;
                bool markState = true;
                if (m_modulator->rttyToneState(&markState)) {
                    emit rttyToneStateChanged(markState, progress);
                }
            }
        }

        if (m_modulator->isFinished() &&
            m_tailSamplesRemaining <= 0 &&
            !m_finishQueued) {
            m_finishQueued = true;
            QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
        }

        return alignedBytes;
    }

    /**
     * @brief Rejects writes because this is a read-only source device.
     */
    qint64 writeData(const char *data, qint64 maxSize) override
    {
        Q_UNUSED(data)
        Q_UNUSED(maxSize)
        return -1;
    }

private:
    TxModulator *m_modulator = nullptr;
    int m_sampleRate = 48000;
    qint64 m_totalSamples = 0;
    int m_samplesSinceRttyStateEmit = 0;
    bool m_finishQueued = false;
    int m_tailSamplesRemaining = -1;
    int m_volumePercent = 100;
};

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

TxAudioEngine::TxAudioEngine(QObject *parent)
    : QObject(parent)
{
}

TxAudioEngine::~TxAudioEngine()
{
    stopOutput();
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

bool TxAudioEngine::startOutput(const QString &deviceName, std::unique_ptr<TxModulator> modulator)
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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

    QAudioDevice selectedDevice = QMediaDevices::defaultAudioOutput();

    const QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
    for (const QAudioDevice &device : devices) {
        if (device.description() == deviceName) {
            selectedDevice = device;
            break;
        }
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

    const QList<QAudioDeviceInfo> devices =
        QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);

    for (const QAudioDeviceInfo &device : devices) {
        if (device.deviceName() == deviceName) {
            selectedDevice = device;
            break;
        }
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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_audioOutput->start(m_outputDevice);
#else
    m_audioOutput->start(m_outputDevice);
#endif

    m_running = true;
    emit started();
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
    if (m_finishedEmitted) {
        return;
    }

    m_finishedEmitted = true;
    emit progressChanged(1.0);
    emit finished();
    stopOutput();
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

void TxAudioEngine::releaseAudioOutput()
{
    if (m_audioOutput != nullptr) {
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

#include "TxAudioEngine.moc"
