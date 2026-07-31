# Decoder policy — MadModem 0.5.78

## CW

MadModem builds one native CW receiver only:

- `CwSkimmerEngine` discovers persistent carriers over the audio passband;
- RX A and RX B each use an independent `SelectedToneCwTracker`;
- `CwBayesianDecoder` is the only component that assigns Morse timing meanings.

The front end emits soft MARK evidence. Raw measured intervals are never
resized or merged. SNR changes confidence only. WPM is a weak prior and a
derived UI result. Low-confidence QSB remains uncertain until the timing beam
can resolve it. The waterfall receives carrier labels, not decoded glyphs.

No other CW decoder, compatibility fallback or external CW core is present in
the source tree or CMake graph.

## FT8/FT4

The continuous capture timeline, exact UTC slot split, immutable final snapshot,
early gate pass and mandatory complete-slot pass remain active. Decoder changes
must be measured against the same external WAV corpus; test audio is not bundled
in release archives.

## Isolation

CW work must not modify the shared `AudioEngine`, global RX start/stop, FT sample
timeline or unrelated modem paths unless a separately demonstrated shared bug
requires it.
