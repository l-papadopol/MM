# CW adaptive decoder notes for future implementation

This note records the CW decoder material supplied by the user and the intended direction for MadModem.

## Useful ideas from the FFT article

- Process short audio frames every 4 ms.
- Use a short Blackman-windowed FFT to find the dominant CW tone bin.
- Detect tone on/off transitions from adaptive magnitude/noise thresholds.
- Maintain a rolling history of magnitudes/transitions, approximately 16 seconds.
- Re-estimate dit/dah/space timing continuously so WPM changes can be handled.

## Useful ideas from the STM32 Goertzel sketch

- A narrow tone detector can be built from center/low/high Goertzel bins.
- The low/center/high magnitudes are useful for a tuning indicator.
- Squelch/noise tracking must adapt to receiver AGC and QSB.
- Decode timing is based on key-down/key-up transitions, with adaptive `avgDit`, `avgDah`, `avgDeadSpace`, letter break and word break.
- Special modes for bug key timing can be useful, but should remain optional in MM.

## MM implementation direction

Do not block the existing audio pipeline. Implement CW as an isolated worker-style decoder:

```
audio block -> CW tone tracker -> key up/down events -> adaptive timing -> text output
```

Preferred plan:

- Keep the existing CW decoder as fallback.
- Add a selectable `Adaptive FFT/Goertzel CW` decoder.
- Use FFT or a small Goertzel bank depending on CPU/quality tests.
- Show estimated tone, WPM, key state and squelch state.
- Use a rolling history so already-received symbols can be reclassified when WPM/timing improves.
- Do not touch FT, CAT, ADIF or map code when implementing this feature.
