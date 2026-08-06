#pragma once

#include "CwSkimmerTypes.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace madmodem::cwskimmer {

/**
 * Full-passband carrier discovery only. It never emits Morse characters.
 * Exact RX A/RX B decoding is performed by SelectedToneCwTracker.
 */
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

  void processFloatMono(const float* samples, std::size_t count, double sourceSampleRate);
  void processPcm16Mono(const int16_t* samples, std::size_t count, double sourceSampleRate);
  void flush();

  std::vector<CwSkimmerChannelState> channelStates() const;
  std::vector<CwSkimmerChannelState> priorityChannels(
      std::size_t maxCount = kDefaultPriorityCount) const;
  std::vector<uint32_t> lastMagnitudes() const;

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace madmodem::cwskimmer
