#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace madmodem::cwskimmer {

constexpr int kDefaultPriorityCount = 8;
constexpr int kSkimmerChannels = 16;

struct CwSkimmerConfig {
  double minimumFrequencyHz = 100.0;
  double maximumFrequencyHz = 3500.0;
  double associationRadiusHz = 18.0;
  double releaseTimeSec = 3.0;
  float thresholdMultiplier = 8.0f;
  float minEventSnrDb = 7.0f;
};

struct CwSkimmerEvent {
  double timestampSec = 0.0;
  int channelIndex = -1;
  double audioFrequencyHz = 0.0;
  float snrDb = -99.0f;
  float confidence = 0.0f;
};

struct CwSkimmerChannelState {
  int channelIndex = -1;
  double audioFrequencyHz = 0.0;
  float snrDb = -99.0f;
  float confidence = 0.0f;
  uint32_t ageFrames = 0;
};

using CwSkimmerCallback = std::function<void(const CwSkimmerEvent&)>;

} // namespace madmodem::cwskimmer
