#include "LinearResampler.h"

#include <algorithm>

void LinearResampler::configure(double outputSampleRate)
{
    m_outputSampleRate = outputSampleRate > 1.0 ? outputSampleRate : 8000.0;
}

void LinearResampler::reset()
{
    m_inputSampleRate = 0;
    m_stepInputSamples = 6.0;
    m_absoluteInputIndex = 0.0;
    m_nextOutputInputIndex = 0.0;
    m_previousSample = 0.0;
    m_havePreviousSample = false;
}

QVector<double> LinearResampler::process(const QVector<float> &input, int inputSampleRate)
{
    QVector<double> output;
    if (input.isEmpty() || inputSampleRate <= 0) {
        return output;
    }

    if (inputSampleRate != m_inputSampleRate) {
        m_inputSampleRate = inputSampleRate;
        m_stepInputSamples = static_cast<double>(inputSampleRate) / m_outputSampleRate;
        m_absoluteInputIndex = 0.0;
        m_nextOutputInputIndex = 0.0;
        m_havePreviousSample = false;
    }

    output.reserve(static_cast<int>((static_cast<double>(input.size()) * m_outputSampleRate /
                                     static_cast<double>(inputSampleRate)) + 4.0));

    for (float raw : input) {
        const double currentSample = std::max(-1.0, std::min(static_cast<double>(raw), 1.0));
        const double currentIndex = m_absoluteInputIndex;

        if (!m_havePreviousSample) {
            m_previousSample = currentSample;
            m_havePreviousSample = true;
            if (m_nextOutputInputIndex <= currentIndex) {
                output.append(currentSample);
                m_nextOutputInputIndex += m_stepInputSamples;
            }
            m_absoluteInputIndex += 1.0;
            continue;
        }

        const double previousIndex = currentIndex - 1.0;
        while (m_nextOutputInputIndex <= currentIndex) {
            const double t = std::max(0.0, std::min(m_nextOutputInputIndex - previousIndex, 1.0));
            const double y = m_previousSample + (t * (currentSample - m_previousSample));
            output.append(y);
            m_nextOutputInputIndex += m_stepInputSamples;
        }

        m_previousSample = currentSample;
        m_absoluteInputIndex += 1.0;
    }

    return output;
}

double LinearResampler::outputSampleRate() const
{
    return m_outputSampleRate;
}
