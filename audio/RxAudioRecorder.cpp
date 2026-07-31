#include "RxAudioRecorder.h"

#include <QDataStream>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr qint64 kWavHeaderBytes = 44;
constexpr quint64 kMaxInsertedSilenceSeconds = 10;

qint16 floatToPcm16(float sample)
{
    const double bounded = std::max(-1.0, std::min(1.0, static_cast<double>(sample)));
    const long value = std::lround(bounded * 32767.0);
    return static_cast<qint16>(std::max<long>(-32768, std::min<long>(32767, value)));
}
}

RxAudioRecorder::RxAudioRecorder(QObject *parent)
    : QObject(parent)
{
}

RxAudioRecorder::~RxAudioRecorder()
{
    closeFile(false);
}

bool RxAudioRecorder::isRecording() const
{
    return m_recording;
}

void RxAudioRecorder::startRecording(const QString &fileName, int expectedSampleRate)
{
    closeFile(false);

    if (fileName.trimmed().isEmpty()) {
        emit recordingError(QStringLiteral("Audio recording failed: empty output file name."));
        return;
    }
    const QString cleanName = QFileInfo(fileName).absoluteFilePath();

    m_file.setFileName(cleanName);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit recordingError(QStringLiteral("Audio recording failed: cannot open %1: %2")
                                .arg(cleanName, m_file.errorString()));
        return;
    }

    m_fileName = cleanName;
    m_sampleRate = std::max(8000, std::min(192000, expectedSampleRate));
    m_samplesWritten = 0;
    m_nextSampleIndex = -1;
    m_lastCaptureSequence = 0;
    m_haveCaptureSequence = false;

    if (!writeHeader(m_sampleRate, 0U)) {
        const QString error = m_file.errorString();
        closeFile(false);
        emit recordingError(QStringLiteral("Audio recording failed while writing WAV header: %1")
                                .arg(error));
        return;
    }

    m_recording = true;
    emit recordingStarted(m_fileName, m_sampleRate);
}

void RxAudioRecorder::stopRecording()
{
    closeFile(true);
}

void RxAudioRecorder::writeAudioBlock(const AudioBlock &block)
{
    if (!m_recording || !m_file.isOpen() || block.samples.isEmpty()) {
        return;
    }

    if (block.sampleRate > 0 && block.sampleRate != m_sampleRate) {
        emit recordingError(QStringLiteral(
            "Audio recording stopped: sample rate changed from %1 Hz to %2 Hz.")
                                .arg(m_sampleRate)
                                .arg(block.sampleRate));
        closeFile(true);
        return;
    }

    if (!m_haveCaptureSequence) {
        m_lastCaptureSequence = block.captureSequence;
        m_haveCaptureSequence = true;
        m_nextSampleIndex = block.firstSampleIndex;
    } else {
        // AudioEngine increments captureSequence for every emitted block and
        // restarts it from one when a new capture session begins. A non-
        // increasing value therefore means that firstSampleIndex has also
        // restarted; do not interpret that restart as a many-hour WAV gap.
        if (block.captureSequence <= m_lastCaptureSequence)
            m_nextSampleIndex = block.firstSampleIndex;
        m_lastCaptureSequence = block.captureSequence;
    }

    qint64 blockBegin = block.firstSampleIndex;
    int sampleBegin = 0;
    int sampleCount = block.samples.size();

    if (m_nextSampleIndex >= 0 && blockBegin > m_nextSampleIndex) {
        const quint64 gap = static_cast<quint64>(blockBegin - m_nextSampleIndex);
        const quint64 maximumGap = static_cast<quint64>(m_sampleRate) *
                                   kMaxInsertedSilenceSeconds;
        if (gap <= maximumGap && !writeSilence(gap)) {
            emit recordingError(QStringLiteral("Audio recording stopped: disk write failed: %1")
                                    .arg(m_file.errorString()));
            closeFile(true);
            return;
        }
        m_nextSampleIndex = blockBegin;
    } else if (m_nextSampleIndex >= 0 && blockBegin < m_nextSampleIndex) {
        const qint64 overlap = m_nextSampleIndex - blockBegin;
        if (overlap >= sampleCount) {
            return;
        }
        sampleBegin = static_cast<int>(overlap);
        sampleCount -= sampleBegin;
        blockBegin += overlap;
    }

    if (!writeSamples(block.samples, sampleBegin, sampleCount)) {
        emit recordingError(QStringLiteral("Audio recording stopped: disk write failed: %1")
                                .arg(m_file.errorString()));
        closeFile(true);
        return;
    }
    m_nextSampleIndex = blockBegin + sampleCount;
}

