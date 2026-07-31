#include "CwSkimmerEngine.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <deque>
#include <limits>
#include <utility>
#include <vector>

namespace madmodem::cwskimmer {
namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr std::size_t kFftSize = 8192U;
constexpr std::size_t kHopSize = 2048U;
constexpr double kEpsilon = 1.0e-18;

inline double clampd(double value, double low, double high) {
  return std::max(low, std::min(high, value));
}

void fft(std::vector<std::complex<double>>& a) {
  const std::size_t n = a.size();
  for (std::size_t i = 1, j = 0; i < n; ++i) {
    std::size_t bit = n >> 1U;
    for (; j & bit; bit >>= 1U) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(a[i], a[j]);
  }
  for (std::size_t len = 2; len <= n; len <<= 1U) {
    const double angle = -kTwoPi / static_cast<double>(len);
    const std::complex<double> wlen(std::cos(angle), std::sin(angle));
    for (std::size_t i = 0; i < n; i += len) {
      std::complex<double> w(1.0, 0.0);
      for (std::size_t j = 0; j < len / 2U; ++j) {
        const auto u = a[i + j];
        const auto v = a[i + j + len / 2U] * w;
        a[i + j] = u + v;
        a[i + j + len / 2U] = u - v;
        w *= wlen;
      }
    }
  }
}

struct Peak {
  double frequencyHz = 0.0;
  double snrDb = -99.0;
  double powerDb = -200.0;
};

} // namespace

class CwSkimmerEngine::Impl {
public:
  struct Lane {
    int id = -1;
    double frequencyHz = 0.0;
    double snrDb = -99.0;
    double confidence = 0.0;
    int ageFrames = 0;
    int missedFrames = 0;
  };

  explicit Impl(CwSkimmerConfig cfg) : config(std::move(cfg)) { sanitize(); }

  void sanitize() {
    config.minimumFrequencyHz = clampd(config.minimumFrequencyHz, 20.0, 5000.0);
    config.maximumFrequencyHz = clampd(config.maximumFrequencyHz,
                                       config.minimumFrequencyHz + 100.0, 12000.0);
    config.associationRadiusHz = clampd(config.associationRadiusHz, 4.0, 80.0);
    config.releaseTimeSec = clampd(config.releaseTimeSec, 0.5, 10.0);
    config.thresholdMultiplier = static_cast<float>(clampd(config.thresholdMultiplier, 1.5, 50.0));
    config.minEventSnrDb = static_cast<float>(clampd(config.minEventSnrDb, 0.0, 30.0));
  }

  void reset(double rate) {
    sampleRate = rate;
    buffer.clear();
    lanes.clear();
    magnitudes.clear();
    processedSamples = 0;
    nextLaneId = 0;
  }

