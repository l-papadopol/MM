#include "AudioEngine.h"

#include <QDateTime>
#include <QDebug>
#include <QIODevice>
#include <QStringList>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

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

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
namespace AudioNs = QtAudio;
#else
namespace AudioNs = QAudio;
#endif

namespace {

QString audioStateName(AudioNs::State state)
{
    switch (state) {
    case AudioNs::ActiveState:
        return QStringLiteral("Active");
    case AudioNs::SuspendedState:
        return QStringLiteral("Suspended");
    case AudioNs::StoppedState:
        return QStringLiteral("Stopped");
    case AudioNs::IdleState:
        return QStringLiteral("Idle");
    }
    return QStringLiteral("Unknown");
}

QString audioErrorName(AudioNs::Error error)
{
    switch (error) {
    case AudioNs::NoError:
        return QStringLiteral("NoError");
    case AudioNs::OpenError:
        return QStringLiteral("OpenError");
    case AudioNs::IOError:
        return QStringLiteral("IOError");
    case AudioNs::UnderrunError:
        return QStringLiteral("UnderrunError");
    case AudioNs::FatalError:
        return QStringLiteral("FatalError");
    }
    return QStringLiteral("UnknownError");
}

double linearToDbfs(double value)
{
    return 20.0 * std::log10(std::max(value, 1.0e-9));
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QString qt6SampleFormatName(QAudioFormat::SampleFormat sampleFormat)
{
    switch (sampleFormat) {
    case QAudioFormat::UInt8:
        return QStringLiteral("UInt8");
    case QAudioFormat::Int16:
        return QStringLiteral("Int16");
    case QAudioFormat::Int32:
        return QStringLiteral("Int32");
    case QAudioFormat::Float:
        return QStringLiteral("Float");
    case QAudioFormat::Unknown:
        return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}
#else
QString qt5SampleTypeName(QAudioFormat::SampleType sampleType)
{
    switch (sampleType) {
    case QAudioFormat::SignedInt:
        return QStringLiteral("SignedInt");
    case QAudioFormat::UnSignedInt:
        return QStringLiteral("UnsignedInt");
    case QAudioFormat::Float:
        return QStringLiteral("Float");
    case QAudioFormat::Unknown:
        return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

QString qt5ByteOrderName(QAudioFormat::Endian byteOrder)
{
    return byteOrder == QAudioFormat::LittleEndian
        ? QStringLiteral("LittleEndian")
        : QStringLiteral("BigEndian");
}
#endif

} // namespace

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

    m_sampleRate.store((requestedSampleRate == 44100 || requestedSampleRate == 48000 || requestedSampleRate == 96000)
                           ? requestedSampleRate
                           : 48000,
                       std::memory_order_relaxed);
    m_channelCount = 1;
    m_totalSamples = 0;
    m_pendingBytes.clear();
    m_streamFirstUtcNs = 0;
    m_streamFirstMonotonicNs = 0;
    m_streamTimestampValid = false;
    m_captureSequence = 0;
    ++m_captureGeneration;
    if (m_captureGeneration == 0) {
        ++m_captureGeneration;
    }
    m_captureClock.start();
    resetDiagnosticCounters();

    reportDiagnostic(QStringLiteral("Audio diagnostic: requested backend=\"%1\", requested format=%2 Hz, mono, signed 16-bit PCM.")
                         .arg(deviceName.isEmpty() ? QStringLiteral("<empty/default>") : deviceName)
                         .arg(m_sampleRate.load(std::memory_order_relaxed)));

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

    QAudioDevice selectedDevice = QMediaDevices::defaultAudioInput();
    bool exactDeviceMatch = false;

    const QList<QAudioDevice> devices = QMediaDevices::audioInputs();
    for (const QAudioDevice &device : devices) {
        if (device.description() == deviceName) {
            selectedDevice = device;
            exactDeviceMatch = true;
            break;
        }
    }

    const bool defaultRequested = deviceName.isEmpty() || deviceName == QStringLiteral("default");
    if (!defaultRequested && !exactDeviceMatch) {
        reportDiagnostic(QStringLiteral("Audio diagnostic: requested input backend was not found; automatic device fallback is disabled."));
        emit errorOccurred(QStringLiteral("Configured audio input '%1' is not available.").arg(deviceName));
        return false;
    }
    reportDiagnostic(QStringLiteral("Audio diagnostic: selected backend=\"%1\" (%2); available inputs=%3.")
                         .arg(selectedDevice.description())
                         .arg(exactDeviceMatch ? QStringLiteral("exact match")
                                               : (defaultRequested ? QStringLiteral("default requested")
                                                                   : QStringLiteral("requested backend not found; default fallback")))
                         .arg(devices.size()));

    QAudioFormat format;
    format.setSampleRate(m_sampleRate.load(std::memory_order_relaxed));
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    if (!selectedDevice.isFormatSupported(format)) {
        const QAudioFormat preferred = selectedDevice.preferredFormat();
        reportDiagnostic(QStringLiteral("Audio diagnostic: requested mono Int16 format unsupported; Qt6 preferred-format fallback requested."));

        if (preferred.sampleFormat() != QAudioFormat::Int16) {
            reportDiagnostic(QStringLiteral("Audio diagnostic: rejected preferred format %1 Hz, %2 channel(s), %3.")
                                 .arg(preferred.sampleRate())
                                 .arg(preferred.channelCount())
                                 .arg(qt6SampleFormatName(preferred.sampleFormat())));
            emit errorOccurred("Audio input does not support signed 16-bit PCM.");
            return false;
        }

        format = preferred;
    }

    if (format.channelCount() < 1) {
        emit errorOccurred("Invalid audio input channel count.");
        return false;
    }

    m_sampleRate.store(format.sampleRate(), std::memory_order_relaxed);
    m_channelCount = format.channelCount();
    resetDiagnosticCounters();

    reportDiagnostic(QStringLiteral("Audio diagnostic: negotiated format=%1 Hz, %2 channel(s), %3/native-endian; converter remains channel 1 only.")
                         .arg(format.sampleRate())
                         .arg(format.channelCount())
                         .arg(qt6SampleFormatName(format.sampleFormat())));

    m_audioInput = new QAudioSource(selectedDevice, format, this);
    connect(m_audioInput, &QAudioSource::stateChanged,
            this, [this](AudioNs::State state) {
                const AudioNs::Error error = m_audioInput != nullptr ? m_audioInput->error() : AudioNs::NoError;
                reportDiagnostic(QStringLiteral("Audio diagnostic: backend state=%1, error=%2.")
                                     .arg(audioStateName(state), audioErrorName(error)));
            });
    m_audioInput->setBufferSize(m_blockSamples * m_channelCount * 2 * 4);

    m_inputDevice = m_audioInput->start();

#else

    QAudioDeviceInfo selectedDevice = QAudioDeviceInfo::defaultInputDevice();
    bool exactDeviceMatch = false;

    const QList<QAudioDeviceInfo> devices =
        QAudioDeviceInfo::availableDevices(QAudio::AudioInput);

    for (const QAudioDeviceInfo &device : devices) {
        if (device.deviceName() == deviceName) {
            selectedDevice = device;
            exactDeviceMatch = true;
            break;
        }
    }

    const bool defaultRequested = deviceName.isEmpty() || deviceName == QStringLiteral("default");
    if (!defaultRequested && !exactDeviceMatch) {
        reportDiagnostic(QStringLiteral("Audio diagnostic: requested input backend was not found; automatic device fallback is disabled."));
        emit errorOccurred(QStringLiteral("Configured audio input '%1' is not available.").arg(deviceName));
        return false;
    }
    reportDiagnostic(QStringLiteral("Audio diagnostic: selected backend=\"%1\" (%2); available inputs=%3.")
                         .arg(selectedDevice.deviceName())
                         .arg(exactDeviceMatch ? QStringLiteral("exact match")
                                               : (defaultRequested ? QStringLiteral("default requested")
                                                                   : QStringLiteral("requested backend not found; default fallback")))
                         .arg(devices.size()));

    QAudioFormat format;
    format.setSampleRate(m_sampleRate.load(std::memory_order_relaxed));
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);

    if (!selectedDevice.isFormatSupported(format)) {
        const QAudioFormat nearest = selectedDevice.nearestFormat(format);
        reportDiagnostic(QStringLiteral("Audio diagnostic: requested mono S16LE format unsupported; Qt5 nearest-format fallback requested."));
        format = nearest;
    }

    if (format.sampleSize() != 16 ||
        format.sampleType() != QAudioFormat::SignedInt ||
        format.channelCount() < 1) {
        reportDiagnostic(QStringLiteral("Audio diagnostic: rejected negotiated format=%1 Hz, %2 channel(s), %3-bit %4 %5, codec=%6.")
                             .arg(format.sampleRate())
                             .arg(format.channelCount())
                             .arg(format.sampleSize())
                             .arg(qt5SampleTypeName(format.sampleType()))
                             .arg(qt5ByteOrderName(format.byteOrder()))
                             .arg(format.codec()));
        emit errorOccurred("Audio input does not support signed 16-bit PCM.");
        return false;
    }

    m_sampleRate.store(format.sampleRate(), std::memory_order_relaxed);
    m_channelCount = format.channelCount();
    resetDiagnosticCounters();

    reportDiagnostic(QStringLiteral("Audio diagnostic: negotiated format=%1 Hz, %2 channel(s), %3-bit %4 %5, codec=%6; converter remains channel 1 only.")
                         .arg(format.sampleRate())
                         .arg(format.channelCount())
                         .arg(format.sampleSize())
                         .arg(qt5SampleTypeName(format.sampleType()))
                         .arg(qt5ByteOrderName(format.byteOrder()))
                         .arg(format.codec()));

    m_audioInput = new QAudioInput(selectedDevice, format, this);
    connect(m_audioInput, &QAudioInput::stateChanged,
            this, [this](AudioNs::State state) {
                const AudioNs::Error error = m_audioInput != nullptr ? m_audioInput->error() : AudioNs::NoError;
                reportDiagnostic(QStringLiteral("Audio diagnostic: backend state=%1, error=%2.")
                                     .arg(audioStateName(state), audioErrorName(error)));
            });
    m_audioInput->setBufferSize(m_blockSamples * m_channelCount * 2 * 4);

    m_inputDevice = m_audioInput->start();

#endif

    if (m_inputDevice == nullptr) {
        const AudioNs::Error backendError = m_audioInput != nullptr ? m_audioInput->error() : AudioNs::OpenError;
        reportDiagnostic(QStringLiteral("Audio diagnostic: start returned no QIODevice; backend error=%1.")
                             .arg(audioErrorName(backendError)));
        releaseAudioInput();
        emit errorOccurred("Unable to start audio input.");
        return false;
    }

    connect(m_inputDevice, &QIODevice::readyRead,
            this, &AudioEngine::readInputData);

    m_running.store(true, std::memory_order_release);
    m_diagnosticReportClock.start();

    reportDiagnostic(QStringLiteral("Audio diagnostic: QIODevice open=%1, readable=%2, configured backend buffer request=%3 bytes, block=%4 frames (%5 bytes).")
                         .arg(m_inputDevice->isOpen() ? QStringLiteral("yes") : QStringLiteral("no"))
                         .arg(m_inputDevice->isReadable() ? QStringLiteral("yes") : QStringLiteral("no"))
                         .arg(m_blockSamples * qMax(1, m_channelCount) * 2 * 4)
                         .arg(m_blockSamples)
                         .arg(m_blockSamples * qMax(1, m_channelCount) * 2));

    emit started();

    return true;
}

void AudioEngine::stopInput()
{
    if (!m_running.load(std::memory_order_acquire) && m_audioInput == nullptr) {
        return;
    }

    maybeEmitPeriodicDiagnostics(true);
    reportDiagnostic(QStringLiteral("Audio diagnostic: stopping capture after %1 callback(s), %2 byte(s), %3 complete frame(s), %4 emitted mono sample(s).")
                         .arg(m_diagnosticCallbackCount)
                         .arg(m_diagnosticBytesReceived)
                         .arg(m_diagnosticFramesReceived)
                         .arg(m_totalSamples));

    releaseAudioInput();

    m_pendingBytes.clear();
    m_streamFirstUtcNs = 0;
    m_streamFirstMonotonicNs = 0;
    m_streamTimestampValid = false;
    m_inputDevice = nullptr;
    m_running.store(false, std::memory_order_release);

    emit stopped();
}

bool AudioEngine::isRunning() const
{
    return m_running.load(std::memory_order_acquire);
}

int AudioEngine::sampleRate() const
{
    const double corrected = static_cast<double>(m_sampleRate.load(std::memory_order_relaxed)) *
                             (1.0 + m_clockCorrectionPpm.load(std::memory_order_relaxed) / 1000000.0);
    return qMax(1, static_cast<int>(qRound(corrected)));
}

void AudioEngine::setClockCorrectionPpm(double ppm)
{
    if (!qIsFinite(ppm) || ppm < -5000.0 || ppm > 5000.0) {
        m_clockCorrectionPpm.store(0.0, std::memory_order_relaxed);
        return;
    }

    m_clockCorrectionPpm.store(ppm, std::memory_order_relaxed);
}

double AudioEngine::clockCorrectionPpm() const
{
    return m_clockCorrectionPpm.load(std::memory_order_relaxed);
}

void AudioEngine::setInputVolumePercent(int percent)
{
    m_inputVolumePercent.store(qBound(0, percent, 100), std::memory_order_relaxed);
}

int AudioEngine::inputVolumePercent() const
{
    return m_inputVolumePercent.load(std::memory_order_relaxed);
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

    const int bytesPerFrame = 2 * qMax(1, m_channelCount);
    const qint64 framesRead = data.size() / qMax(1, bytesPerFrame);
    const int remainderBytes = data.size() % qMax(1, bytesPerFrame);

    ++m_diagnosticCallbackCount;
    m_diagnosticBytesReceived += static_cast<quint64>(data.size());
    m_diagnosticFramesReceived += static_cast<quint64>(qMax<qint64>(qint64{0}, framesRead));
    m_diagnosticLastCallbackBytes = data.size();
    m_diagnosticLastCallbackFrames = framesRead;
    m_diagnosticLastRemainderBytes = remainderBytes;

    if (m_diagnosticCallbackCount <= 3) {
        reportDiagnostic(QStringLiteral("Audio diagnostic: callback #%1 read %2 byte(s) = %3 complete frame(s), remainder=%4 byte(s), pending-before=%5 byte(s).")
                             .arg(m_diagnosticCallbackCount)
                             .arg(data.size())
                             .arg(framesRead)
                             .arg(remainderBytes)
                             .arg(m_pendingBytes.size()));
    }

    const int effectiveRate = qMax(1, sampleRate());
    const qint64 nowMonotonicNs = m_captureClock.isValid() ? m_captureClock.nsecsElapsed() : 0;
    const qint64 nowUtcNs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() * 1000000LL;
    const qint64 dataDurationNs = framesRead > 0
        ? static_cast<qint64>(std::llround((1000000000.0 * static_cast<double>(framesRead)) /
                                         static_cast<double>(effectiveRate)))
        : 0;

    if (!m_streamTimestampValid) {
        // QAudio notifies us after the first captured frames are available.
        // Anchor sample index zero once, then derive every block timestamp from
        // the continuous sample count. This preserves exact spacing even when
        // readyRead callbacks or queued decoder work arrive late.
        m_streamFirstMonotonicNs = qMax<qint64>(qint64{0}, nowMonotonicNs - dataDurationNs);
        m_streamFirstUtcNs = nowUtcNs - dataDurationNs;
        m_streamTimestampValid = true;
    }

    m_pendingBytes.append(data);
    processPendingBytes();
}

void AudioEngine::processPendingBytes()
{
    const int bytesPerSample = 2;
    const int bytesPerFrame = bytesPerSample * m_channelCount;
    const int bytesPerBlock = bytesPerFrame * m_blockSamples;

    int consumedBytes = 0;
    const float gain = static_cast<float>(m_inputVolumePercent.load(std::memory_order_relaxed)) / 100.0f;
    while (m_pendingBytes.size() - consumedBytes >= bytesPerBlock) {
        AudioBlock block;
        block.sampleRate = sampleRate();
        block.firstSampleIndex = m_totalSamples;
        const double nsPerFrame = 1000000000.0 / static_cast<double>(qMax(1, block.sampleRate));
        const qint64 streamOffsetNs = static_cast<qint64>(std::llround(
            static_cast<double>(block.firstSampleIndex) * nsPerFrame));
        block.firstSampleUtcNs = m_streamTimestampValid ? m_streamFirstUtcNs + streamOffsetNs : 0;
        block.firstSampleMonotonicNs = m_streamTimestampValid ? m_streamFirstMonotonicNs + streamOffsetNs : 0;
        block.captureSequence = ++m_captureSequence;
        block.captureGeneration = m_captureGeneration;
        block.samples.resize(m_blockSamples);

        const char *raw = m_pendingBytes.constData() + consumedBytes;

        // Diagnostic-only pass over all negotiated channels.  The conversion
        // loop below remains exactly channel 1, as in the supplied baseline.
        accumulateInputDiagnostics(raw, m_blockSamples, bytesPerFrame);

        for (int i = 0; i < m_blockSamples; ++i) {
            const char *frame = raw + (i * bytesPerFrame);

            qint16 sample = 0;
            memcpy(&sample, frame, sizeof(qint16));

            block.samples[i] = (static_cast<float>(sample) / 32768.0f) * gain;
        }

        consumedBytes += bytesPerBlock;
        m_totalSamples += m_blockSamples;

        emitLevel(block.samples);
        emit audioBlockReady(block);
        maybeEmitPeriodicDiagnostics();
    }
    if (consumedBytes > 0) {
        m_pendingBytes.remove(0, consumedBytes);
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

void AudioEngine::reportDiagnostic(const QString &message)
{
    qInfo().noquote() << message;
    emit diagnosticMessage(message);
}

void AudioEngine::resetDiagnosticCounters()
{
    m_diagnosticChannelSumSquares.fill(0.0, qMax(1, m_channelCount));
    m_diagnosticChannelPeaks.fill(0, qMax(1, m_channelCount));
    m_diagnosticMeasuredFrames = 0;
    m_diagnosticCallbackCount = 0;
    m_diagnosticBytesReceived = 0;
    m_diagnosticFramesReceived = 0;
    m_diagnosticLastCallbackBytes = 0;
    m_diagnosticLastCallbackFrames = 0;
    m_diagnosticLastRemainderBytes = 0;
    m_diagnosticReportClock.invalidate();
}

void AudioEngine::accumulateInputDiagnostics(const char *raw, int frameCount, int bytesPerFrame)
{
    if (raw == nullptr || frameCount <= 0 || bytesPerFrame <= 0 || m_channelCount <= 0) {
        return;
    }

    if (m_diagnosticChannelSumSquares.size() != m_channelCount) {
        m_diagnosticChannelSumSquares.fill(0.0, m_channelCount);
        m_diagnosticChannelPeaks.fill(0, m_channelCount);
        m_diagnosticMeasuredFrames = 0;
    }

    for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        const char *frame = raw + frameIndex * bytesPerFrame;
        for (int channel = 0; channel < m_channelCount; ++channel) {
            qint16 sample = 0;
            memcpy(&sample, frame + channel * static_cast<int>(sizeof(qint16)), sizeof(qint16));
            const int magnitude = sample == std::numeric_limits<qint16>::min()
                ? 32768
                : std::abs(static_cast<int>(sample));
            const double normalized = static_cast<double>(sample) / 32768.0;
            m_diagnosticChannelSumSquares[channel] += normalized * normalized;
            m_diagnosticChannelPeaks[channel] = qMax(m_diagnosticChannelPeaks[channel], magnitude);
        }
    }

    m_diagnosticMeasuredFrames += static_cast<quint64>(frameCount);
}

void AudioEngine::maybeEmitPeriodicDiagnostics(bool force)
{
    if (!force) {
        if (!m_diagnosticReportClock.isValid()) {
            m_diagnosticReportClock.start();
            return;
        }
        if (m_diagnosticReportClock.elapsed() < 2000) {
            return;
        }
    }

    if (m_diagnosticMeasuredFrames == 0 || m_diagnosticChannelSumSquares.isEmpty()) {
        if (force && m_diagnosticCallbackCount > 0) {
            reportDiagnostic(QStringLiteral("Audio diagnostic: callbacks arrived but no complete %1-frame PCM block was emitted; pending=%2 byte(s), last callback=%3 byte(s)/%4 frame(s), remainder=%5.")
                                 .arg(m_blockSamples)
                                 .arg(m_pendingBytes.size())
                                 .arg(m_diagnosticLastCallbackBytes)
                                 .arg(m_diagnosticLastCallbackFrames)
                                 .arg(m_diagnosticLastRemainderBytes));
        }
        if (m_diagnosticReportClock.isValid()) {
            m_diagnosticReportClock.restart();
        }
        return;
    }

    QStringList channelSummaries;
    QVector<double> rmsDbValues;
    rmsDbValues.reserve(m_diagnosticChannelSumSquares.size());

    for (int channel = 0; channel < m_diagnosticChannelSumSquares.size(); ++channel) {
        const double rms = std::sqrt(m_diagnosticChannelSumSquares.at(channel) /
                                     static_cast<double>(m_diagnosticMeasuredFrames));
        const double peak = static_cast<double>(m_diagnosticChannelPeaks.value(channel)) / 32768.0;
        const double rmsDb = linearToDbfs(rms);
        const double peakDb = linearToDbfs(peak);
        rmsDbValues.push_back(rmsDb);
        channelSummaries << QStringLiteral("ch%1 RMS %2 dBFS peak %3 dBFS")
                                .arg(channel + 1)
                                .arg(rmsDb, 0, 'f', 1)
                                .arg(peakDb, 0, 'f', 1);
    }

    reportDiagnostic(QStringLiteral("Audio diagnostic: %1; measured=%2 frame(s), callbacks=%3, bytes=%4, pending=%5, last callback=%6 B/%7 frames/rem %8. MONO OUTPUT=ch1 (unchanged).")
                         .arg(channelSummaries.join(QStringLiteral("; ")))
                         .arg(m_diagnosticMeasuredFrames)
                         .arg(m_diagnosticCallbackCount)
                         .arg(m_diagnosticBytesReceived)
                         .arg(m_pendingBytes.size())
                         .arg(m_diagnosticLastCallbackBytes)
                         .arg(m_diagnosticLastCallbackFrames)
                         .arg(m_diagnosticLastRemainderBytes));

    if (rmsDbValues.size() > 1) {
        int strongestChannel = 0;
        for (int channel = 1; channel < rmsDbValues.size(); ++channel) {
            if (rmsDbValues.at(channel) > rmsDbValues.at(strongestChannel)) {
                strongestChannel = channel;
            }
        }
        const double advantageDb = rmsDbValues.at(strongestChannel) - rmsDbValues.at(0);
        if (strongestChannel != 0 && advantageDb >= 6.0) {
            reportDiagnostic(QStringLiteral("Audio diagnostic WARNING: channel %1 is %2 dB stronger than channel 1, but this diagnostic build deliberately still feeds channel 1 to the application.")
                                 .arg(strongestChannel + 1)
                                 .arg(advantageDb, 0, 'f', 1));
        }
    }

    m_diagnosticChannelSumSquares.fill(0.0, qMax(1, m_channelCount));
    m_diagnosticChannelPeaks.fill(0, qMax(1, m_channelCount));
    m_diagnosticMeasuredFrames = 0;
    if (m_diagnosticReportClock.isValid()) {
        m_diagnosticReportClock.restart();
    }
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
