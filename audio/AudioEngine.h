#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#include "AudioBlock.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>
#include <atomic>
#include <mutex>

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
    virtual bool startInput(const QString &deviceName, int requestedSampleRate = 48000);

    /**
     * @brief Stops audio capture and clears pending buffers.
     */
    virtual void stopInput();

    // Thread-safe, latest-request mailbox. Backends run only on this object's
    // thread; the GUI receives an acknowledgement instead of blocking on QAudio.
    quint64 requestStartInput(const QString &deviceName, int sampleRate);
    quint64 requestStopInput();
    void invalidateInputRequests();
    quint64 latestInputRequest() const { return m_inputRequestGeneration.load(); }


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
    void inputRequestFinished(quint64 request, bool startRequested, bool success);
    void audioBlockReady(const AudioBlock &block);
    void levelChanged(int percent, double db, double rms);
    void errorOccurred(const QString &message);
    void diagnosticMessage(const QString &message);
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
     * @brief Emits one diagnostic line without changing the audio path.
     */
    void reportDiagnostic(const QString &message);

    /**
     * @brief Resets callback and per-channel measurement counters.
     */
    void resetDiagnosticCounters();

    /**
     * @brief Measures every negotiated input channel for diagnostics only.
     */
    void accumulateInputDiagnostics(const char *raw, int frameCount, int bytesPerFrame);

    /**
     * @brief Emits a throttled per-channel RMS/peak/backend summary.
     */
    void maybeEmitPeriodicDiagnostics(bool force = false);

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

    std::atomic<int> m_sampleRate{48000};
    std::atomic<double> m_clockCorrectionPpm{0.0};
    std::atomic<int> m_inputVolumePercent{100};
    int m_channelCount = 1;
    int m_blockSamples = 1024;

    qint64 m_totalSamples = 0;

    // Continuous capture timeline. The first callback anchors sample index 0
    // to UTC/monotonic time; every later block timestamp is derived from its
    // sample index, so callback jitter cannot create artificial gaps/overlaps.
    QElapsedTimer m_captureClock;
    qint64 m_streamFirstUtcNs = 0;
    qint64 m_streamFirstMonotonicNs = 0;
    bool m_streamTimestampValid = false;
    quint64 m_captureSequence = 0;
    quint64 m_captureGeneration = 0;

    // Diagnostic-only counters.  These never select, mix, attenuate or reorder
    // channels; the production PCM conversion remains unchanged.
    QElapsedTimer m_diagnosticReportClock;
    QVector<double> m_diagnosticChannelSumSquares;
    QVector<int> m_diagnosticChannelPeaks;
    quint64 m_diagnosticMeasuredFrames = 0;
    quint64 m_diagnosticCallbackCount = 0;
    quint64 m_diagnosticBytesReceived = 0;
    quint64 m_diagnosticFramesReceived = 0;
    qint64 m_diagnosticLastCallbackBytes = 0;
    qint64 m_diagnosticLastCallbackFrames = 0;
    int m_diagnosticLastRemainderBytes = 0;

    struct InputRequest {
        quint64 id = 0;
        bool start = false;
        QString device;
        int rate = 48000;
    };
    quint64 enqueueInputRequest(bool start, const QString &device, int rate);
    void processInputRequests();
    std::atomic<quint64> m_inputRequestGeneration{0};
    std::mutex m_inputRequestMutex;
    InputRequest m_pendingInputRequest;
    bool m_inputRequestScheduled = false;
    std::atomic<bool> m_running{false};
};

#endif // AUDIOENGINE_H