  void process(const float* samples, std::size_t count, double rate) {
    if (!samples || count == 0U || !(rate > 1000.0)) return;
    if (std::abs(rate - sampleRate) > 1.0e-6) reset(rate);
    for (std::size_t i = 0; i < count; ++i) buffer.push_back(samples[i]);
    while (buffer.size() >= kFftSize) {
      analyzeFrame();
      const std::size_t drop = std::min(kHopSize, buffer.size());
      buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(drop));
      processedSamples += drop;
    }
  }

  void analyzeFrame() {
    std::vector<std::complex<double>> spectrum(kFftSize);
    for (std::size_t i = 0; i < kFftSize; ++i) {
      const double w = 0.5 - 0.5 * std::cos(kTwoPi * static_cast<double>(i) /
                                           static_cast<double>(kFftSize - 1U));
      spectrum[i] = std::complex<double>(static_cast<double>(buffer[i]) * w, 0.0);
    }
    fft(spectrum);

    const double binHz = sampleRate / static_cast<double>(kFftSize);
    const int firstBin = std::max(1, static_cast<int>(std::ceil(config.minimumFrequencyHz / binHz)));
    const int lastBin = std::min(static_cast<int>(kFftSize / 2U) - 2,
                                 static_cast<int>(std::floor(config.maximumFrequencyHz / binHz)));
    if (lastBin <= firstBin) return;

    std::vector<double> dbValues;
    dbValues.reserve(static_cast<std::size_t>(lastBin - firstBin + 1));
    magnitudes.assign(kFftSize / 2U, 0U);
    for (int bin = firstBin; bin <= lastBin; ++bin) {
      const double power = std::norm(spectrum[static_cast<std::size_t>(bin)]) + kEpsilon;
      const double db = 10.0 * std::log10(power);
      dbValues.push_back(db);
      const double scaled = clampd((db + 160.0) * 1000000.0, 0.0,
                                   static_cast<double>(std::numeric_limits<uint32_t>::max()));
      magnitudes[static_cast<std::size_t>(bin)] = static_cast<uint32_t>(scaled);
    }
    std::vector<double> floorCopy = dbValues;
    std::nth_element(floorCopy.begin(), floorCopy.begin() + floorCopy.size() / 2U,
                     floorCopy.end());
    const double noiseDb = floorCopy[floorCopy.size() / 2U];
    const double multiplierDb = 10.0 * std::log10(std::max(1.0f, config.thresholdMultiplier));
    const double thresholdDb = noiseDb + std::max<double>(config.minEventSnrDb, multiplierDb);

    std::vector<Peak> peaks;
    for (int bin = firstBin + 1; bin < lastBin; ++bin) {
      const double left = 10.0 * std::log10(std::norm(spectrum[static_cast<std::size_t>(bin - 1)]) + kEpsilon);
      const double center = 10.0 * std::log10(std::norm(spectrum[static_cast<std::size_t>(bin)]) + kEpsilon);
      const double right = 10.0 * std::log10(std::norm(spectrum[static_cast<std::size_t>(bin + 1)]) + kEpsilon);
      if (center < thresholdDb || center <= left || center < right) continue;
      const double denom = left - 2.0 * center + right;
      const double delta = std::abs(denom) > 1.0e-9
          ? clampd(0.5 * (left - right) / denom, -0.5, 0.5) : 0.0;
      Peak peak;
      peak.frequencyHz = (static_cast<double>(bin) + delta) * binHz;
      peak.powerDb = center;
      peak.snrDb = center - noiseDb;
      peaks.push_back(peak);
    }
    std::sort(peaks.begin(), peaks.end(), [](const Peak& a, const Peak& b) {
      return a.powerDb > b.powerDb;
    });
    if (peaks.size() > static_cast<std::size_t>(kSkimmerChannels * 2)) {
      peaks.resize(static_cast<std::size_t>(kSkimmerChannels * 2));
    }
    updateLanes(peaks);
  }

  void updateLanes(const std::vector<Peak>& peaks) {
    std::vector<bool> laneUsed(lanes.size(), false);
    std::vector<bool> peakUsed(peaks.size(), false);

    for (std::size_t p = 0; p < peaks.size(); ++p) {
      double bestDistance = config.associationRadiusHz;
      std::size_t bestLane = lanes.size();
      for (std::size_t l = 0; l < lanes.size(); ++l) {
        if (laneUsed[l]) continue;
        const double distance = std::abs(peaks[p].frequencyHz - lanes[l].frequencyHz);
        if (distance < bestDistance) {
          bestDistance = distance;
          bestLane = l;
        }
      }
      if (bestLane == lanes.size()) continue;
      auto& lane = lanes[bestLane];
      const double alpha = lane.ageFrames < 4 ? 0.45 : 0.20;
      lane.frequencyHz += alpha * (peaks[p].frequencyHz - lane.frequencyHz);
      lane.snrDb += 0.25 * (peaks[p].snrDb - lane.snrDb);
      lane.confidence = clampd(lane.confidence + 0.18, 0.0, 1.0);
      ++lane.ageFrames;
      lane.missedFrames = 0;
      laneUsed[bestLane] = true;
      peakUsed[p] = true;
      emitLane(lane);
    }

    for (std::size_t l = 0; l < lanes.size(); ++l) {
      if (laneUsed[l]) continue;
      ++lanes[l].missedFrames;
      lanes[l].confidence *= 0.86;
    }

    const int releaseFrames = std::max(2, static_cast<int>(std::ceil(
        config.releaseTimeSec * sampleRate / static_cast<double>(kHopSize))));
    lanes.erase(std::remove_if(lanes.begin(), lanes.end(), [releaseFrames](const Lane& lane) {
      return lane.missedFrames > releaseFrames || lane.confidence < 0.03;
    }), lanes.end());

    for (std::size_t p = 0; p < peaks.size() && lanes.size() < kSkimmerChannels; ++p) {
      if (peakUsed[p]) continue;
      Lane lane;
      lane.id = nextLaneId++;
      lane.frequencyHz = peaks[p].frequencyHz;
      lane.snrDb = peaks[p].snrDb;
      lane.confidence = clampd((peaks[p].snrDb - config.minEventSnrDb) / 12.0, 0.10, 0.65);
      lane.ageFrames = 1;
      lanes.push_back(lane);
      emitLane(lanes.back());
    }
  }

  void emitLane(const Lane& lane) {
    if (!callback) return;
    CwSkimmerEvent event;
    event.timestampSec = static_cast<double>(processedSamples) / sampleRate;
    event.channelIndex = lane.id;
    event.audioFrequencyHz = lane.frequencyHz;
    event.snrDb = static_cast<float>(lane.snrDb);
    event.confidence = static_cast<float>(lane.confidence);
    callback(event);
  }

  CwSkimmerConfig config;
  CwSkimmerCallback callback;
  double sampleRate = 0.0;
  std::vector<float> buffer;
  std::vector<Lane> lanes;
  std::vector<uint32_t> magnitudes;
  std::uint64_t processedSamples = 0;
  int nextLaneId = 0;
};

