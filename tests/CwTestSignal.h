#pragma once
#include "modems/cw/skimmer/CwCarrierDiscriminator.h"
#include "modems/cw/skimmer/CwMorseBeamDecoder.h"
#include "modems/cw/skimmer/CwRelativeTimingDecoder.h"
#include "modems/cw/skimmer/CwSkimmerEngine.h"
#include "modems/cw/skimmer/SelectedToneCwTracker.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace cwtest {

constexpr double kPi = 3.1415926535897932384626433832795;
constexpr int kSampleRate = 48000;

const std::map<char, std::string> kMorse = {
    {'A', ".-"}, {'B', "-..."}, {'C', "-.-."}, {'D', "-.."}, {'E', "."},
    {'F', "..-."}, {'G', "--."}, {'H', "...."}, {'I', ".."}, {'J', ".---"},
    {'K', "-.-"}, {'L', ".-.."}, {'M', "--"}, {'N', "-."}, {'O', "---"},
    {'P', ".--."}, {'Q', "--.-"}, {'R', ".-."}, {'S', "..."}, {'T', "-"},
    {'U', "..-"}, {'V', "...-"}, {'W', ".--"}, {'X', "-..-"}, {'Y', "-.--"},
    {'Z', "--.."}, {'0', "-----"}, {'1', ".----"}, {'2', "..---"},
    {'3', "...--"}, {'4', "....-"}, {'5', "....."}, {'6', "-...."},
    {'7', "--..."}, {'8', "---.."}, {'9', "----."}
};

