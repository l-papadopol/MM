# CW Skimmer API

## Header

```cpp
#include "cwskimmer/CwSkimmerEngine.h"
```

## Basic use

```cpp
using namespace madmodem::cwskimmer;

CwSkimmerConfig cfg;
cfg.thresholdMultiplier = 9.0f;
cfg.minEventSnrDb = 12.0f;

CwSkimmerEngine engine(cfg);
engine.setCallback([](const CwSkimmerEvent& ev) {
    // ev.channelIndex
    // ev.audioFrequencyHz
    // ev.snrDb
    // ev.wpm
    // ev.confidence
    // ev.committedText
    // ev.partialText
    // ev.rollingText
});

engine.processFloatMono(samples, count, sampleRate);
```

## Input audio

Supported input calls:

```cpp
void processFloatMono(const float* samples, std::size_t count, double sourceSampleRate);
void processPcm16Mono(const int16_t* samples, std::size_t count, double sourceSampleRate);
```

`processFloatMono()` expects normalized mono samples in the range `[-1.0, +1.0]`.

The source sample rate may be 44.1 kHz, 48 kHz, 96 kHz, or another positive value. The wrapper resamples to the 15 kHz internal rate expected by the assimilated core.

## Events

`CwSkimmerEvent` is emitted when the core produces committed or partial text.

Important fields:

```cpp
struct CwSkimmerEvent {
  double timestampSec;
  int channelIndex;
  double audioFrequencyHz;
  float snrDb;
  float confidence;
  float wpm;
  std::string committedText;
  std::string partialText;
  std::string rollingText;
};
```

`committedText` is the newly committed text fragment. `rollingText` is the trimmed cumulative text for that channel and is the best field for waterfall OSD rendering.

## Channel state snapshots

```cpp
auto states = engine.channelStates();
auto top2 = engine.priorityChannels(2);
```

`channelStates()` returns all skimmer channel states.

`priorityChannels(2)` remains available for diagnostics and future auto-skimmer UI modes. In the current MadModem UI, the main RX textbox is driven by the operator-selected RX A/RX B markers; automatic ranking is not allowed to move the user's active decoder lanes.

## Waterfall magnitudes

```cpp
auto mags = engine.lastMagnitudes();
```

Returns the latest 32 FFT magnitudes from the assimilated core. This is optional for MadModem because the existing waterfall already has its own spectrum data, but it is useful for diagnostics.
