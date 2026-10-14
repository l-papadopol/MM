#pragma once
#include "../../dsp/text/SkimmerSpot.h"
#include "skimmer/CwSkimmerTypes.h"
#include "../../dsp/text/LinearResampler.h"
#include <map>
#include <memory>

// Owned exclusively by CwDecoder on the existing RX worker.
class CwBandSkimmer {
public:
    CwBandSkimmer();
    ~CwBandSkimmer();
    void reset();
    void process(const QVector<float>& audio, int sampleRate,
                 const std::vector<madmodem::cwskimmer::CwSkimmerChannelState>& carriers);
    QVector<SkimmerSpot> spots() const;
private:
    struct Lane;
    std::map<int, std::unique_ptr<Lane>> m_lanes;
    LinearResampler m_resampler;
    QVector<float> m_history;
    quint64 m_nextId = 1;
    double m_audioTime = 0;
};
