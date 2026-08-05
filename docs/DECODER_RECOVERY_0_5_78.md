# Decoder policy — MadModem 0.5.78

## CW

MadModem builds one native CW receiver architecture only:

- `CwSkimmerEngine` discovers persistent carriers over the audio passband;
- RX A and RX B each use an independent `SelectedToneCwTracker`;
- `CwCarrierDiscriminator` produces timestamped MARK/SPACE runs;
- `CwRelativeTimingTask` runs the temporal model on a dedicated worker thread;
- `CwRelativeTimingDecoder` learns relative MARK and SPACE families and emits
  characters.

The temporal stage receives no audio and cannot alter measured run timestamps.
No Bayesian, geometric-rescue, semi-Markov fallback or external CW core is
present in the source tree or CMake graph.

## FT8/FT4

The continuous capture timeline, exact UTC slot split, immutable final snapshot,
early gate pass and mandatory complete-slot pass remain active. Decoder changes
must be measured against the same external WAV corpus; test audio is not bundled
in release archives.

## Isolation

CW work must not modify the shared `AudioEngine`, global RX start/stop, FT sample
timeline or unrelated modem paths unless a separately demonstrated shared bug
requires it.
