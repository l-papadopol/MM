#ifndef DSPENGINE_H
#define DSPENGINE_H

#include "../audio/AudioBlock.h"

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

signals:
    /**
     * @brief Sends one normalized waterfall intensity line.
     */
    void waterfallLineReady(const QVector<quint8> &line, double minHz, double maxHz);

    /**
     * @brief Sends the strongest detected frequency in the visible band.
     */
    void dominantFrequencyChanged(double frequencyHz, double levelDb);

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

    QVector<double> m_smoothedWaterfall;

    double m_displayFloorDb = -110.0;
    double m_displayCeilingDb = -62.0;
    int m_levelWarmupWindows = 0;
};

#endif // DSPENGINE_H
