#ifndef RXAUDIORECORDER_H
#define RXAUDIORECORDER_H

#include "AudioBlock.h"

#include <QFile>
#include <QObject>
#include <QString>
#include <QtGlobal>

/**
 * @brief Writes the normalized live RX stream to a mono 16-bit PCM WAV file.
 *
 * The recorder is intentionally downstream from AudioEngine: the WAV contains
 * exactly the mono samples seen by the waterfall and decoders, including the
 * selected input attenuation and sound-card sample rate. It runs in its own
 * thread so disk I/O never blocks capture, DSP or the GUI.
 */
class RxAudioRecorder final : public QObject
{
    Q_OBJECT

public:
    explicit RxAudioRecorder(QObject *parent = nullptr);
    ~RxAudioRecorder() override;

    bool isRecording() const;

public slots:
    void startRecording(const QString &fileName, int expectedSampleRate);
    void stopRecording();
    void writeAudioBlock(const AudioBlock &block);

signals:
    void recordingStarted(const QString &fileName, int sampleRate);
    void recordingStopped(const QString &fileName, quint64 samplesWritten,
                          qint64 bytesWritten);
    void recordingError(const QString &message);

private:
    bool writeHeader(int sampleRate, quint32 dataBytes);
    bool rewriteHeader();
    bool writeSilence(quint64 sampleCount);
    bool writeSamples(const QVector<float> &samples, int begin, int count);
    void closeFile(bool emitStoppedSignal);

private:
    QFile m_file;
    QString m_fileName;
    int m_sampleRate = 48000;
    quint64 m_samplesWritten = 0;
    qint64 m_nextSampleIndex = -1;
    quint64 m_lastCaptureSequence = 0;
    bool m_haveCaptureSequence = false;
    bool m_recording = false;
};

#endif // RXAUDIORECORDER_H
