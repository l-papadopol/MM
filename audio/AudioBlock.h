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
 */
struct AudioBlock
{
    QVector<float> samples;
    int sampleRate = 48000;
    qint64 firstSampleIndex = 0;
};

Q_DECLARE_METATYPE(AudioBlock)

#endif // AUDIOBLOCK_H
