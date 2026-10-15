#pragma once
#include "../../dsp/text/SkimmerSpot.h"
#include "skimmer/CwSkimmerTypes.h"
#include "../../dsp/text/LinearResampler.h"
#include <map>
#include <memory>

// Owned exclusively by its processing worker.
class CwBandSkimmer {
public:
    CwBandSkimmer();
    ~CwBandSkimmer();
    void reset();
    void setChannelLimit(int limit);
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
    int m_channelLimit = 16;
};
