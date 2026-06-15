#ifndef TXAUDIOENGINE_H
#define TXAUDIOENGINE_H

#include "AudioBlock.h"
#include "../tx/TxModulator.h"

#include <QObject>
#include <QString>
#include <memory>

class QIODevice;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class QAudioSink;
#else
class QAudioOutput;
#endif

/**
 * @brief Plays generated modem TX audio to a selected output device.
 *
 * Purpose:
 * - Hide Qt5 / Qt6 audio-output API differences.
 * - Pull samples from a TxModulator in real time.
 * - Emit a copy of generated samples for the waterfall.
 * - Report transmission progress and completion.
 */
class TxAudioEngine : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Creates an idle TX audio engine.
     */
    explicit TxAudioEngine(QObject *parent = nullptr);

    /**
     * @brief Stops TX and releases audio resources.
     */
    ~TxAudioEngine() override;

    /**
     * @brief Starts playback using the selected output device and modulator.
     */
    bool startOutput(const QString &deviceName, std::unique_ptr<TxModulator> modulator);

    /**
     * @brief Stops active playback immediately.
     */
    void stopOutput();

    /**
     * @brief Returns true while TX audio output is active.
     */
    bool isRunning() const;

    /**
     * @brief Returns the active output sample rate.
     */
    int sampleRate() const;

    /**
     * @brief Sets TX output attenuation. 100% generates unchanged PCM.
     */
    void setOutputVolumePercent(int percent);

    /**
     * @brief Returns current TX output attenuation percent.
     */
    int outputVolumePercent() const;

signals:
    /**
     * @brief Emits generated TX audio blocks for the waterfall.
     */
    void audioBlockReady(const AudioBlock &block);

    /**
     * @brief Emits lightweight RTTY TX tone metadata for tuning instruments.
     *
     * This signal is generated from transmitter state, not by demodulating PCM.
     * It is intentionally throttled so the UI can drop visual frames without
     * affecting real-time audio generation.
     */
    void rttyToneStateChanged(bool mark, double progress);

    /**
     * @brief Emits image-progress ratio in the range 0.0 ... 1.0.
     */
    void progressChanged(double progress);

    /**
     * @brief Emits after the audio device has started.
     */
    void started();

    /**
     * @brief Emits after TX has stopped or ended naturally.
     */
    void stopped();

    /**
     * @brief Emits when the modulator reaches the end of the image.
     */
    void finished();

    /**
     * @brief Emits user-visible audio-output errors.
     */
    void errorOccurred(const QString &message);

private slots:
    /**
     * @brief Handles natural end-of-transmission from the pull device.
     */
    void handleDeviceFinished();

private:
    /**
     * @brief Releases the active Qt audio output object and pull device.
     */
    void releaseAudioOutput();

private:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioSink *m_audioOutput = nullptr;
#else
    QAudioOutput *m_audioOutput = nullptr;
#endif

    QIODevice *m_outputDevice = nullptr;
    std::unique_ptr<TxModulator> m_modulator;

    int m_sampleRate = 48000;
    qint64 m_totalSamples = 0;
    bool m_running = false;
    bool m_finishedEmitted = false;
    int m_outputVolumePercent = 100;
};

#endif // TXAUDIOENGINE_H