bool RxAudioRecorder::writeHeader(int sampleRate, quint32 dataBytes)
{
    if (!m_file.isOpen() || !m_file.seek(0)) {
        return false;
    }

    QDataStream stream(&m_file);
    stream.setByteOrder(QDataStream::LittleEndian);
    const quint16 channels = 1;
    const quint16 bitsPerSample = 16;
    const quint32 byteRate = static_cast<quint32>(sampleRate) * channels *
                             bitsPerSample / 8U;
    const quint16 blockAlign = channels * bitsPerSample / 8U;

    stream.writeRawData("RIFF", 4);
    stream << static_cast<quint32>(36U + dataBytes);
    stream.writeRawData("WAVE", 4);
    stream.writeRawData("fmt ", 4);
    stream << static_cast<quint32>(16U);
    stream << static_cast<quint16>(1U); // PCM
    stream << channels;
    stream << static_cast<quint32>(sampleRate);
    stream << byteRate;
    stream << blockAlign;
    stream << bitsPerSample;
    stream.writeRawData("data", 4);
    stream << dataBytes;
    return stream.status() == QDataStream::Ok && m_file.pos() == kWavHeaderBytes;
}

bool RxAudioRecorder::rewriteHeader()
{
    const quint64 dataBytes64 = m_samplesWritten * 2U;
    const quint32 dataBytes = static_cast<quint32>(std::min<quint64>(
        dataBytes64, std::numeric_limits<quint32>::max() - 36U));
    const qint64 endPosition = m_file.pos();
    if (!writeHeader(m_sampleRate, dataBytes)) {
        return false;
    }
    return m_file.seek(endPosition);
}

bool RxAudioRecorder::writeSilence(quint64 sampleCount)
{
    static const QByteArray zeros(8192, '\0');
    quint64 bytesRemaining = sampleCount * 2U;
    while (bytesRemaining > 0U) {
        const qint64 chunk = static_cast<qint64>(std::min<quint64>(
            bytesRemaining, static_cast<quint64>(zeros.size())));
        if (m_file.write(zeros.constData(), chunk) != chunk) {
            return false;
        }
        bytesRemaining -= static_cast<quint64>(chunk);
    }
    m_samplesWritten += sampleCount;
    return true;
}

bool RxAudioRecorder::writeSamples(const QVector<float> &samples, int begin, int count)
{
    if (count <= 0) {
        return true;
    }
    QByteArray pcm;
    pcm.resize(count * static_cast<int>(sizeof(qint16)));
    char *destination = pcm.data();
    for (int i = 0; i < count; ++i) {
        const qint16 value = floatToPcm16(samples.at(begin + i));
        const quint16 raw = static_cast<quint16>(value);
        destination[2 * i] = static_cast<char>(raw & 0xffU);
        destination[2 * i + 1] = static_cast<char>((raw >> 8U) & 0xffU);
    }
    const qint64 expected = pcm.size();
    if (m_file.write(pcm) != expected) {
        return false;
    }
    m_samplesWritten += static_cast<quint64>(count);
    return true;
}

void RxAudioRecorder::closeFile(bool emitStoppedSignal)
{
    if (!m_file.isOpen()) {
        m_recording = false;
        return;
    }

    const QString finishedName = m_fileName;
    const quint64 finishedSamples = m_samplesWritten;
    if (m_recording) {
        rewriteHeader();
    }
    m_file.flush();
    const qint64 finalBytes = m_file.size();
    m_file.close();

    m_recording = false;
    m_fileName.clear();
    m_samplesWritten = 0;
    m_nextSampleIndex = -1;
    m_haveCaptureSequence = false;

    if (emitStoppedSignal) {
        emit recordingStopped(finishedName, finishedSamples, finalBytes);
    }
}
