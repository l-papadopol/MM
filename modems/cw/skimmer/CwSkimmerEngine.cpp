#include "CwSkimmerEngine.h"
#include "core/cw_dsp.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <stdexcept>
#include <utility>

namespace madmodem::cwskimmer {
namespace {

float clamp01(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

float estimateConfidence(float snrDb, float wpm, const std::string& rolling, const std::string& partial) {
  float snrScore = clamp01((snrDb - 8.0f) / 18.0f);
  float textScore = clamp01(static_cast<float>(rolling.size() + partial.size()) / 24.0f);
  float wpmScore = (wpm >= 5.0f && wpm <= 45.0f) ? 1.0f : 0.65f;
  return clamp01((0.65f * snrScore) + (0.25f * textScore) + (0.10f * wpmScore));
}

std::string trimRolling(std::string text, std::size_t maxLen) {
  if (maxLen > 0 && text.size() > maxLen) {
    text.erase(0, text.size() - maxLen);
  }
  return text;
}

class CallbackDsp final : public c_cw_dsp {
public:
  explicit CallbackDsp(CwSkimmerConfig config) : m_config(std::move(config)) {
    set_threshold(m_config.thresholdMultiplier);
  }

  void setCallback(CwSkimmerCallback callback) { m_callback = std::move(callback); }
  void setTimestamp(double t) { m_timestampSec = t; }
  void setConfig(const CwSkimmerConfig& config) {
    m_config = config;
    set_threshold(m_config.thresholdMultiplier);
  }

  std::vector<CwSkimmerChannelState> states() const {
    std::vector<CwSkimmerChannelState> out;
    out.reserve(kSkimmerChannels);
    for (int ch = 0; ch < kSkimmerChannels; ++ch) {
      CwSkimmerChannelState st;
      st.channelIndex = ch;
      st.audioFrequencyHz = getChannelFrequency(ch);
      st.snrDb = const_cast<CallbackDsp*>(this)->get_snr(ch);
      st.wpm = const_cast<CallbackDsp*>(this)->get_WPM(ch);
      st.trainingPercent = const_cast<CallbackDsp*>(this)->get_buffer_percent(ch);
      st.rollingText = m_rolling[ch];
      st.partialText = m_partial[ch];
      st.confidence = estimateConfidence(st.snrDb, st.wpm, st.rollingText, st.partialText);
      out.push_back(std::move(st));
    }
    return out;
  }

  std::vector<uint32_t> magnitudes() const {
    uint32_t* raw = const_cast<CallbackDsp*>(this)->get_magnitudes();
    return std::vector<uint32_t>(raw, raw + (kFftSize / 2));
  }

private:
  double getChannelFrequency(int channel) const {
    return (static_cast<double>(channel * kChannelBins) + (static_cast<double>(kChannelBins) * 0.5)) * kBinHz;
  }

  void decode(uint16_t channel, std::string text, std::string partial) override {
    if (channel >= kSkimmerChannels) return;
    if (partial == "#" || partial == "~") partial.clear();
    if (text.empty() && partial.empty() && !m_config.emitEmptyPartials) return;

    m_rolling[channel] = trimRolling(m_rolling[channel] + text, m_config.maxRollingText);
    m_partial[channel] = std::move(partial);

    const float snr = get_snr(channel);
    const float wpm = get_WPM(channel);
    const float confidence = estimateConfidence(snr, wpm, m_rolling[channel], m_partial[channel]);

    if (text.empty() && snr < m_config.minEventSnrDb && !m_config.emitEmptyPartials) return;

    CwSkimmerEvent ev;
    ev.timestampSec = m_timestampSec;
    ev.channelIndex = static_cast<int>(channel);
    ev.audioFrequencyHz = getChannelFrequency(static_cast<int>(channel));
    ev.snrDb = snr;
    ev.wpm = wpm;
    ev.confidence = confidence;
    ev.committedText = std::move(text);
    ev.partialText = m_partial[channel];
    ev.rollingText = m_rolling[channel];

    if (m_callback) m_callback(ev);
  }

  CwSkimmerConfig m_config;
  CwSkimmerCallback m_callback;
  double m_timestampSec = 0.0;
  std::array<std::string, kSkimmerChannels> m_rolling{};
  std::array<std::string, kSkimmerChannels> m_partial{};
};

} // namespace

class CwSkimmerEngine::Impl {
public:
  explicit Impl(CwSkimmerConfig cfg) : config(std::move(cfg)), dsp(config) {}

  void processInternalSample(float normalizedSample) {
    float clipped = std::max(-1.0f, std::min(1.0f, normalizedSample));
    auto pcm = static_cast<int16_t>(std::lrint(clipped * 32767.0f));
    dsp.setTimestamp(processedInternalSamples / config.internalSampleRate);
    dsp.process_sample(pcm);
    ++processedInternalSamples;
  }

