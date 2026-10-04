#include "WavFileReader.h"

#include <QtGlobal>

#include <cmath>
#include <cstring>
#include <limits>

namespace MadModemAudio {
namespace {

quint16 readLe16(const char *p)
{
    return static_cast<quint16>(static_cast<unsigned char>(p[0])) |
           static_cast<quint16>(static_cast<unsigned char>(p[1]) << 8);
}

quint32 readLe32(const char *p)
{
    return static_cast<quint32>(static_cast<unsigned char>(p[0])) |
           (static_cast<quint32>(static_cast<unsigned char>(p[1])) << 8) |
           (static_cast<quint32>(static_cast<unsigned char>(p[2])) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(p[3])) << 24);
}

qint32 readLe24(const char *p)
{
    qint32 value = static_cast<qint32>(static_cast<unsigned char>(p[0])) |
                   (static_cast<qint32>(static_cast<unsigned char>(p[1])) << 8) |
                   (static_cast<qint32>(static_cast<unsigned char>(p[2])) << 16);
    if ((value & 0x00800000) != 0) {
        value |= static_cast<qint32>(0xff000000);
    }
    return value;
}

bool fail(QString &error, const QString &message)
{
    error = message;
    return false;
}

bool decodeSample(const char *data, const WavFileFormat &format, float &valueOut)
{
    if (format.audioFormat == 3 && format.bitsPerSample == 32) {
        const quint32 raw = readLe32(data);
        float value = 0.0f;
        static_assert(sizeof(value) == sizeof(raw), "IEEE float32 size mismatch");
        std::memcpy(&value, &raw, sizeof(value));
        if (!std::isfinite(value)) {
            return false;
        }
        valueOut = qBound(-1.0f, value, 1.0f);
        return true;
    }

    switch (format.bitsPerSample) {
    case 8:
        valueOut = static_cast<float>(static_cast<int>(static_cast<unsigned char>(data[0])) - 128) / 128.0f;
        return true;
    case 16:
        valueOut = static_cast<float>(static_cast<qint16>(readLe16(data))) / 32768.0f;
        return true;
    case 24:
        valueOut = static_cast<float>(readLe24(data)) / 8388608.0f;
        return true;
    case 32:
        valueOut = static_cast<float>(static_cast<double>(static_cast<qint32>(readLe32(data))) / 2147483648.0);
        return true;
    default:
        return false;
    }
}

} // namespace

