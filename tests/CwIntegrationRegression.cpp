#include "CwTestSignal.h"
#include "modems/cw/CwDecoder.h"
#include <QCoreApplication>

using namespace cwtest;

namespace {
void require(bool condition, const std::string& name) {
  if (!condition) throw std::runtime_error(name);
  std::cout << "PASS " << name << '\n';
}

void receive(const std::string& text, double offset = 0.0, bool adjacent = false,
             bool manual = false, int rate = kSampleRate) {
  SignalOptions signal;
  signal.frequencyHz = 700.0;
  signal.noiseAmplitude = 0.005;
  signal.portableRandom = true;
  signal.riseTimeMs = 5.0;
  if (adjacent) {
    signal.interfererOffsetHz = 70.0;
    signal.interfererAmplitude = 0.55;
  }
  auto audio = synthesize(text, signal);
  if (rate != kSampleRate) {
    std::vector<float> converted(static_cast<std::size_t>(audio.size() * double(rate) / kSampleRate));
    for (std::size_t i = 0; i < converted.size(); ++i) {
      const double position = i * double(kSampleRate) / rate;
      const auto left = static_cast<std::size_t>(position);
      const double fraction = position - left;
      converted[i] = static_cast<float>(audio[left] * (1.0 - fraction) +
          audio[std::min(left + 1U, audio.size() - 1U)] * fraction);
    }
    audio = std::move(converted);
  }
  CwDecoder decoder;
  decoder.setToneHz(700.0 - offset);
  decoder.setAfcEnabled(true);
  decoder.setAutoWpm(!manual);
  std::string output;
  QObject::connect(&decoder, &CwDecoder::priorityTextReceived,
      [&](int rank, const QString& value) { if (rank == 0) output += value.toStdString(); });
  for (std::size_t p = 0; p < audio.size(); p += 1024U) {
    AudioBlock block;
    block.sampleRate = rate;
    block.samples = QVector<float>(audio.data() + p,
        audio.data() + std::min(p + 1024U, audio.size()));
    decoder.processAudioBlock(block);
  }
  const std::string name = text + " offset=" + std::to_string(offset) +
      " adjacent=" + std::to_string(adjacent) + " manual=" + std::to_string(manual) +
      " rate=" + std::to_string(rate);
  if (normalize(output) != text) std::cerr << "Received: " << normalize(output) << '\n';
  require(normalize(output) == text, name);
  if (offset != 0.0) require(std::abs(decoder.trackedToneHz(0) - 700.0) < 3.0, "AFC converges");
}

void filterAndManualClock() {
  using namespace madmodem::cwskimmer;
  CwRelativeTimingConfig timingConfig;
  timingConfig.initialWpm = 20.0;
  timingConfig.autoWpm = false;
  CwRelativeTimingDecoder timing(timingConfig);
  // Acquisition at a different speed must not silently replace manual timing.
  timing.beginEpoch(34.0, 102.0, 34.0, true);
  require(std::abs(timing.snapshot().ditMs - 60.0) < 0.001 &&
      std::abs(timing.snapshot().dahMs - 180.0) < 0.001, "manual timing survives acquisition");

  auto envelopePower = [](double bandwidth) {
    SelectedToneCwConfig config;
    config.toneHz = 700.0;
    config.bandwidthHz = bandwidth;
    config.autoBandwidth = false;
    config.afcEnabled = false;
    SelectedToneCwTracker tracker(config);
    std::size_t accepted = 0U;
    double energy = 0.0;
    tracker.setDiagnosticCallback([&](const auto& state) {
      if (state.timestampSec > 1.0) { energy += state.filteredEnvelope * state.filteredEnvelope; ++accepted; }
    });
    std::vector<float> audio(3U * kSampleRate);
    for (std::size_t i = 0; i < audio.size(); ++i)
      audio[i] = static_cast<float>(0.2 * std::sin(2.0 * kPi * 780.0 * i / kSampleRate));
    for (std::size_t p = 0; p < audio.size(); p += 1024U)
      tracker.processFloatMono(audio.data() + p, std::min<std::size_t>(1024U, audio.size() - p), kSampleRate);
    return accepted ? energy / accepted : -1.0;
  };
  const double narrow = envelopePower(60.0), wide = envelopePower(240.0);
  require(narrow >= 0.0 && wide > 0.0 && narrow < wide * 0.1,
      "selected bandwidth attenuates off-channel discriminator input");
}
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  try {
    for (const auto* message : {"TEST", "SOS", "599", "IZ6NNH"}) receive(message);
    for (double offset : {-20.0, -15.0, -10.0, 0.0, 10.0, 15.0, 20.0})
      receive("CQ CQ DE IZ6NNH 599", offset);
    receive("CQ CQ DE IZ6NNH 599", 0.0, true);
    receive("TEST", 0.0, false, true);
    for (int rate : {44100, 96000}) receive("CQ CQ DE IZ6NNH 599", 0.0, false, false, rate);
    filterAndManualClock();
  } catch (const std::exception& error) {
    std::cerr << "CW integration failed: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