  CwSkimmerConfig config;
  CallbackDsp dsp;
  std::deque<float> resampleBuffer;
  double resampleReadPos = 0.0;
  double currentSourceRate = 0.0;
  uint64_t processedInternalSamples = 0;
};

CwSkimmerEngine::CwSkimmerEngine(CwSkimmerConfig config)
    : m_impl(std::make_unique<Impl>(std::move(config))) {}

CwSkimmerEngine::~CwSkimmerEngine() = default;
CwSkimmerEngine::CwSkimmerEngine(CwSkimmerEngine&&) noexcept = default;
CwSkimmerEngine& CwSkimmerEngine::operator=(CwSkimmerEngine&&) noexcept = default;

void CwSkimmerEngine::setCallback(CwSkimmerCallback callback) {
  m_impl->dsp.setCallback(std::move(callback));
}

void CwSkimmerEngine::setThresholdMultiplier(float thresholdMultiplier) {
  m_impl->config.thresholdMultiplier = thresholdMultiplier;
  m_impl->dsp.setConfig(m_impl->config);
}

const CwSkimmerConfig& CwSkimmerEngine::config() const { return m_impl->config; }

void CwSkimmerEngine::processFloatMono(const float* samples, std::size_t count, double sourceSampleRate) {
  if (!samples || count == 0) return;
  if (!(sourceSampleRate > 0.0)) throw std::invalid_argument("sourceSampleRate must be positive");

  if (m_impl->currentSourceRate != 0.0 && std::abs(m_impl->currentSourceRate - sourceSampleRate) > 1e-6) {
    m_impl->resampleBuffer.clear();
    m_impl->resampleReadPos = 0.0;
  }
  m_impl->currentSourceRate = sourceSampleRate;

  for (std::size_t i = 0; i < count; ++i) {
    m_impl->resampleBuffer.push_back(samples[i]);
  }

  const double step = sourceSampleRate / m_impl->config.internalSampleRate;
  while (m_impl->resampleReadPos + 1.0 < static_cast<double>(m_impl->resampleBuffer.size())) {
    const auto idx = static_cast<std::size_t>(std::floor(m_impl->resampleReadPos));
    const double frac = m_impl->resampleReadPos - static_cast<double>(idx);
    const float a = m_impl->resampleBuffer[idx];
    const float b = m_impl->resampleBuffer[idx + 1];
    const float y = static_cast<float>((1.0 - frac) * a + frac * b);
    m_impl->processInternalSample(y);
    m_impl->resampleReadPos += step;
  }

  const auto drop = static_cast<std::size_t>(std::floor(m_impl->resampleReadPos));
  if (drop > 0) {
    const auto actualDrop = std::min(drop, m_impl->resampleBuffer.size() > 1 ? m_impl->resampleBuffer.size() - 1 : 0);
    m_impl->resampleBuffer.erase(m_impl->resampleBuffer.begin(), m_impl->resampleBuffer.begin() + static_cast<std::ptrdiff_t>(actualDrop));
    m_impl->resampleReadPos -= static_cast<double>(actualDrop);
  }
}

void CwSkimmerEngine::processPcm16Mono(const int16_t* samples, std::size_t count, double sourceSampleRate) {
  if (!samples || count == 0) return;
  std::vector<float> tmp;
  tmp.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    tmp.push_back(static_cast<float>(samples[i]) / 32768.0f);
  }
  processFloatMono(tmp.data(), tmp.size(), sourceSampleRate);
}

void CwSkimmerEngine::flush() { m_impl->dsp.flush(); }

std::vector<CwSkimmerChannelState> CwSkimmerEngine::channelStates() const {
  return m_impl->dsp.states();
}

std::vector<CwSkimmerChannelState> CwSkimmerEngine::priorityChannels(std::size_t maxCount) const {
  auto states = channelStates();
  std::sort(states.begin(), states.end(), [](const auto& a, const auto& b) {
    const float as = a.confidence * 100.0f + std::max(0.0f, a.snrDb) + static_cast<float>(a.rollingText.size()) * 0.05f;
    const float bs = b.confidence * 100.0f + std::max(0.0f, b.snrDb) + static_cast<float>(b.rollingText.size()) * 0.05f;
    return as > bs;
  });
  if (states.size() > maxCount) states.resize(maxCount);
  return states;
}

std::vector<uint32_t> CwSkimmerEngine::lastMagnitudes() const {
  return m_impl->dsp.magnitudes();
}

} // namespace madmodem::cwskimmer
