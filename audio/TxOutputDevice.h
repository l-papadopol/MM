#pragma once
#include "AudioBlock.h"
#include "../core/tx/TxModulator.h"
#include <QIODevice>
#include <QMetaObject>
#include <QtMath>
#include <atomic>

/**
 * @brief Pull-device used by Qt audio output to read generated PCM samples.
 */
class TxOutputDevice final : public QIODevice
{
    Q_OBJECT

public:
    /**
     * @brief Creates a pull-device connected to one modulator.
     */
    TxOutputDevice(TxModulator *modulator,
                   int sampleRate,
                   int volumePercent,
                   QObject *parent = nullptr)
        : QIODevice(parent),
          m_modulator(modulator),
          m_sampleRate(sampleRate),
          m_volumePercent(qBound(0, volumePercent, 100))
    {
    }

    /**
     * @brief Returns sequential device status.
     */
    bool exhausted() const { return m_exhausted.load(std::memory_order_acquire); }

    bool atEnd() const override { return exhausted(); }

    bool isSequential() const override
    {
        return true;
    }

    /**
     * @brief Opens the device for audio-output reads.
     */
    bool start()
    {
        m_totalSamples = 0;
        m_finishQueued = false;
        m_tailSamplesRemaining = -1;
        m_samplesSinceRttyStateEmit = 0;
        m_exhausted.store(false, std::memory_order_release);
        return open(QIODevice::ReadOnly);
    }

    void setVolumePercent(int percent)
    {
        m_volumePercent = qBound(0, percent, 100);
    }

signals:
    void audioBlockReady(const AudioBlock &block);
    void rttyToneStateChanged(bool mark, double progress);
    void progressChanged(double progress);
    void finished();

protected:
    /**
     * @brief Provides signed 16-bit little-endian PCM to Qt audio output.
     */
    qint64 readData(char *data, qint64 maxSize) override
    {
        if (data == nullptr || maxSize <= 0 || m_modulator == nullptr || exhausted()) {
            return 0;
        }

        const qint64 alignedBytes = maxSize - (maxSize % 2);
        const int requestedSamples = static_cast<int>(alignedBytes / 2);

        if (requestedSamples <= 0) {
            return 0;
        }

        QVector<float> samples(requestedSamples, 0.0f);
        int generatedSamples = 0;

        if (!m_modulator->isFinished()) {
            generatedSamples = m_modulator->generate(samples.data(), requestedSamples);
            generatedSamples = qBound(0, generatedSamples, requestedSamples);
        }

        if (m_modulator->isFinished() && m_tailSamplesRemaining < 0) {
            const int requestedTail = m_modulator->trailingSilenceSamples();
            m_tailSamplesRemaining = requestedTail >= 0 ? requestedTail : qMax(1, m_sampleRate / 3);
        }
        const int silenceSamples = m_tailSamplesRemaining > 0
            ? qMin(m_tailSamplesRemaining, requestedSamples - generatedSamples) : 0;
        if (m_tailSamplesRemaining > 0) m_tailSamplesRemaining -= silenceSamples;
        const int returnedSamples = generatedSamples + silenceSamples;

        qint16 *pcm = reinterpret_cast<qint16 *>(data);

        const double gain = static_cast<double>(m_volumePercent.load(std::memory_order_relaxed)) / 100.0;
        for (int i = 0; i < returnedSamples; ++i) {
            const double bounded = qBound(-1.0, static_cast<double>(samples.at(i)) * gain, 1.0);
            pcm[i] = static_cast<qint16>(qRound(bounded * 32767.0));
        }

        if (generatedSamples > 0) {
            AudioBlock block;
            block.sampleRate = m_sampleRate;
            block.firstSampleIndex = m_totalSamples;
            block.samples = samples.mid(0, generatedSamples);
            m_totalSamples += generatedSamples;

            emit audioBlockReady(block);
            const double progress = m_modulator->progress();
            emit progressChanged(progress);

            m_samplesSinceRttyStateEmit += generatedSamples;
            const int stateEmitInterval = qMax(1, m_sampleRate / 15);
            if (m_samplesSinceRttyStateEmit >= stateEmitInterval) {
                m_samplesSinceRttyStateEmit = 0;
                bool markState = true;
                if (m_modulator->rttyToneState(&markState)) {
                    emit rttyToneStateChanged(markState, progress);
                }
            }
        }

        if (m_modulator->isFinished() &&
            m_tailSamplesRemaining <= 0 &&
            !m_finishQueued) {
            m_finishQueued = true;
            m_exhausted.store(true, std::memory_order_release);
            QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
        }

        return static_cast<qint64>(returnedSamples) * 2;
    }

    /**
     * @brief Rejects writes because this is a read-only source device.
     */
    qint64 writeData(const char *data, qint64 maxSize) override
    {
        Q_UNUSED(data)
        Q_UNUSED(maxSize)
        return -1;
    }

private:
    TxModulator *m_modulator = nullptr;
    int m_sampleRate = 48000;
    qint64 m_totalSamples = 0;
    int m_samplesSinceRttyStateEmit = 0;
    bool m_finishQueued = false;
    int m_tailSamplesRemaining = -1;
    std::atomic<int> m_volumePercent{100};
    std::atomic<bool> m_exhausted{false};
};

