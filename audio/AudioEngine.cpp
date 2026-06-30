#include "AudioEngine.h"

#include <QIODevice>
#include <QtMath>

#include <cstring>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QMediaDevices>
#else
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QAudioInput>
#endif

// -----------------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------------

AudioEngine::AudioEngine(QObject *parent)
    : QObject(parent)
{
}

AudioEngine::~AudioEngine()
{
    stopInput();
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

bool AudioEngine::startInput(const QString &deviceName, int requestedSampleRate)
{
    stopInput();

    m_sampleRate = (requestedSampleRate == 44100 || requestedSampleRate == 48000 || requestedSampleRate == 96000)
                       ? requestedSampleRate
                       : 48000;
    m_channelCount = 1;
    m_totalSamples = 0;
    m_pendingBytes.clear();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

    QAudioDevice selectedDevice = QMediaDevices::defaultAudioInput();

    const QList<QAudioDevice> devices = QMediaDevices::audioInputs();
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
        QAudioFormat preferred = selectedDevice.preferredFormat();

        if (preferred.sampleFormat() != QAudioFormat::Int16) {
            emit errorOccurred("Audio input does not support signed 16-bit PCM.");
            return false;
        }

        format = preferred;
    }

    if (format.channelCount() < 1) {
        emit errorOccurred("Invalid audio input channel count.");
        return false;
    }

    m_sampleRate = format.sampleRate();
    m_channelCount = format.channelCount();

    m_audioInput = new QAudioSource(selectedDevice, format, this);
    m_audioInput->setBufferSize(m_blockSamples * m_channelCount * 2 * 4);

    m_inputDevice = m_audioInput->start();

#else

    QAudioDeviceInfo selectedDevice = QAudioDeviceInfo::defaultInputDevice();

    const QList<QAudioDeviceInfo> devices =
        QAudioDeviceInfo::availableDevices(QAudio::AudioInput);

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
        format = selectedDevice.nearestFormat(format);
    }

    if (format.sampleSize() != 16 ||
        format.sampleType() != QAudioFormat::SignedInt ||
        format.channelCount() < 1) {
        emit errorOccurred("Audio input does not support signed 16-bit PCM.");
        return false;
    }

    m_sampleRate = format.sampleRate();
    m_channelCount = format.channelCount();

    m_audioInput = new QAudioInput(selectedDevice, format, this);
    m_audioInput->setBufferSize(m_blockSamples * m_channelCount * 2 * 4);

    m_inputDevice = m_audioInput->start();

#endif

    if (m_inputDevice == nullptr) {
        releaseAudioInput();
        emit errorOccurred("Unable to start audio input.");
        return false;
    }

    connect(m_inputDevice, &QIODevice::readyRead,
            this, &AudioEngine::readInputData);

    m_running = true;

    emit started();

    return true;
}

void AudioEngine::stopInput()
{
    if (!m_running && m_audioInput == nullptr) {
        return;
    }

    releaseAudioInput();

    m_pendingBytes.clear();
    m_inputDevice = nullptr;
    m_running = false;

    emit stopped();
}

bool AudioEngine::isRunning() const
{
    return m_running;
}

int AudioEngine::sampleRate() const
{
    const double corrected = static_cast<double>(m_sampleRate) *
                             (1.0 + m_clockCorrectionPpm / 1000000.0);
    return qMax(1, static_cast<int>(qRound(corrected)));
}

void AudioEngine::setClockCorrectionPpm(double ppm)
{
    if (!qIsFinite(ppm) || ppm < -5000.0 || ppm > 5000.0) {
        m_clockCorrectionPpm = 0.0;
        return;
    }

    m_clockCorrectionPpm = ppm;
}

double AudioEngine::clockCorrectionPpm() const
{
    return m_clockCorrectionPpm;
}

void AudioEngine::setInputVolumePercent(int percent)
{
    m_inputVolumePercent = qBound(0, percent, 100);
}

int AudioEngine::inputVolumePercent() const
{
    return m_inputVolumePercent;
}

// -----------------------------------------------------------------------------
// Input processing
// -----------------------------------------------------------------------------

void AudioEngine::readInputData()
{
    if (m_inputDevice == nullptr) {
        return;
    }

    const QByteArray data = m_inputDevice->readAll();

    if (data.isEmpty()) {
        return;
    }

    m_pendingBytes.append(data);
    processPendingBytes();
}

void AudioEngine::processPendingBytes()
{
    const int bytesPerSample = 2;
    const int bytesPerFrame = bytesPerSample * m_channelCount;
    const int bytesPerBlock = bytesPerFrame * m_blockSamples;

    while (m_pendingBytes.size() >= bytesPerBlock) {
        AudioBlock block;
        block.sampleRate = sampleRate();
        block.firstSampleIndex = m_totalSamples;
        block.samples.resize(m_blockSamples);

        const char *raw = m_pendingBytes.constData();

        for (int i = 0; i < m_blockSamples; ++i) {
            const char *frame = raw + (i * bytesPerFrame);

            qint16 sample = 0;
            memcpy(&sample, frame, sizeof(qint16));

            const float gain = static_cast<float>(qBound(0, m_inputVolumePercent, 100)) / 100.0f;
            block.samples[i] = (static_cast<float>(sample) / 32768.0f) * gain;
        }

        m_pendingBytes.remove(0, bytesPerBlock);
        m_totalSamples += m_blockSamples;

        emitLevel(block.samples);
        emit audioBlockReady(block);
    }
}

void AudioEngine::emitLevel(const QVector<float> &samples)
{
    if (samples.isEmpty()) {
        emit levelChanged(0, -120.0, 0.0);
        return;
    }

    double sumSquares = 0.0;

    for (float sample : samples) {
        sumSquares += static_cast<double>(sample) * static_cast<double>(sample);
    }

    const double rms = qSqrt(sumSquares / static_cast<double>(samples.size()));
    const double db = 20.0 * qLn(qMax(rms, 1.0e-9)) / qLn(10.0);

    const double normalized = (db + 60.0) / 60.0;
    const int percent = qBound(0, static_cast<int>(normalized * 100.0), 100);

    emit levelChanged(percent, db, rms);
}

// -----------------------------------------------------------------------------
// Resource management
// -----------------------------------------------------------------------------

void AudioEngine::releaseAudioInput()
{
    if (m_audioInput == nullptr) {
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_audioInput->stop();
#else
    m_audioInput->stop();
#endif

    delete m_audioInput;
    m_audioInput = nullptr;
    m_inputDevice = nullptr;
}
