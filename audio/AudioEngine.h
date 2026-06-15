#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#include "AudioBlock.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtGlobal>

class QIODevice;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class QAudioSource;
#else
class QAudioInput;
#endif

/**
 * @brief Captures audio from the selected input device and emits float blocks.
 *
 * Purpose:
 * - Hide Qt5 / Qt6 multimedia API differences.
 * - Open a selected audio input device.
 * - Convert signed 16-bit PCM samples to normalized mono float samples.
 * - Emit fixed-size AudioBlock objects for DSP processing.
 */
class AudioEngine : public QObject
{
    Q_OBJECT

public:
    explicit AudioEngine(QObject *parent = nullptr);
    ~AudioEngine() override;

    /**
     * @brief Starts audio capture from a selected device name.
     *
     * Behavior:
     * - Uses 48000 Hz, mono, signed 16-bit PCM when available.
     * - Falls back to the closest supported format when possible.
     * - Emits errorOccurred(...) if the device cannot be opened.
     */
    bool startInput(const QString &deviceName, int requestedSampleRate = 48000);

    /**
     * @brief Stops audio capture and clears pending buffers.
     */
    void stopInput();

    /**
     * @brief Returns true when audio capture is running.
     */
    bool isRunning() const;

    /**
     * @brief Returns the active input sample rate.
     */
    int sampleRate() const;

    /**
     * @brief Sets measured RX sound-card clock correction in parts per million.
     */
    void setClockCorrectionPpm(double ppm);

    /**
     * @brief Returns the current RX sound-card clock correction in ppm.
     */
    double clockCorrectionPpm() const;

    /**
     * @brief Sets RX input attenuation. 100% leaves samples unchanged.
     */
    void setInputVolumePercent(int percent);

    /**
     * @brief Returns the current RX input attenuation percent.
     */
    int inputVolumePercent() const;

signals:
    void audioBlockReady(const AudioBlock &block);
    void levelChanged(int percent, double db, double rms);
    void errorOccurred(const QString &message);
    void started();
    void stopped();

private slots:
    void readInputData();

private:
    /**
     * @brief Converts pending PCM bytes into normalized AudioBlock objects.
     */
    void processPendingBytes();

    /**
     * @brief Emits a level estimate for the current block.
     */
    void emitLevel(const QVector<float> &samples);

    /**
     * @brief Releases the active Qt audio object.
     */
    void releaseAudioInput();

private:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioSource *m_audioInput = nullptr;
#else
    QAudioInput *m_audioInput = nullptr;
#endif

    QIODevice *m_inputDevice = nullptr;

    QByteArray m_pendingBytes;

    int m_sampleRate = 48000;
    double m_clockCorrectionPpm = 0.0;
    int m_inputVolumePercent = 100;
    int m_channelCount = 1;
    int m_blockSamples = 1024;

    qint64 m_totalSamples = 0;

    bool m_running = false;
};

#endif // AUDIOENGINE_H