CwSkimmerEngine::CwSkimmerEngine(CwSkimmerConfig config)
    : m_impl(std::make_unique<Impl>(std::move(config))) {}
CwSkimmerEngine::~CwSkimmerEngine() = default;
CwSkimmerEngine::CwSkimmerEngine(CwSkimmerEngine&&) noexcept = default;
CwSkimmerEngine& CwSkimmerEngine::operator=(CwSkimmerEngine&&) noexcept = default;

void CwSkimmerEngine::setCallback(CwSkimmerCallback callback) {
  m_impl->callback = std::move(callback);
}
void CwSkimmerEngine::setThresholdMultiplier(float thresholdMultiplier) {
  m_impl->config.thresholdMultiplier = static_cast<float>(clampd(thresholdMultiplier, 1.5, 50.0));
}
const CwSkimmerConfig& CwSkimmerEngine::config() const { return m_impl->config; }
void CwSkimmerEngine::processFloatMono(const float* samples, std::size_t count,
                                       double sourceSampleRate) {
  m_impl->process(samples, count, sourceSampleRate);
}
void CwSkimmerEngine::processPcm16Mono(const int16_t* samples, std::size_t count,
                                       double sourceSampleRate) {
  if (!samples || count == 0U) return;
  std::vector<float> normalized(count);
  for (std::size_t i = 0; i < count; ++i) normalized[i] = samples[i] / 32768.0f;
  m_impl->process(normalized.data(), normalized.size(), sourceSampleRate);
}
void CwSkimmerEngine::flush() {}
std::vector<CwSkimmerChannelState> CwSkimmerEngine::channelStates() const {
  std::vector<CwSkimmerChannelState> out;
  out.reserve(m_impl->lanes.size());
  for (const auto& lane : m_impl->lanes) {
    CwSkimmerChannelState state;
    state.channelIndex = lane.id;
    state.audioFrequencyHz = lane.frequencyHz;
    state.snrDb = static_cast<float>(lane.snrDb);
    state.confidence = static_cast<float>(lane.confidence);
    state.ageFrames = static_cast<uint32_t>(lane.ageFrames);
    out.push_back(state);
  }
  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
    return a.audioFrequencyHz < b.audioFrequencyHz;
  });
  return out;
}
std::vector<CwSkimmerChannelState> CwSkimmerEngine::priorityChannels(std::size_t maxCount) const {
  auto out = channelStates();
  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
    const double scoreA = 100.0 * a.confidence + a.snrDb;
    const double scoreB = 100.0 * b.confidence + b.snrDb;
    return scoreA > scoreB;
  });
  if (out.size() > maxCount) out.resize(maxCount);
  return out;
}
std::vector<uint32_t> CwSkimmerEngine::lastMagnitudes() const { return m_impl->magnitudes; }

} // namespace madmodem::cwskimmer
