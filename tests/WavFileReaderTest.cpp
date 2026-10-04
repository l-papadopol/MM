#include "audio/WavFileReader.h"

#include <QByteArray>
#include <QFile>
#include <QTemporaryFile>

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

namespace {

void appendLe16(QByteArray &bytes, quint16 value)
{
    bytes.append(static_cast<char>(value & 0xffU));
    bytes.append(static_cast<char>((value >> 8) & 0xffU));
}

void appendLe32(QByteArray &bytes, quint32 value)
{
    bytes.append(static_cast<char>(value & 0xffU));
    bytes.append(static_cast<char>((value >> 8) & 0xffU));
    bytes.append(static_cast<char>((value >> 16) & 0xffU));
    bytes.append(static_cast<char>((value >> 24) & 0xffU));
}

QByteArray makeWav(quint16 format,
                   quint16 channels,
                   quint32 sampleRate,
                   quint16 bitsPerSample,
                   quint16 blockAlign,
                   const QByteArray &audio,
                   qint32 declaredDataSize = -1,
                   qint32 declaredRiffSize = -1)
{
    QByteArray bytes;
    bytes.append("RIFF", 4);
    appendLe32(bytes, 0);
    bytes.append("WAVE", 4);
    bytes.append("fmt ", 4);
    appendLe32(bytes, 16);
    appendLe16(bytes, format);
    appendLe16(bytes, channels);
    appendLe32(bytes, sampleRate);
    appendLe32(bytes, sampleRate * blockAlign);
    appendLe16(bytes, blockAlign);
    appendLe16(bytes, bitsPerSample);
    bytes.append("data", 4);
    appendLe32(bytes, declaredDataSize >= 0 ? static_cast<quint32>(declaredDataSize)
                                            : static_cast<quint32>(audio.size()));
    bytes.append(audio);

    const quint32 riffSize = declaredRiffSize >= 0
        ? static_cast<quint32>(declaredRiffSize)
        : static_cast<quint32>(bytes.size() - 8);
    bytes[4] = static_cast<char>(riffSize & 0xffU);
    bytes[5] = static_cast<char>((riffSize >> 8) & 0xffU);
    bytes[6] = static_cast<char>((riffSize >> 16) & 0xffU);
    bytes[7] = static_cast<char>((riffSize >> 24) & 0xffU);
    return bytes;
}

bool parseBytes(const QByteArray &bytes,
                MadModemAudio::WavFileFormat &format,
                QString &error,
                QByteArray *audioOut = nullptr)
{
    QTemporaryFile file;
    if (!file.open() || file.write(bytes) != bytes.size() || !file.flush()) {
        error = QStringLiteral("cannot create temporary test WAV");
        return false;
    }
    const bool ok = MadModemAudio::parseWavHeader(file, format, error);
    if (ok && audioOut != nullptr) {
        *audioOut = file.read(format.dataSize);
    }
    return ok;
}

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    std::cout << "PASS: " << message << '\n';
    return true;
}

} // namespace

int main()
{
    bool ok = true;

    QByteArray pcm;
    appendLe16(pcm, 0x8000U);
    appendLe16(pcm, 0x7fffU);
    MadModemAudio::WavFileFormat format;
    QString error;
    QByteArray audio;
    ok &= expect(parseBytes(makeWav(1, 1, 48000, 16, 2, pcm), format, error, &audio),
                 "valid PCM WAV parses");
    const QVector<float> samples = MadModemAudio::convertWavBytesToMono(audio, format, &error);
    ok &= expect(samples.size() == 2 && samples.front() <= -0.999f && samples.back() >= 0.999f,
                 "valid PCM frames convert safely");

    MadModemAudio::WavFileFormat badFormat;
    QString badError;
    ok &= expect(!parseBytes(makeWav(1, 2, 48000, 16, 2, pcm), badFormat, badError),
                 "undersized blockAlign is rejected");
    ok &= expect(!parseBytes(makeWav(1, 1, 48000, 16, 2, QByteArray(3, '\0')), badFormat, badError),
                 "partial audio frame is rejected");
    ok &= expect(!parseBytes(makeWav(1, 1, 48000, 16, 2, pcm, 4096), badFormat, badError),
                 "data chunk beyond RIFF bounds is rejected");
    ok &= expect(!parseBytes(makeWav(1, 1, 48000, 16, 2, pcm, -1, 0x100000), badFormat, badError),
                 "declared RIFF size beyond file is rejected");

    float nanValue = std::numeric_limits<float>::quiet_NaN();
    quint32 nanBits = 0;
    static_assert(sizeof(nanValue) == sizeof(nanBits), "float32 test layout mismatch");
    std::memcpy(&nanBits, &nanValue, sizeof(nanBits));
    QByteArray floatAudio;
    appendLe32(floatAudio, nanBits);
    error.clear();
    audio.clear();
    ok &= expect(parseBytes(makeWav(3, 1, 48000, 32, 4, floatAudio), format, error, &audio),
                 "float32 WAV container parses");
    const QVector<float> invalidFloat = MadModemAudio::convertWavBytesToMono(audio, format, &error);
    ok &= expect(invalidFloat.isEmpty() && !error.isEmpty(),
                 "non-finite float sample is rejected");

    return ok ? 0 : 1;
}
