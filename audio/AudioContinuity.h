#pragma once
#include "AudioBlock.h"

// Detect discontinuities before stateful filters/decoders consume samples.
class AudioContinuity {
public:
    bool accept(const AudioBlock &block) {
        const bool gap = m_valid && (block.sampleRate != m_rate ||
            block.captureGeneration != m_generation || block.firstSampleIndex != m_next);
        m_valid = true;
        m_rate = block.sampleRate;
        m_generation = block.captureGeneration;
        m_next = block.firstSampleIndex + block.samples.size();
        return gap;
    }
    void reset() { m_valid = false; }
private:
    bool m_valid = false;
    int m_rate = 0;
    quint64 m_generation = 0;
    qint64 m_next = 0;
};
