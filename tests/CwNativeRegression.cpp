#include "modems/cw/skimmer/CwCarrierDiscriminator.h"
#include "modems/cw/skimmer/CwMorseBeamDecoder.h"
#include "modems/cw/skimmer/CwRelativeTimingDecoder.h"
#include "modems/cw/skimmer/CwSkimmerEngine.h"
#include "modems/cw/skimmer/SelectedToneCwTracker.h"

#include <algorithm>
#include <array>
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
  double markJitter = 0.0;
  double spaceJitter = 0.0;
  double dashRatio = 3.0;
  double wordSpaceUnits = 7.0;
  bool qsb = false;
  bool impulsiveNoise = false;
  double interfererOffsetHz = 0.0;
  double interfererAmplitude = 0.0;
  unsigned seed = 0x4d4d4357U;
};

std::vector<float> synthesize(const std::string& text, const SignalOptions& options) {
  const double unitSec = 1.2 / options.wpm;
  std::mt19937 rng(options.seed);
  std::normal_distribution<double> markVariation(1.0, options.markJitter);
  std::normal_distribution<double> spaceVariation(1.0, options.spaceJitter);
  std::normal_distribution<double> noise(0.0, options.noiseAmplitude);
  std::uniform_real_distribution<double> impulseChance(0.0, 1.0);

  std::vector<float> samples;
  samples.reserve(static_cast<std::size_t>(20 * kSampleRate));

  auto sampleBackground = [&](double time) {
    double value = noise(rng);
    if (options.interfererAmplitude > 0.0 && std::abs(options.interfererOffsetHz) > 0.1) {
      value += options.interfererAmplitude * std::sin(
          2.0 * kPi * (options.frequencyHz + options.interfererOffsetHz) * time);
    }
    if (options.impulsiveNoise && impulseChance(rng) < 0.0008)
      value += impulseChance(rng) < 0.5 ? -0.8 : 0.8;
    return value;
  };

  auto appendSpace = [&](double units) {
    const double factor = std::clamp(spaceVariation(rng), 0.55, 1.65);
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
    const double factor = std::clamp(markVariation(rng), 0.55, 1.65);
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

std::string decode(const std::vector<float>& samples, double frequencyHz,
                   double hintWpm, std::vector<std::string>* logs = nullptr,
                   const std::vector<madmodem::cwskimmer::SelectedToneCwInterferer>& interferers = {},
                   double* finalWpm = nullptr) {
  madmodem::cwskimmer::SelectedToneCwConfig config;
  config.toneHz = frequencyHz;
  config.bandwidthHz = 120.0;
  config.minSnrDb = 0.0;
  config.initialWpm = hintWpm;
  config.autoWpm = true;
  config.autoBandwidth = true;
  madmodem::cwskimmer::SelectedToneCwTracker tracker(config);
  tracker.setInterferers(interferers);
  std::string output;
  tracker.setCallback([&](const auto& event) { output += event.committedText; });
  if (logs) tracker.setLogCallback([&](const std::string& line) { logs->push_back(line); });
  for (std::size_t position = 0; position < samples.size(); position += 1024U) {
    const std::size_t count = std::min<std::size_t>(1024U, samples.size() - position);
    tracker.processFloatMono(samples.data() + position, count, kSampleRate);
  }
  tracker.flush();
  if (finalWpm) *finalWpm = tracker.wpm();
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

void requireTrue(const std::string& name, bool value) {
  if (!value) throw std::runtime_error(name);
  std::cout << "PASS " << name << '\n';
}

void testRelativeTimingDirect() {
  using namespace madmodem::cwskimmer;
  CwRelativeTimingConfig cfg;
  cfg.initialWpm = 15.0;
  CwRelativeTimingDecoder decoder(cfg);
  const double dit = 54.0;
  double time = 0.0;
  std::string output;
  auto run = [&](bool mark, double ms) {
    CwLogicRun r;
    r.startSec = time;
    time += ms / 1000.0;
    r.endSec = time;
    r.mark = mark;
    r.durationMs = ms;
    r.confidence = 0.9;
    r.meanSnrDb = 12.0;
    r.coherence = 0.9;
    r.carrierCentered = true;
    output += decoder.processRun(r).committedText;
  };
  const std::string text = "CQ";
  for (std::size_t ci = 0; ci < text.size(); ++ci) {
    const std::string pattern = kMorse.at(text[ci]);
    for (std::size_t ei = 0; ei < pattern.size(); ++ei) {
      const double mark = (pattern[ei] == '.' ? dit : 2.45 * dit) *
          (ei % 2 == 0 ? 0.94 : 1.08);
      run(true, mark);
      if (ei + 1U < pattern.size()) run(false, dit * 1.05);
    }
    run(false, ci + 1U < text.size() ? 3.2 * dit : 7.2 * dit);
  }
  output += decoder.flush(time).committedText;
  requireEqual("relative-pair-human-ratio", normalize(output), text);
  const auto state = decoder.snapshot();
  requireTrue("relative-pair-lock", state.state == CwRelativeTimingState::Track ||
                                    state.state == CwRelativeTimingState::PairLock);
}

void testSameLaneAbruptOperatorSpeedChange() {
  using namespace madmodem::cwskimmer;
  CwRelativeTimingConfig cfg;
  cfg.initialWpm = 20.0;
  cfg.autoWpm = true;
  CwRelativeTimingDecoder decoder(cfg);
  double time = 0.0;
  std::string output;

  const auto feed = [&](bool mark, double durationMs) {
    CwLogicRun run;
    run.startSec = time;
    time += durationMs / 1000.0;
    run.endSec = time;
    run.mark = mark;
    run.durationMs = durationMs;
    run.confidence = 0.96;
    run.meanSnrDb = 28.0;
    run.coherence = 0.90;
    run.carrierCentered = mark;
    run.carrierCenteredFraction = mark ? 1.0 : 0.0;
    run.meanMarkProbability = mark ? 0.985 : 0.015;
    run.qsbProbability = 0.01;
    run.noiseProbability = 0.01;
    output += decoder.processRun(run).committedText;
  };

  const auto sendText = [&](const std::string& text, double ditMs,
                            double dashRatio) {
    for (std::size_t ci = 0; ci < text.size(); ++ci) {
      if (text[ci] == ' ') continue;
      const std::string& pattern = kMorse.at(text[ci]);
      for (std::size_t ei = 0; ei < pattern.size(); ++ei) {
        feed(true, pattern[ei] == '.' ? ditMs : dashRatio * ditMs);
        if (ei + 1U < pattern.size()) feed(false, ditMs);
      }
      if (ci + 1U < text.size())
        feed(false, text[ci + 1U] == ' ' ? 7.0 * ditMs : 3.0 * ditMs);
    }
  };

  // Operator A establishes a fast 30 WPM clock.  Operator B then answers on
  // exactly the same lane at about 14 WPM, without resetting the decoder.
  sendText("CQ", 54.0, 2.45);
  output += decoder.flush(time).committedText;
  feed(false, 520.0);
  sendText("QTH", 90.0, 2.45);
  output += decoder.flush(time).committedText;

  // flush() intentionally does not expose an artificial trailing blank, so
  // the direct decoder test validates the two payloads back-to-back.
  requireEqual("same-lane-abrupt-speed-change", normalize(output), "CQQTH");
  const auto state = decoder.snapshot();
  requireTrue("same-lane-new-temporal-epoch", state.temporalEpoch >= 1U);
  requireTrue("same-lane-slow-clock-installed",
              state.ditMs >= 75.0 && state.ditMs <= 105.0);
}

void testAdaptiveBeamReplaysWrongInitialClock() {
  using namespace madmodem::cwskimmer;
  CwMorseBeamDecoder beam;
  CwMorseTimingSnapshot timing;
  timing.ditMs = 80.0;       // deliberately wrong 15 WPM hint
  timing.dahMs = 240.0;
  timing.elementSpaceMs = 80.0;
  timing.characterSpaceMs = 240.0;
  timing.wordSpaceMs = 560.0;
  timing.timingConfidence = 0.10;

  CwMorseObservationQuality quality;
  quality.confidence = 0.95;
  quality.coherence = 0.90;
  quality.snrDb = 25.0;
  quality.carrierCentered = true;

  std::string output;
  std::vector<std::string> patterns;
  const auto absorb = [&](const CwMorseBeamResult& result) {
    output += result.committedText;
    patterns.insert(patterns.end(), result.committedPatterns.begin(),
                    result.committedPatterns.end());
  };

  // First element of C is a 132 ms dash. Under the wrong hint it lies between
  // the old dot and dash centres. It must remain reversible until the following
  // short/long pair establishes the actual 54/132 ms clock.
  absorb(beam.observeMark(132.0, timing, quality));
  absorb(beam.observeSpace(54.0, timing, quality));
  requireEqual("adaptive-beam-no-premature-first-mark", normalize(output), "");

  timing.ditMs = 54.0;
  timing.dahMs = 132.0;
  timing.elementSpaceMs = 54.0;
  timing.characterSpaceMs = 170.0;
  timing.wordSpaceMs = 390.0;
  timing.timingConfidence = 0.92;

  absorb(beam.observeMark(54.0, timing, quality));
  absorb(beam.observeSpace(54.0, timing, quality));
  absorb(beam.observeMark(132.0, timing, quality));
  absorb(beam.observeSpace(54.0, timing, quality));
  absorb(beam.observeMark(54.0, timing, quality));
  absorb(beam.observeSpace(170.0, timing, quality));
  absorb(beam.flush(timing));

  requireEqual("adaptive-beam-replay-first-dash", normalize(output), "C");
  const auto found = std::find(patterns.begin(), patterns.end(), "-.-.");
  requireTrue("adaptive-beam-pattern-exact", found != patterns.end());
}


void testBayesianPosteriorMetadata() {
  using namespace madmodem::cwskimmer;
  CwMorseBeamDecoder beam;
  CwMorseTimingSnapshot timing;
  timing.ditMs = 40.0;
  timing.dahMs = 122.0;
  timing.elementSpaceMs = 41.0;
  timing.characterSpaceMs = 124.0;
  timing.wordSpaceMs = 320.0;
  timing.timingConfidence = 0.90;

  CwMorseObservationQuality quality;
  quality.confidence = 0.93;
  quality.coherence = 0.88;
  quality.snrDb = 24.0;
  quality.carrierCentered = true;
  quality.stateProbability = 0.97;
  quality.qsbProbability = 0.02;
  quality.noiseProbability = 0.03;
  quality.centeredProbability = 0.95;

  std::string output;
  CwMorseBeamResult last;
  const auto absorb = [&](const CwMorseBeamResult& result) {
    output += result.committedText;
    last = result;
  };
  const std::string pattern = "-.-.";
  for (std::size_t index = 0; index < pattern.size(); ++index) {
    absorb(beam.observeMark(pattern[index] == '.' ? 40.0 : 122.0,
                            timing, quality));
    if (index + 1U < pattern.size())
      absorb(beam.observeSpace(41.0, timing, quality));
  }
  absorb(beam.observeSpace(126.0, timing, quality));
  absorb(beam.flush(timing));

  requireEqual("bayesian-posterior-text", normalize(output), "C");
  requireTrue("bayesian-posterior-range",
              last.bestPosterior >= 0.0 && last.bestPosterior <= 1.0 &&
              std::isfinite(last.posteriorOddsDb));
  requireTrue("bayesian-beam-bounded",
              last.hypothesisCount <= beam.config().beamWidth);
}

void testAudioCase(const std::string& name, const SignalOptions& options,
                   const std::string& expected, double hintWpm = 20.0) {
  std::vector<std::string> logs;
  const std::string actual = decode(synthesize(expected, options),
                                    options.frequencyHz, hintWpm, &logs);
  if (actual != expected) {
    std::cerr << "Last logs for " << name << ":\n";
    const std::size_t begin = logs.size() > 24U ? logs.size() - 24U : 0U;
    for (std::size_t i = begin; i < logs.size(); ++i) std::cerr << logs[i] << '\n';
  }
  requireEqual(name, actual, expected);
}


void appendBroadNarrowbandNoise(std::vector<float>& samples,
                                double centerHz,
                                double seconds,
                                unsigned seed) {
  std::mt19937 rng(seed);
  std::normal_distribution<double> white(0.0, 0.030);
  std::uniform_real_distribution<double> phase(0.0, 2.0 * kPi);
  const std::array<double, 8> offsets = {
      -92.0, -68.0, -46.0, -24.0, 24.0, 46.0, 68.0, 92.0};
  std::array<double, offsets.size()> phases{};
  for (double& value : phases) value = phase(rng);
  const std::size_t count = static_cast<std::size_t>(seconds * kSampleRate);
  samples.reserve(samples.size() + count);
  for (std::size_t i = 0; i < count; ++i) {
    const double time = static_cast<double>(samples.size()) / kSampleRate;
    double value = white(rng);
    for (std::size_t tone = 0; tone < offsets.size(); ++tone) {
      const double flutter = 0.55 + 0.45 * std::sin(
          2.0 * kPi * (1.7 + 0.31 * tone) * time + phases[tone]);
      value += 0.016 * flutter * std::sin(
          2.0 * kPi * (centerHz + offsets[tone]) * time + phases[tone]);
    }
    samples.push_back(static_cast<float>(value));
  }
}

void testTimingRejectsMicroRunStorm() {
  using namespace madmodem::cwskimmer;
  CwRelativeTimingConfig cfg;
  cfg.initialWpm = 20.0;
  CwRelativeTimingDecoder decoder(cfg);
  double time = 0.0;
  std::string output;
  auto feed = [&](bool mark, double durationMs, bool centered,
                  double confidence, double coherence) {
    CwLogicRun run;
    run.startSec = time;
    time += durationMs / 1000.0;
    run.endSec = time;
    run.mark = mark;
    run.durationMs = durationMs;
    run.confidence = confidence;
    run.meanSnrDb = 24.0;
    run.peakSnrDb = 30.0;
    run.coherence = coherence;
    run.carrierCentered = centered;
    run.carrierCenteredFraction = centered ? 1.0 : 0.02;
    output += decoder.processRun(run).committedText;
  };

  // Establish the real 20 WPM clock visible in the on-air log: about
  // 60/180 ms.  C and Q provide several trustworthy short/long pairs.
  const std::string text = "CQ";
  for (std::size_t ci = 0; ci < text.size(); ++ci) {
    const std::string pattern = kMorse.at(text[ci]);
    for (std::size_t ei = 0; ei < pattern.size(); ++ei) {
      feed(true, pattern[ei] == '.' ? 60.0 : 180.0, true, 0.95, 0.78);
      if (ei + 1U < pattern.size()) feed(false, 60.0, false, 0.90, 0.45);
    }
    feed(false, ci + 1U < text.size() ? 180.0 : 430.0,
         false, 0.90, 0.40);
  }
  output += decoder.flush(time).committedText;
  requireEqual("micro-run-storm-bootstrap", normalize(output), text);
  const auto before = decoder.snapshot();
  requireTrue("micro-run-storm-track", before.state == CwRelativeTimingState::Track);

  output.clear();
  const std::array<double, 20> marks = {
      16, 28, 41, 19, 12, 43, 8, 26, 35, 11,
      17, 22, 9, 33, 14, 21, 7, 38, 10, 24};
  const std::array<double, 20> spaces = {
      21, 50, 14, 68, 6, 40, 19, 37, 15, 56,
      11, 42, 8, 52, 16, 35, 9, 47, 13, 61};
  for (std::size_t i = 0; i < marks.size(); ++i) {
    // These quality values mirror the post-carrier fragments in the supplied
    // runtime log: locally high coherent/residual "SNR", but no centred lane.
    feed(true, marks[i], false, 0.82 + 0.04 * (i % 3),
         0.18 + 0.12 * (i % 4));
    feed(false, spaces[i], false, 0.80, 0.30);
  }
  output += decoder.flush(time).committedText;
  requireEqual("micro-run-storm-no-text", normalize(output), "");
  const auto after = decoder.snapshot();
  requireTrue("micro-run-storm-clock-retained",
              after.ditMs >= 48.0 && after.ditMs <= 72.0 &&
              after.wpm >= 16.0 && after.wpm <= 25.0);
}

void testWpmCeilingAndDuplicateToneUpdate() {
  using namespace madmodem::cwskimmer;
  SelectedToneCwConfig cfg;
  cfg.toneHz = 1000.0;
  cfg.initialWpm = 70.0;  // stale setting from the failing live run
  cfg.autoWpm = true;
  SelectedToneCwTracker tracker(cfg);
  requireTrue("auto-wpm-ceiling-50", tracker.wpm() <= 50.0 + 1.0e-6);

  std::vector<std::string> logs;
  tracker.setLogCallback([&](const std::string& line) { logs.push_back(line); });
  tracker.setToneHz(1000.0);
  tracker.setToneHz(1000.2);
  tracker.setToneHz(999.8);
  requireTrue("duplicate-tone-no-reset", logs.empty());
  tracker.setToneHz(1001.0);
  const int resetCount = static_cast<int>(std::count_if(
      logs.begin(), logs.end(), [](const std::string& line) {
        return line.find("clean restart") != std::string::npos;
      }));
  requireTrue("real-tone-change-one-reset", resetCount == 1);
}

void testDiscriminatorResetUsesLiveTimestamp() {
  using namespace madmodem::cwskimmer;
  CwCarrierDiscriminatorConfig cfg;
  cfg.minSnrDb = 0.0;
  cfg.minimumStableMs = 4;
  CwCarrierDiscriminator discriminator(cfg);
  discriminator.reset();

  std::optional<CwLogicRun> completed;
  double timestamp = 10.0;  // reset in the middle of an already-running stream
  for (int i = 0; i < 80 && !completed.has_value(); ++i) {
    timestamp += 0.001;
    CwCarrierObservation observation;
    observation.timestampSec = timestamp;
    observation.envelope = i < 30 ? 0.01 : 1.0;
    observation.acquisitionEnvelope = observation.envelope;
    observation.snrDb = i < 30 ? -3.0 : 30.0;
    observation.coherence = i < 30 ? 0.05 : 0.95;
    observation.carrierCentered = i >= 30;
    const auto result = discriminator.process(observation);
    if (result.completedRun.has_value()) completed = result.completedRun;
  }

  requireTrue("mid-stream-reset-produces-run", completed.has_value());
  requireTrue("mid-stream-reset-live-start",
              completed->startSec >= 10.0 && completed->durationMs < 250.0);
  requireTrue("discriminator-posterior-export",
              completed->meanMarkProbability >= 0.001 &&
              completed->meanMarkProbability <= 0.999 &&
              completed->qsbProbability >= 0.0 &&
              completed->qsbProbability <= 1.0 &&
              completed->noiseProbability >= 0.0 &&
              completed->noiseProbability <= 1.0);
}

void testFirstSampleRateDoesNotRestartAgain() {
  using namespace madmodem::cwskimmer;
  SelectedToneCwConfig cfg;
  cfg.toneHz = 1000.0;
  SelectedToneCwTracker tracker(cfg);
  std::vector<std::string> logs;
  tracker.setLogCallback([&](const std::string& line) { logs.push_back(line); });
  std::array<float, 1024> silence{};
  tracker.processFloatMono(silence.data(), silence.size(), 48000.0);
  const auto restartCount = [&]() {
    return static_cast<int>(std::count_if(
        logs.begin(), logs.end(), [](const std::string& line) {
          return line.find("clean restart") != std::string::npos;
        }));
  };
  requireTrue("first-sample-rate-no-extra-restart", restartCount() == 0);
  tracker.processFloatMono(silence.data(), silence.size(), 44100.0);
  requireTrue("real-sample-rate-change-restarts", restartCount() == 1);
}

void testCommittedPatternSnapshot() {
  using namespace madmodem::cwskimmer;
  CwRelativeTimingDecoder decoder;
  double time = 0.0;
  CwRelativeTimingResult committed;
  auto feed = [&](bool mark, double durationMs) {
    CwLogicRun run;
    run.startSec = time;
    time += durationMs / 1000.0;
    run.endSec = time;
    run.mark = mark;
    run.durationMs = durationMs;
    run.confidence = 0.95;
    run.meanSnrDb = 25.0;
    run.coherence = 0.85;
    run.carrierCentered = mark;
    run.carrierCenteredFraction = mark ? 1.0 : 0.0;
    const auto result = decoder.processRun(run);
    if (!result.committedText.empty()) committed = result;
  };

  const std::string pattern = "-.-.";  // C
  for (std::size_t i = 0; i < pattern.size(); ++i) {
    feed(true, pattern[i] == '.' ? 50.0 : 150.0);
    if (i + 1U < pattern.size()) feed(false, 50.0);
  }
  feed(false, 170.0);
  if (committed.committedText.empty()) committed = decoder.flush(time);
  requireTrue("commit-pattern-character", committed.committedText == "C");
  requireTrue("commit-pattern-exact",
              committed.committedPatterns.size() == 1U &&
              committed.committedPatterns.front() == pattern);
}

void testMessageThenBroadNoiseTail() {
  SignalOptions options;
  options.frequencyHz = 1309.0;
  options.wpm = 20.0;
  options.amplitude = 0.35;
  options.noiseAmplitude = 0.015;
  const std::string expected = "CQ CQ DE TEST";
  std::vector<float> samples = synthesize(expected, options);

  // Keep only about seven dit units after the last MARK, then replace the rest
  // with broad, beating narrow-band noise. This reproduces the live transition
  // from a valid 20 WPM lane into the run storm that previously emitted pages
  // of E/T garbage and re-locked at 70 WPM.
  const std::size_t trim = static_cast<std::size_t>(
      std::lround(9.0 * (1.2 / options.wpm) * kSampleRate));
  if (samples.size() > trim) samples.resize(samples.size() - trim);
  appendBroadNarrowbandNoise(samples, options.frequencyHz, 6.0, 0x70badU);

  std::vector<std::string> logs;
  double finalWpm = 0.0;
  const std::string actual = decode(samples, options.frequencyHz, 20.0,
                                    &logs, {}, &finalWpm);
  if (actual != expected) {
    const std::size_t begin = logs.size() > 50U ? logs.size() - 50U : 0U;
    for (std::size_t i = begin; i < logs.size(); ++i) std::cerr << logs[i] << '\n';
  }
  requireEqual("message-then-broad-noise-tail", actual, expected);
  const bool stopped = std::any_of(logs.begin(), logs.end(),
      [](const std::string& line) {
        return line.find("carrier lost: timing frozen") != std::string::npos;
      });
  requireTrue("noise-tail-carrier-gate-closes", stopped);
  requireTrue("noise-tail-wpm-frozen",
              finalWpm >= 16.0 && finalWpm <= 25.0);
}

void testLiveSameLaneTwoOperators() {
  SignalOptions fast;
  fast.frequencyHz = 1437.0;
  fast.wpm = 30.0;
  fast.amplitude = 0.35;
  fast.noiseAmplitude = 0.010;
  fast.seed = 0xA30U;

  SignalOptions slow = fast;
  slow.wpm = 14.0;
  slow.amplitude = 0.31;
  slow.noiseAmplitude = 0.014;
  slow.seed = 0xB14U;

  std::vector<float> samples = synthesize("CQ CQ", fast);
  const std::vector<float> reply = synthesize("QTH ROMA", slow);
  samples.insert(samples.end(), reply.begin(), reply.end());

  std::vector<std::string> logs;
  const std::string actual = decode(samples, fast.frequencyHz, 20.0, &logs);
  if (actual != "CQ CQ QTH ROMA") {
    const std::size_t begin = logs.size() > 80U ? logs.size() - 80U : 0U;
    for (std::size_t i = begin; i < logs.size(); ++i) std::cerr << logs[i] << '\n';
  }
  requireEqual("live-same-lane-two-operators", actual, "CQ CQ QTH ROMA");
  requireTrue("live-same-lane-epoch-log",
      std::any_of(logs.begin(), logs.end(), [](const std::string& line) {
        return line.find("temporal epoch") != std::string::npos;
      }));
}

void testNoiseOnly() {
  SignalOptions options;
  options.frequencyHz = 1293.0;
  options.noiseAmplitude = 0.09;
  options.impulsiveNoise = true;
  std::vector<float> samples(static_cast<std::size_t>(8 * kSampleRate), 0.0f);
  std::mt19937 rng(1234U);
  std::normal_distribution<float> noise(0.0f, 0.09f);
  std::uniform_real_distribution<float> chance(0.0f, 1.0f);
  for (float& value : samples) {
    value = noise(rng);
    if (chance(rng) < 0.0005f) value += chance(rng) < 0.5f ? -0.9f : 0.9f;
  }
  requireEqual("noise-only", decode(samples, options.frequencyHz, 20.0), "");
}

} // namespace

int main() {
  try {
    testRelativeTimingDirect();
    testSameLaneAbruptOperatorSpeedChange();
    testAdaptiveBeamReplaysWrongInitialClock();
    testBayesianPosteriorMetadata();
    testTimingRejectsMicroRunStorm();
    testWpmCeilingAndDuplicateToneUpdate();
    testDiscriminatorResetUsesLiveTimestamp();
    testFirstSampleRateDoesNotRestartAgain();
    testCommittedPatternSnapshot();

    SignalOptions clean;
    clean.frequencyHz = 1329.0;
    clean.wpm = 20.0;
    testAudioCase("clean-20", clean, "CQ CQ DE IZ6NNH 599", 20.0);

    SignalOptions fast = clean;
    fast.frequencyHz = 1453.0;
    fast.wpm = 35.0;
    testAudioCase("clean-35-wrong-hint", fast, "CQ CQ DE TEST", 18.0);

    SignalOptions liveLoop = clean;
    liveLoop.frequencyHz = 1548.0;
    liveLoop.wpm = 30.5;
    liveLoop.wordSpaceUnits = 8.8;
    liveLoop.noiseAmplitude = 0.012;
    testAudioCase("live-30-long-word-gap", liveLoop,
                  "CQ CQ OG50YL OG50YL CQ CQ OG50YL", 20.0);

    SignalOptions fastFarnsworth = clean;
    fastFarnsworth.frequencyHz = 1607.0;
    fastFarnsworth.wpm = 38.0;
    fastFarnsworth.wordSpaceUnits = 11.0;
    fastFarnsworth.noiseAmplitude = 0.018;
    testAudioCase("fast-farnsworth-11-dit", fastFarnsworth,
                  "CQ CQ DE TEST", 18.0);

    SignalOptions slow = clean;
    slow.frequencyHz = 1187.0;
    slow.wpm = 12.0;
    slow.noiseAmplitude = 0.018;
    testAudioCase("clean-12-wrong-hint", slow,
                  "CQ DE IZ6NNH", 28.0);

    SignalOptions noisy = clean;
    noisy.frequencyHz = 1514.0;
    noisy.noiseAmplitude = 0.055;
    testAudioCase("awgn", noisy, "CQ CQ DE IZ6NNH", 20.0);

    SignalOptions qsb = noisy;
    qsb.frequencyHz = 1293.0;
    qsb.qsb = true;
    qsb.noiseAmplitude = 0.035;
    testAudioCase("qsb", qsb, "CQ CQ DE TEST", 20.0);

    SignalOptions human = clean;
    human.frequencyHz = 1531.0;
    human.wpm = 23.0;
    human.markJitter = 0.16;
    human.spaceJitter = 0.18;
    human.dashRatio = 2.45;
    human.noiseAmplitude = 0.025;
    testAudioCase("human-relative", human, "CQ CQ DE IZ6NNH", 18.0);

    SignalOptions adjacent = noisy;
    adjacent.frequencyHz = 1388.0;
    adjacent.interfererOffsetHz = 70.0;
    adjacent.interfererAmplitude = 0.55;
    std::vector<madmodem::cwskimmer::SelectedToneCwInterferer> neighbours = {
        {adjacent.frequencyHz + adjacent.interfererOffsetHz, 1.0}};
    const std::string adjacentText = "CQ CQ DE IZ6NNH";
    std::vector<std::string> adjacentLogs;
    const std::string adjacentActual = decode(
        synthesize(adjacentText, adjacent), adjacent.frequencyHz,
        20.0, &adjacentLogs, neighbours);
    if (adjacentActual != adjacentText) {
      for (const auto& line : adjacentLogs) std::cerr << line << '\n';
    }
    requireEqual("adjacent-carrier-plus-70", adjacentActual, adjacentText);

    testMessageThenBroadNoiseTail();
    testLiveSameLaneTwoOperators();
    testNoiseOnly();
    std::cout << "All carrier-gated relative-timing CW regressions passed.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "CW regression aborted: " << error.what() << '\n';
    return 1;
  }
}