inline std::string normalize(std::string text) {
  std::string out;
  bool previousSpace = true;
  for (char c : text) {
    const bool space = c == ' ' || c == '\n' || c == '\r' || c == '\t';
    if (space) {
      if (!previousSpace && !out.empty()) out.push_back(' ');
      previousSpace = true;
    } else {
      out.push_back(c);
      previousSpace = false;
    }
  }
  while (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}

inline double editSimilarity(const std::string& left, const std::string& right) {
  if (left.empty() && right.empty()) return 1.0;
  std::vector<std::size_t> previous(right.size() + 1U);
  std::vector<std::size_t> current(right.size() + 1U);
  for (std::size_t j = 0U; j <= right.size(); ++j) previous[j] = j;
  for (std::size_t i = 1U; i <= left.size(); ++i) {
    current[0U] = i;
    for (std::size_t j = 1U; j <= right.size(); ++j) {
      const std::size_t substitution = previous[j - 1U] +
          (left[i - 1U] == right[j - 1U] ? 0U : 1U);
      current[j] = std::min({previous[j] + 1U, current[j - 1U] + 1U,
                             substitution});
    }
    previous.swap(current);
  }
  const double scale = static_cast<double>(
      std::max(left.size(), right.size()));
  return 1.0 - static_cast<double>(previous.back()) /
                   std::max(1.0, scale);
}

struct SignalOptions {
  double frequencyHz = 900.0;
  double wpm = 20.0;
  double amplitude = 0.35;
  double noiseAmplitude = 0.0;
  double markJitter = 0.0;
  double spaceJitter = 0.0;
  double dashRatio = 3.0;
  double wordSpaceUnits = 7.0;
  double riseTimeMs = 0.0;
  bool qsb = false;
  bool impulsiveNoise = false;
  double interfererOffsetHz = 0.0;
  double interfererAmplitude = 0.0;
  unsigned seed = 0x4d4d4357U;
  bool portableRandom = false;
};

inline double portableUniform01(std::mt19937& rng) {
  const std::uint64_t high = static_cast<std::uint64_t>(rng() >> 5U);
  const std::uint64_t low = static_cast<std::uint64_t>(rng() >> 6U);
  return static_cast<double>((high << 26U) | low) /
         static_cast<double>(std::uint64_t{1} << 53U);
}

class PortableNormalDistribution {
 public:
  PortableNormalDistribution(double mean, double standardDeviation)
      : m_mean(mean), m_standardDeviation(standardDeviation) {}

  double operator()(std::mt19937& rng) {
    if (m_standardDeviation == 0.0) return m_mean;
    if (m_hasSpare) {
      m_hasSpare = false;
      return m_mean + m_standardDeviation * m_spare;
    }
    double x = 0.0;
    double y = 0.0;
    double radiusSquared = 0.0;
    do {
      x = 2.0 * portableUniform01(rng) - 1.0;
      y = 2.0 * portableUniform01(rng) - 1.0;
      radiusSquared = x * x + y * y;
    } while (radiusSquared <= 0.0 || radiusSquared >= 1.0);
    const double scale = std::sqrt(-2.0 * std::log(radiusSquared) /
                                   radiusSquared);
    m_spare = y * scale;
    m_hasSpare = true;
    return m_mean + m_standardDeviation * x * scale;
  }

 private:
  double m_mean = 0.0;
  double m_standardDeviation = 1.0;
  double m_spare = 0.0;
  bool m_hasSpare = false;
};

inline std::vector<float> synthesize(const std::string& text, const SignalOptions& options) {
  const double unitSec = 1.2 / options.wpm;
  std::mt19937 rng(options.seed);
  std::normal_distribution<double> markVariation(1.0, options.markJitter);
  std::normal_distribution<double> spaceVariation(1.0, options.spaceJitter);
  std::normal_distribution<double> noise(0.0, options.noiseAmplitude);
  std::uniform_real_distribution<double> impulseChance(0.0, 1.0);
  PortableNormalDistribution portableMarkVariation(1.0, options.markJitter);
  PortableNormalDistribution portableSpaceVariation(1.0, options.spaceJitter);
  PortableNormalDistribution portableNoise(0.0, options.noiseAmplitude);
  const auto sampleMarkVariation = [&]() {
    return options.portableRandom ? portableMarkVariation(rng)
                                  : markVariation(rng);
  };
  const auto sampleSpaceVariation = [&]() {
    return options.portableRandom ? portableSpaceVariation(rng)
                                  : spaceVariation(rng);
  };
  const auto sampleNoise = [&]() {
    return options.portableRandom ? portableNoise(rng) : noise(rng);
  };
  const auto sampleUniform = [&]() {
    return options.portableRandom ? portableUniform01(rng)
                                  : impulseChance(rng);
  };

  std::vector<float> samples;
  samples.reserve(static_cast<std::size_t>(20 * kSampleRate));

  auto sampleBackground = [&](double time) {
    double value = sampleNoise();
    if (options.interfererAmplitude > 0.0 && std::abs(options.interfererOffsetHz) > 0.1) {
      value += options.interfererAmplitude * std::sin(
          2.0 * kPi * (options.frequencyHz + options.interfererOffsetHz) * time);
    }
    if (options.impulsiveNoise && sampleUniform() < 0.0008)
      value += sampleUniform() < 0.5 ? -0.8 : 0.8;
    return value;
  };

  auto appendSpace = [&](double units) {
    const double factor = std::clamp(sampleSpaceVariation(), 0.55, 1.65);
    const int count = std::max(1, static_cast<int>(std::lround(
        units * unitSec * factor * kSampleRate)));
    for (int i = 0; i < count; ++i) {
      const double time = static_cast<double>(samples.size()) / kSampleRate;
      samples.push_back(static_cast<float>(sampleBackground(time)));
    }
  };

  int markNumber = 0;
  auto appendMark = [&](bool dash) {
    const double nominalUnits = dash ? options.dashRatio : 1.0;
    const double factor = std::clamp(sampleMarkVariation(), 0.55, 1.65);
    const int count = std::max(1, static_cast<int>(std::lround(
        nominalUnits * unitSec * factor * kSampleRate)));
    for (int i = 0; i < count; ++i) {
      const double time = static_cast<double>(samples.size()) / kSampleRate;
      double level = options.amplitude;
      if (options.qsb) {
        const double slow = 0.58 + 0.42 * std::sin(2.0 * kPi * 0.19 * time + 0.3);
        level *= std::max(0.10, slow);
        if (dash && markNumber % 3 == 1 && i > count / 2 - count / 12 && i < count / 2 + count / 12)
          level *= 0.16;
      }
      if (options.riseTimeMs > 0.0) {
        const double ramp = std::min(1.0, std::min(i, count - 1 - i) /
            (options.riseTimeMs * kSampleRate / 1000.0));
        level *= 0.5 - 0.5 * std::cos(kPi * ramp);
      }
      const double wanted = level * std::sin(2.0 * kPi * options.frequencyHz * time);
      samples.push_back(static_cast<float>(wanted + sampleBackground(time)));
    }
    ++markNumber;
  };

  appendSpace(12.0);
  for (std::size_t character = 0; character < text.size(); ++character) {
    const char c = text[character];
    if (c == ' ') continue;
    const auto it = kMorse.find(c);
    if (it == kMorse.end()) throw std::runtime_error("unsupported test character");
    for (std::size_t element = 0; element < it->second.size(); ++element) {
      appendMark(it->second[element] == '-');
      if (element + 1U < it->second.size()) appendSpace(1.0);
    }
    if (character + 1U < text.size()) {
      appendSpace(text[character + 1U] == ' ' ? options.wordSpaceUnits : 3.0);
    }
  }
  appendSpace(16.0);
  return samples;
}


} // namespace cwtest
