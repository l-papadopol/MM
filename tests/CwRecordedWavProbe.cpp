#include "modems/cw/skimmer/SelectedToneCwTracker.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::uint16_t readU16(std::istream& input) {
  unsigned char bytes[2]{};
  input.read(reinterpret_cast<char*>(bytes), 2);
  if (!input) throw std::runtime_error("truncated WAV");
  return static_cast<std::uint16_t>(bytes[0]) |
         (static_cast<std::uint16_t>(bytes[1]) << 8U);
}

std::uint32_t readU32(std::istream& input) {
  unsigned char bytes[4]{};
  input.read(reinterpret_cast<char*>(bytes), 4);
  if (!input) throw std::runtime_error("truncated WAV");
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8U) |
         (static_cast<std::uint32_t>(bytes[2]) << 16U) |
         (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

struct WavData {
  double sampleRate = 0.0;
  std::vector<float> mono;
};

WavData readPcm16Wav(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("cannot open WAV: " + path);
  char id[4]{};
  input.read(id, 4);
  if (std::memcmp(id, "RIFF", 4) != 0) throw std::runtime_error("not RIFF");
  (void)readU32(input);
  input.read(id, 4);
  if (std::memcmp(id, "WAVE", 4) != 0) throw std::runtime_error("not WAVE");

  std::uint16_t format = 0;
  std::uint16_t channels = 0;
  std::uint16_t bits = 0;
  std::uint32_t sampleRate = 0;
  std::vector<unsigned char> payload;
  while (input && (payload.empty() || format == 0)) {
    input.read(id, 4);
    if (!input) break;
    const std::uint32_t size = readU32(input);
    if (std::memcmp(id, "fmt ", 4) == 0) {
      format = readU16(input);
      channels = readU16(input);
      sampleRate = readU32(input);
      (void)readU32(input);
      (void)readU16(input);
      bits = readU16(input);
      if (size > 16U) input.seekg(static_cast<std::streamoff>(size - 16U), std::ios::cur);
    } else if (std::memcmp(id, "data", 4) == 0) {
      payload.resize(size);
      input.read(reinterpret_cast<char*>(payload.data()),
                 static_cast<std::streamsize>(payload.size()));
    } else {
      input.seekg(static_cast<std::streamoff>(size), std::ios::cur);
    }
    if ((size & 1U) != 0U) input.seekg(1, std::ios::cur);
  }
  if (format != 1U || channels == 0U || bits != 16U || sampleRate == 0U ||
      payload.empty())
    throw std::runtime_error("probe accepts PCM 16-bit WAV only");

  const std::size_t frames = payload.size() / (2U * channels);
  WavData wav;
  wav.sampleRate = static_cast<double>(sampleRate);
  wav.mono.reserve(frames);
  for (std::size_t frame = 0; frame < frames; ++frame) {
    double sum = 0.0;
    for (std::size_t channel = 0; channel < channels; ++channel) {
      const std::size_t index = 2U * (frame * channels + channel);
      const std::uint16_t raw = static_cast<std::uint16_t>(payload[index]) |
          (static_cast<std::uint16_t>(payload[index + 1U]) << 8U);
      sum += static_cast<double>(static_cast<std::int16_t>(raw)) / 32768.0;
    }
    wav.mono.push_back(static_cast<float>(sum / channels));
  }
  return wav;
}

} // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 3 || argc > 5) {
      std::cerr << "usage: CwRecordedWavProbe WAV TONE_HZ [INITIAL_WPM] [INTERFERER_HZ]\n";
      return 2;
    }
    const WavData wav = readPcm16Wav(argv[1]);
    madmodem::cwskimmer::SelectedToneCwConfig config;
    config.toneHz = std::stod(argv[2]);
    config.bandwidthHz = 120.0;
    config.minSnrDb = 0.0;
    config.initialWpm = argc >= 4 ? std::stod(argv[3]) : 25.0;
    config.autoWpm = true;
    config.autoBandwidth = true;
    madmodem::cwskimmer::SelectedToneCwTracker tracker(config);
    if (argc >= 5) {
      tracker.setInterferers({{std::stod(argv[4]), 1.0}});
    }
    std::string decoded;
    double currentInputSec = 0.0;
    tracker.setCallback([&](const auto& event) {
      decoded += event.committedText;
      if (!event.committedText.empty()) {
        std::cout << "EVENT t=" << event.timestampSec
                  << " text=[" << event.committedText << "]\n";
      }
    });
    tracker.setLogCallback([&](const std::string& line) {
      std::cout << "LOG t~" << currentInputSec << ' ' << line << '\n';
    });
    tracker.setSpectrumCallback([](const auto& frame) {
      std::cout << "PSD t=" << frame.timestampSec
                << " prom=" << frame.carrierProminenceDb
                << " width=" << frame.carrierPeakWidthHz
                << " second=" << frame.carrierToSecondDb << '\n';
    });
    for (std::size_t offset = 0; offset < wav.mono.size(); offset += 1024U) {
      const std::size_t count = std::min<std::size_t>(1024U, wav.mono.size() - offset);
      currentInputSec = static_cast<double>(offset + count) / wav.sampleRate;
      tracker.processFloatMono(wav.mono.data() + offset, count, wav.sampleRate);
    }
    tracker.flush();
    std::cout << "DECODED=[" << decoded << "]\n";
    std::cout << "FINAL_WPM=" << tracker.wpm() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
