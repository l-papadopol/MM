#ifndef AUDIOBLOCK_H
#define AUDIOBLOCK_H

#include <QMetaType>
#include <QVector>
#include <QtGlobal>

/**
 * @brief Carries one normalized mono audio block through the DSP chain.
 *
 * Purpose:
 * - Store audio samples normalized in the range -1.0 ... +1.0.
 * - Keep the input sample rate associated with the block.
 * - Provide a monotonically increasing first-sample index.
 * - Timestamp the first sample at capture time, before queued DSP work.
 */
struct AudioBlock
{
    QVector<float> samples;
    int sampleRate = 48000;
    qint64 firstSampleIndex = 0;

    // Estimated UTC/monotonic time of samples[0], in nanoseconds.  These are
    // assigned by AudioEngine when bytes are read from the backend, not later
    // when a decoder happens to process the queued block.  A zero value means
    // that the producer has no capture timestamp (for example an old WAV test).
    qint64 firstSampleUtcNs = 0;
    qint64 firstSampleMonotonicNs = 0;
    quint64 captureSequence = 0;

    // Identifies one continuous AudioEngine start/stop capture session. Blocks
    // queued by an earlier RX session can therefore be rejected after a rapid
    // stop/start or mode transition instead of corrupting the current UTC slot.
    // Zero keeps compatibility with offline/test producers.
    quint64 captureGeneration = 0;
};

Q_DECLARE_METATYPE(AudioBlock)

#endif // AUDIOBLOCK_H
