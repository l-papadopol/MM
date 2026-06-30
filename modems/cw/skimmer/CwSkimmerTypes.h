#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace madmodem::cwskimmer {

constexpr int kDefaultPriorityCount = 2;
constexpr int kSkimmerChannels = 6;
constexpr double kInternalSampleRate = 15000.0;
constexpr double kFftSampleRate = 7500.0;
constexpr int kFftSize = 64;
constexpr double kBinHz = kFftSampleRate / static_cast<double>(kFftSize);
constexpr int kChannelBins = 5;

struct CwSkimmerConfig {
  double internalSampleRate = kInternalSampleRate;
  float thresholdMultiplier = 9.0f;
  float minEventSnrDb = 12.0f;
  std::size_t maxRollingText = 160;
  bool emitEmptyPartials = false;
};

struct CwSkimmerEvent {
  double timestampSec = 0.0;
  int channelIndex = -1;
  double audioFrequencyHz = 0.0;
  float snrDb = -99.0f;
  float confidence = 0.0f;
  float wpm = 0.0f;
  std::string committedText;
  std::string partialText;
  std::string rollingText;
};

struct CwSkimmerChannelState {
  int channelIndex = -1;
  double audioFrequencyHz = 0.0;
  float snrDb = -99.0f;
  float confidence = 0.0f;
  float wpm = 0.0f;
  uint32_t trainingPercent = 0;
  std::string rollingText;
  std::string partialText;
};

using CwSkimmerCallback = std::function<void(const CwSkimmerEvent&)>;

} // namespace madmodem::cwskimmer
