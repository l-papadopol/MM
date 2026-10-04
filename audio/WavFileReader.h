#ifndef WAVFILEREADER_H
#define WAVFILEREADER_H

#include <QByteArray>
#include <QFile>
#include <QString>
#include <QVector>

namespace MadModemAudio {

struct WavFileFormat
{
    quint16 audioFormat = 0;
    quint16 channels = 0;
    quint32 sampleRate = 0;
    quint16 blockAlign = 0;
    quint16 bitsPerSample = 0;
    qint64 dataOffset = 0;
    qint64 dataSize = 0;
};

bool parseWavHeader(QFile &file, WavFileFormat &format, QString &errorMessage);
QVector<float> convertWavBytesToMono(const QByteArray &bytes,
                                     const WavFileFormat &format,
                                     QString *errorMessage = nullptr);

} // namespace MadModemAudio

#endif // WAVFILEREADER_H
