#include "modems/cw/skimmer/CwSkimmerEngine.h"
#include "modems/cw/skimmer/SelectedToneCwTracker.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

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

std::string normalize(std::string text) {
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

struct SignalOptions {
  double frequencyHz = 900.0;
  double wpm = 20.0;
  double amplitude = 0.35;
  double noiseAmplitude = 0.0;
  bool qsbNotches = false;
};

std::vector<float> synthesize(const std::string& text, const SignalOptions& options) {
  const int unit = static_cast<int>(std::lround(1.2 / options.wpm * kSampleRate));
  std::vector<float> samples(static_cast<std::size_t>(kSampleRate), 0.0f);
  std::mt19937 rng(0x4d4d4357U);
  std::uniform_real_distribution<float> noise(-options.noiseAmplitude,
                                               options.noiseAmplitude);

  auto appendSilence = [&](int count) {
    for (int i = 0; i < count; ++i) samples.push_back(noise(rng));
  };
  int markIndex = 0;
  auto appendMark = [&](int count) {
    for (int i = 0; i < count; ++i) {
      const double time = static_cast<double>(samples.size()) / kSampleRate;
      double level = options.amplitude;
      if (options.qsbNotches && count >= 2 * unit &&
          i > count / 2 - std::max(2, unit / 8) &&
          i < count / 2 + std::max(2, unit / 8) && (markIndex % 2 == 0)) {
        level *= 0.12;
      }
      samples.push_back(static_cast<float>(level * std::sin(2.0 * kPi *
          options.frequencyHz * time)) + noise(rng));
    }
    ++markIndex;
  };

  for (std::size_t character = 0; character < text.size(); ++character) {
    const char c = text[character];
    if (c == ' ') {
      appendSilence(7 * unit);
      continue;
    }
    const auto it = kMorse.find(c);
    if (it == kMorse.end()) throw std::runtime_error("unsupported test character");
    for (std::size_t element = 0; element < it->second.size(); ++element) {
      appendMark((it->second[element] == '.' ? 1 : 3) * unit);
      if (element + 1U < it->second.size()) appendSilence(unit);
    }
    if (character + 1U < text.size() && text[character + 1U] != ' ') {
      appendSilence(3 * unit);
    }
  }
  appendSilence(10 * unit);
  return samples;
}

std::string decode(const std::vector<float>& samples, double frequencyHz,
                   double hintWpm) {
  madmodem::cwskimmer::SelectedToneCwConfig config;
  config.toneHz = frequencyHz;
  config.bandwidthHz = 120.0;
  config.minSnrDb = 0.0;
  config.initialWpm = hintWpm;
  config.autoWpm = true;
  config.autoBandwidth = true;
  madmodem::cwskimmer::SelectedToneCwTracker tracker(config);
  std::string output;
  tracker.setCallback([&](const auto& event) { output += event.committedText; });
  for (std::size_t position = 0; position < samples.size(); position += 1024U) {
    const std::size_t count = std::min<std::size_t>(1024U, samples.size() - position);
    tracker.processFloatMono(samples.data() + position, count, kSampleRate);
  }
  tracker.flush();
  return normalize(output);
}

void requireEqual(const std::string& name, const std::string& actual,
                  const std::string& expected) {
  if (actual != expected) {
    std::cerr << "FAIL " << name << "\nexpected: " << expected
              << "\nactual:   " << actual << '\n';
    throw std::runtime_error(name);
  }
  std::cout << "PASS " << name << ": " << actual << '\n';
}

void testMessage(double wpm, double hintWpm, bool qsb, double noise) {
  const std::string expected = "CQ CQ DE IZ6NNH 599";
  SignalOptions options;
  options.frequencyHz = 1329.0;
  options.wpm = wpm;
  options.noiseAmplitude = noise;
  options.qsbNotches = qsb;
  const auto samples = synthesize(expected, options);
  requireEqual("message-" + std::to_string(static_cast<int>(wpm)) +
                   (qsb ? "-qsb" : ""),
               decode(samples, options.frequencyHz, hintWpm), expected);
}

void testRealtimeCommitAndStableWpm() {
  const std::string expected = "CQ CQ DE IZ6NNH";
  SignalOptions options;
  options.frequencyHz = 1293.0;
  options.wpm = 27.0;
  options.amplitude = 0.35;
  const auto samples = synthesize(expected, options);

  madmodem::cwskimmer::SelectedToneCwConfig config;
  config.toneHz = options.frequencyHz;
  config.bandwidthHz = 120.0;
  config.minSnrDb = 0.0;
  config.initialWpm = 20.0;
  config.autoWpm = true;
  config.autoBandwidth = true;
  madmodem::cwskimmer::SelectedToneCwTracker tracker(config);

  std::string output;
  double firstCommitSec = -1.0;
  double previousWpm = 20.0;
  double maximumStep = 0.0;
  double finalWpm = 20.0;
  tracker.setCallback([&](const auto& event) {
    if (event.committedText.empty()) return;
    if (firstCommitSec < 0.0) firstCommitSec = event.timestampSec;
    output += event.committedText;
    maximumStep = std::max(maximumStep, std::abs(event.wpm - previousWpm));
    previousWpm = event.wpm;
    finalWpm = event.wpm;
  });

  for (std::size_t position = 0; position < samples.size(); position += 1024U) {
    const std::size_t count = std::min<std::size_t>(1024U, samples.size() - position);
    tracker.processFloatMono(samples.data() + position, count, kSampleRate);
  }
  tracker.flush();

  requireEqual("realtime-text", normalize(output), expected);
  // The synthesized signal begins at 1.0 s.  The first completed character
  // must be visible within one second, not after a phrase-level flush.
  if (!(firstCommitSec >= 1.0 && firstCommitSec <= 2.0))
    throw std::runtime_error("realtime-first-commit");
  if (maximumStep > 1.55)
    throw std::runtime_error("stable-wpm-step");
  if (finalWpm < 25.0 || finalWpm > 29.0)
    throw std::runtime_error("stable-wpm-final");
  std::cout << "PASS realtime-first-commit: " << firstCommitSec
            << " s, final " << finalWpm << " WPM\n";
}

void testPhaseJitteredCarrier() {
  const std::string expected = "CQ CQ DE IZ6NNH";
  constexpr double frequencyHz = 1293.0;
  constexpr double wpm = 24.0;
  constexpr double amplitude = 0.45;
  const int unit = static_cast<int>(std::lround(1.2 / wpm * kSampleRate));
  std::vector<float> samples(static_cast<std::size_t>(kSampleRate), 0.0f);

  auto appendSilence = [&](int count) {
    samples.insert(samples.end(), static_cast<std::size_t>(count), 0.0f);
  };
  auto appendMark = [&](int count) {
    for (int i = 0; i < count; ++i) {
      const double time = static_cast<double>(samples.size()) / kSampleRate;
      const int phaseBlock = i / std::max(1, kSampleRate / 125);
      const double phaseJump = (phaseBlock % 2 == 0) ? -0.55 : 0.55;
      samples.push_back(static_cast<float>(amplitude * std::sin(
          2.0 * kPi * frequencyHz * time + phaseJump)));
    }
  };

  for (std::size_t character = 0; character < expected.size(); ++character) {
    const char c = expected[character];
    if (c == ' ') {
      appendSilence(7 * unit);
      continue;
    }
    const auto it = kMorse.find(c);
    if (it == kMorse.end()) throw std::runtime_error("unsupported phase-jitter character");
    for (std::size_t element = 0; element < it->second.size(); ++element) {
      appendMark((it->second[element] == '.' ? 1 : 3) * unit);
      if (element + 1U < it->second.size()) appendSilence(unit);
    }
    if (character + 1U < expected.size() && expected[character + 1U] != ' ')
      appendSilence(3 * unit);
  }
  appendSilence(10 * unit);

  requireEqual("phase-jittered-carrier",
               decode(samples, frequencyHz, 20.0), expected);
}

void testNoiseOnly() {
  std::mt19937 rng(77U);
  std::normal_distribution<float> noise(0.0f, 0.025f);
  std::vector<float> samples(static_cast<std::size_t>(4 * kSampleRate));
  for (float& sample : samples) sample = noise(rng);
  requireEqual("noise-only", decode(samples, 1329.0, 20.0), "");
}

void testCarrierSeparation() {
  const double firstHz = 900.0;
  const double secondHz = 925.0;
  std::vector<float> samples(static_cast<std::size_t>(3 * kSampleRate));
  for (std::size_t i = 0; i < samples.size(); ++i) {
    const double t = static_cast<double>(i) / kSampleRate;
    samples[i] = static_cast<float>(0.22 * std::sin(2.0 * kPi * firstHz * t) +
                                    0.19 * std::sin(2.0 * kPi * secondHz * t));
  }
  madmodem::cwskimmer::CwSkimmerConfig config;
  config.minimumFrequencyHz = 700.0;
  config.maximumFrequencyHz = 1100.0;
  config.associationRadiusHz = 12.0;
  config.minEventSnrDb = 5.0f;
  madmodem::cwskimmer::CwSkimmerEngine scanner(config);
  scanner.processFloatMono(samples.data(), samples.size(), kSampleRate);
  const auto lanes = scanner.channelStates();
  const auto near = [&](double frequency) {
    return std::any_of(lanes.begin(), lanes.end(), [&](const auto& lane) {
      return std::abs(lane.audioFrequencyHz - frequency) < 6.0 && lane.confidence > 0.20f;
    });
  };
  if (!near(firstHz) || !near(secondHz)) {
    throw std::runtime_error("carrier-separation-25Hz");
  }
  std::cout << "PASS carrier-separation-25Hz\n";
}

} // namespace

int main() {
  try {
    testMessage(20.0, 20.0, false, 0.0);
    testMessage(30.0, 18.0, false, 0.0);
    testMessage(22.0, 22.0, true, 0.004);
    testRealtimeCommitAndStableWpm();
    testPhaseJitteredCarrier();
    testNoiseOnly();
    testCarrierSeparation();
  } catch (const std::exception& error) {
    std::cerr << "CW native regression failed: " << error.what() << '\n';
    return 1;
  }
  std::cout << "CW native regression: all checks passed\n";
  return 0;
}
