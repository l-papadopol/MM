# FT WAV analysis — v2.24

This version adds an offline test path for FT8/FT4 recordings.

The purpose is regression testing and field-debugging: record an FT8/FT4 audio file, load it into MM, and compare decoded rows against MSHV/WSJT-X without routing the file through speakers or disturbing PTT/CAT.

## UI

FT4/FT8 Mode tab:

- `Analyze FT WAV...`

The file dialog accepts WAV/WAVE files. The analysis uses the currently selected FT mode and the current Deep decode / triple pass setting.

## Architecture

The path is deliberately isolated:

```text
WAV file
  -> Ft8RxDecoder worker thread
  -> offline WAV reader
  -> mono conversion
  -> 12 kHz decoder resampler
  -> slot splitter
  -> same FT decodeSlot() pipeline
  -> normal decodeReady/performanceUpdated signals
  -> decode table / log
```

It does **not** touch:

- PTT
- TX scheduler
- FtTxPlan
- FT TX worker
- CAT frequency control
- live audio input

This preserves the divide-et-impera architecture already used by MM: live audio, decoder worker, sequencer, UTC scheduler, TX worker and UI remain separate.

## Supported WAV formats

- PCM 8-bit unsigned
- PCM 16-bit signed
- PCM 24-bit signed
- PCM 32-bit signed
- IEEE float 32-bit
- mono or multichannel, mixed to mono

Compressed formats such as MP3/OGG/FLAC are not decoded in this first pass. Use WAV exports for repeatable FT tests.
