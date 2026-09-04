#ifndef DSPENGINE_H
#define DSPENGINE_H

#include "../audio/AudioBlock.h"
#include "WaterfallLeveler.h"

#include <QObject>
#include <QVector>

/**
 * @brief Performs first-stage DSP analysis for the receiver.
 *
 * Purpose:
 * - Receive normalized mono audio blocks from AudioEngine.
 * - Compute FFT spectra for the diagnostic waterfall.
 * - Estimate the strongest audio frequency in the visible passband.
 * - Keep waterfall processing intentionally lighter than modem decoding.
 *
 * Performance note:
 * - The engine is designed to run in a worker QThread owned by MainWindow.
 * - The decoder path never depends on waterfall FFT density; this keeps
 *   WAV/offline analysis and live decoding responsive on modest PCs.
 */
class DspEngine : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Creates the DSP engine.
     */
    explicit DspEngine(QObject *parent = nullptr);

public slots:
    /**
     * @brief Processes one audio block from the audio engine.
     */
    void processAudioBlock(const AudioBlock &block);

    /**
     * @brief Clears the internal DSP buffers.
     */
    void reset();

    /**
     * @brief Enables/disables the low-cost QSO peak metric extracted from the
     *        already-computed waterfall FFT.
     *
     * This never performs a second FFT and therefore cannot add a heavy DSP
     * operation to the modem/UI receive path. The slot runs in the DSP worker
     * thread together with waterfall processing.
     */
    void configureSignalPeakMetric(bool enabled, int lowHz, int highHz);

signals:
    /**
     * @brief Sends one normalized waterfall intensity line.
     */
    void waterfallLineReady(const QVector<quint8> &line, double minHz, double maxHz);

    /**
     * @brief Sends the strongest detected frequency in the visible band.
     */
    void dominantFrequencyChanged(double frequencyHz, double levelDb);

    /**
     * @brief Sends a signal-to-adjacent-noise metric for mechanical QSO peak
     *        tracking. Computed from the existing DSP FFT in the worker thread.
     */
    void signalPeakMetricReady(double metricDb, bool valid);

private:
    /**
     * @brief Runs one FFT analysis window.
     */
    void analyzeWindow(const QVector<float> &window, int sampleRate);

    /**
     * @brief Prepares the reusable Hann analysis window.
     */
    void ensureWindowTable();

    /**
     * @brief Runs an in-place radix-2 FFT.
     */
    void fft(QVector<double> &real, QVector<double> &imag);

private:
    QVector<float> m_fifo;
    QVector<double> m_windowTable;

    int m_fftSize = 8192;
    int m_hopSize = 2048;
    int m_columns = 1024;

    double m_minHz = 100.0;
    double m_maxHz = 3000.0;

    WaterfallLeveler m_waterfallLeveler;

    bool m_signalPeakMetricEnabled = false;
    int m_signalPeakLowHz = 0;
    int m_signalPeakHighHz = 0;
};

#endif // DSPENGINE_H
