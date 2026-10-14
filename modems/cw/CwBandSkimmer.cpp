#include "CwBandSkimmer.h"
#include "skimmer/SelectedToneCwTracker.h"
#include "../../dsp/text/SkimmerCqTracker.h"
#include <algorithm>
#include <cmath>
#include <set>

using namespace madmodem::cwskimmer;

struct CwBandSkimmer::Lane {
    SkimmerSpot spot;
    std::unique_ptr<SelectedToneCwTracker> decoder;
    SkimmerCqTracker cq;
    void append(const QString& text, double now) {
        spot.text = (spot.text + text).right(192);
        cq.append(text, now);
    }

};

CwBandSkimmer::CwBandSkimmer() { m_resampler.configure(12000); }
CwBandSkimmer::~CwBandSkimmer() = default;
void CwBandSkimmer::reset() {
    m_lanes.clear(); m_history.clear(); m_resampler.reset(); m_audioTime = 0;
}

void CwBandSkimmer::process(const QVector<float>& audio, int rate,
                          const std::vector<CwSkimmerChannelState>& carriers) {
    const auto reduced = m_resampler.process(audio, rate);
    QVector<float> samples; samples.reserve(reduced.size());
    for (double value : reduced) samples.append(static_cast<float>(value));
    m_audioTime += double(samples.size()) / 12000.0;
    m_history += samples;
    if (m_history.size() > 12000) m_history.remove(0, m_history.size() - 12000);
    auto ranked = carriers;
    std::sort(ranked.begin(), ranked.end(), [&](const auto& a, const auto& b) {
        auto score = [&](const auto& c) {
            const auto existing = m_lanes.find(c.channelIndex);
            const bool decoded = existing != m_lanes.end() && !existing->second->spot.text.trimmed().isEmpty();
            return c.snrDb + (decoded ? 6.f : 0.f);
        };
        return score(a) > score(b);
    });
    std::vector<CwSkimmerChannelState> selected;
    std::set<int> active;
    for (const auto& c : ranked) {
        const bool existing = m_lanes.count(c.channelIndex) != 0;
        if (!existing && (c.ageFrames < 4 || c.stableFrames < 3 || c.confidence < .45f || c.snrDb < 9.f)) continue;
        selected.push_back(c); active.insert(c.channelIndex);
        if (selected.size() >= kSkimmerChannels) break;
    }
    for (auto it = m_lanes.begin(); it != m_lanes.end();) {
        if (!active.count(it->first)) it = m_lanes.erase(it); else ++it;
    }
    for (const auto& carrier : selected) {
        auto found = m_lanes.find(carrier.channelIndex);
        bool created = false;
        if (found == m_lanes.end()) {
            if (carrier.ageFrames < 4 || carrier.stableFrames < 3 || carrier.confidence < 0.45f || carrier.snrDb < 9.f ||
                m_lanes.size() >= kSkimmerChannels) continue;
            auto lane = std::make_unique<Lane>();
            lane->spot.id = m_nextId++;
            lane->spot.frequencyHz = carrier.audioFrequencyHz;
            SelectedToneCwConfig cfg;
            cfg.toneHz = carrier.audioFrequencyHz;
            cfg.autoWpm = true; cfg.autoBandwidth = true;
            cfg.bandwidthHz = 100; cfg.afcRangeHz = 12;
            cfg.maxRollingText = 192;
            cfg.dedicatedTimingThread = false;
            lane->decoder = std::make_unique<SelectedToneCwTracker>(cfg);
            Lane* ptr = lane.get();
            lane->decoder->setCallback([this, ptr](const SelectedToneCwEvent& event) {
                ptr->spot.frequencyHz = event.trackedToneHz;
                ptr->append(QString::fromStdString(event.committedText).toUpper(), m_audioTime);
            });
            found = m_lanes.emplace(carrier.channelIndex, std::move(lane)).first;
            created = true;
        }
        auto& lane = *found->second;
        std::vector<SelectedToneCwInterferer> neighbours;
        for (const auto& other : carriers) {
            const double distance = std::abs(other.audioFrequencyHz - carrier.audioFrequencyHz);
            if (other.channelIndex != carrier.channelIndex && other.confidence >= .4f && distance > 18 && distance < 260)
                neighbours.push_back({other.audioFrequencyHz, other.confidence});
        }
        lane.decoder->setInterferers(neighbours);
        // Replay bounded pre-roll once on acquisition; never lose the start of
        // the first character while the FFT confirms a new carrier.
        const auto& input = created ? m_history : samples;
        lane.decoder->processFloatMono(input.constData(), input.size(), 12000);
        lane.cq.expire(m_audioTime);
        lane.spot.cqCall = lane.cq.call();
    }
}

QVector<SkimmerSpot> CwBandSkimmer::spots() const {
    QVector<SkimmerSpot> candidates, result;
    for (const auto& item : m_lanes) candidates.append(item.second->spot);
    // Keying skirts can temporarily acquire the same physical tone. Keep the
    // oldest decoded stream, never concatenate text from two receiver states.
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        if (a.text.isEmpty() != b.text.isEmpty()) return !a.text.isEmpty();
        return a.id < b.id;
    });
    for (const auto& spot : candidates) {
        bool duplicate = false;
        for (const auto& existing : result)
            if (std::abs(existing.frequencyHz - spot.frequencyHz) < 18) { duplicate = true; break; }
        if (!duplicate) result.append(spot);
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.frequencyHz < b.frequencyHz; });
    return result;
}