bool parseWavHeader(QFile &file, WavFileFormat &format, QString &errorMessage)
{
    format = WavFileFormat();
    if (!file.isOpen() || !file.isReadable()) {
        return fail(errorMessage, QStringLiteral("WAV file is not open for reading"));
    }
    if (!file.seek(0)) {
        return fail(errorMessage, QStringLiteral("cannot seek to WAV file start"));
    }

    const qint64 fileSize = file.size();
    if (fileSize < 12) {
        return fail(errorMessage, QStringLiteral("WAV file is shorter than the RIFF header"));
    }
    const QByteArray riff = file.read(12);
    if (riff.size() != 12 || riff.mid(0, 4) != "RIFF" || riff.mid(8, 4) != "WAVE") {
        return fail(errorMessage, QStringLiteral("not a RIFF/WAVE file"));
    }
    const qint64 riffPayloadSize = static_cast<qint64>(readLe32(riff.constData() + 4));
    if (riffPayloadSize < 4 || riffPayloadSize > fileSize - 8) {
        return fail(errorMessage, QStringLiteral("RIFF size exceeds file bounds"));
    }
    const qint64 riffEnd = 8 + riffPayloadSize;

    bool haveFmt = false;
    bool haveData = false;
    while (file.pos() <= riffEnd - 8) {
        const QByteArray header = file.read(8);
        if (header.size() != 8) {
            return fail(errorMessage, QStringLiteral("truncated WAV chunk header"));
        }
        const QByteArray id = header.left(4);
        const quint32 chunkSize = readLe32(header.constData() + 4);
        const qint64 payloadOffset = file.pos();
        const qint64 payloadSize = static_cast<qint64>(chunkSize);
        if (payloadOffset < 0 || payloadSize > riffEnd - payloadOffset) {
            return fail(errorMessage, QStringLiteral("WAV chunk extends beyond the RIFF container"));
        }

        if (id == "fmt ") {
            if (chunkSize < 16 || chunkSize > 4096) {
                return fail(errorMessage, QStringLiteral("invalid or unsupported WAV fmt chunk"));
            }
            const QByteArray fmt = file.read(payloadSize);
            if (fmt.size() != static_cast<int>(chunkSize)) {
                return fail(errorMessage, QStringLiteral("truncated WAV fmt chunk"));
            }
            format.audioFormat = readLe16(fmt.constData());
            format.channels = readLe16(fmt.constData() + 2);
            format.sampleRate = readLe32(fmt.constData() + 4);
            const quint32 byteRate = readLe32(fmt.constData() + 8);
            format.blockAlign = readLe16(fmt.constData() + 12);
            format.bitsPerSample = readLe16(fmt.constData() + 14);

            if (format.audioFormat == 0xfffe) {
                if (fmt.size() < 40 || readLe16(fmt.constData() + 16) < 22) {
                    return fail(errorMessage, QStringLiteral("truncated WAV extensible format"));
                }
                format.audioFormat = readLe16(fmt.constData() + 24);
            }

            if (format.channels < 1 || format.channels > 8 ||
                format.sampleRate < 8000 || format.sampleRate > 384000) {
                return fail(errorMessage, QStringLiteral("unsupported WAV channel/rate layout"));
            }
            const bool pcm = format.audioFormat == 1 &&
                             (format.bitsPerSample == 8 || format.bitsPerSample == 16 ||
                              format.bitsPerSample == 24 || format.bitsPerSample == 32);
            const bool ieeeFloat = format.audioFormat == 3 && format.bitsPerSample == 32;
            if (!pcm && !ieeeFloat) {
                return fail(errorMessage, QStringLiteral("unsupported WAV sample format; use PCM 8/16/24/32-bit or float32"));
            }

            const quint32 bytesPerSample = static_cast<quint32>(format.bitsPerSample / 8);
            const quint64 expectedAlign = static_cast<quint64>(format.channels) * bytesPerSample;
            const quint64 expectedRate = static_cast<quint64>(format.sampleRate) * expectedAlign;
            if (bytesPerSample == 0 || expectedAlign > std::numeric_limits<quint16>::max() ||
                format.blockAlign != expectedAlign || byteRate != expectedRate) {
                return fail(errorMessage, QStringLiteral("inconsistent WAV block alignment or byte rate"));
            }
            haveFmt = true;
        } else if (id == "data") {
            format.dataOffset = payloadOffset;
            format.dataSize = payloadSize;
            haveData = true;
        }

        if (haveFmt && haveData) {
            break;
        }

        const qint64 padding = static_cast<qint64>(chunkSize & 1U);
        if (payloadSize > std::numeric_limits<qint64>::max() - payloadOffset - padding) {
            return fail(errorMessage, QStringLiteral("WAV chunk offset overflow"));
        }
        const qint64 next = payloadOffset + payloadSize + padding;
        if (next > riffEnd || !file.seek(next)) {
            return fail(errorMessage, QStringLiteral("cannot seek to the next WAV chunk"));
        }
    }

    if (!haveFmt || !haveData) {
        return fail(errorMessage, QStringLiteral("WAV is missing fmt or data chunk"));
    }
    if (format.dataSize <= 0 || format.dataSize % format.blockAlign != 0) {
        return fail(errorMessage, QStringLiteral("WAV data size is not a whole number of audio frames"));
    }
    if (format.dataOffset < 0 || format.dataSize > riffEnd - format.dataOffset) {
        return fail(errorMessage, QStringLiteral("WAV data chunk exceeds file bounds"));
    }
    if (!file.seek(format.dataOffset)) {
        return fail(errorMessage, QStringLiteral("cannot seek to WAV audio data"));
    }
    return true;
}

QVector<float> convertWavBytesToMono(const QByteArray &bytes,
                                     const WavFileFormat &format,
                                     QString *errorMessage)
{
    QVector<float> samples;
    const int bytesPerSample = static_cast<int>(format.bitsPerSample / 8);
    const quint64 minimumFrameBytes = static_cast<quint64>(format.channels) *
                                      static_cast<quint64>(qMax(0, bytesPerSample));
    if (format.channels == 0 || bytesPerSample <= 0 || format.blockAlign != minimumFrameBytes) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("invalid WAV frame layout");
        }
        return samples;
    }
    if (bytes.size() % static_cast<int>(format.blockAlign) != 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("partial WAV frame in input buffer");
        }
        return samples;
    }

    const int frames = bytes.size() / static_cast<int>(format.blockAlign);
    samples.reserve(frames);
    for (int frame = 0; frame < frames; ++frame) {
        const char *frameData = bytes.constData() + frame * static_cast<int>(format.blockAlign);
        double mono = 0.0;
        for (quint16 channel = 0; channel < format.channels; ++channel) {
            float sample = 0.0f;
            if (!decodeSample(frameData + channel * bytesPerSample, format, sample)) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("WAV contains an invalid or non-finite sample");
                }
                samples.clear();
                return samples;
            }
            mono += static_cast<double>(sample);
        }
        mono /= static_cast<double>(format.channels);
        samples.append(static_cast<float>(qBound(-1.0, mono, 1.0)));
    }
    return samples;
}

} // namespace MadModemAudio
