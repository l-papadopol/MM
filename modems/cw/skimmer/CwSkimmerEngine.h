#pragma once

#include "CwSkimmerTypes.h"
#include <memory>

namespace madmodem::cwskimmer {

class CwSkimmerEngine {
public:
  explicit CwSkimmerEngine(CwSkimmerConfig config = {});
  ~CwSkimmerEngine();

  CwSkimmerEngine(const CwSkimmerEngine&) = delete;
  CwSkimmerEngine& operator=(const CwSkimmerEngine&) = delete;
  CwSkimmerEngine(CwSkimmerEngine&&) noexcept;
  CwSkimmerEngine& operator=(CwSkimmerEngine&&) noexcept;

  void setCallback(CwSkimmerCallback callback);
  void setThresholdMultiplier(float thresholdMultiplier);
  const CwSkimmerConfig& config() const;

  // Feed normalized mono audio [-1.0, +1.0] at sourceSampleRate.
  // The engine resamples internally to 15 kHz, matching the assimilated DSP core.
  void processFloatMono(const float* samples, std::size_t count, double sourceSampleRate);

  // Feed signed 16-bit mono PCM at sourceSampleRate.
  void processPcm16Mono(const int16_t* samples, std::size_t count, double sourceSampleRate);

  // Forces all active channel buffers through the decoder.
  void flush();

  std::vector<CwSkimmerChannelState> channelStates() const;
  std::vector<CwSkimmerChannelState> priorityChannels(std::size_t maxCount = kDefaultPriorityCount) const;
  std::vector<uint32_t> lastMagnitudes() const;

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace madmodem::cwskimmer
