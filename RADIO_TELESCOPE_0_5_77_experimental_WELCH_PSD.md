# MadModem 0.5.77.experimental — Radio Telescope Welch PSD noise estimator

This patch replaces the first Radio Telescope band-noise prototype based on a
Goertzel-bank average with a robust Welch PSD estimator.

## Measurement pipeline

For each audio block received while a sky tile is in its dwell interval:

1. remove DC bias from the block;
2. split the block into power-of-two segments, capped at 4096 samples;
3. apply a Hann window to each segment;
4. run a radix-2 FFT;
5. compute one-sided PSD bins;
6. integrate only the configured audio slice, for example 1000-2000 Hz;
7. use a trimmed mean across frequency bins to suppress narrow carriers/spurs;
8. use a trimmed mean across Welch segments;
9. average block-level powers for the full dwell time;
10. store the final tile value as dBFS band power.

The value is still relative to digital audio full scale, not calibrated RF dBm.
The intent is stable site-survey/RFI and amateur radio astronomy heatmapping.

## Why this is better

The old Goertzel-bank path was useful as a quick prototype, but a single strong
carrier inside the selected slice could dominate the tile colour.  Welch PSD
with robust trimming follows broadband noise better and makes the final
logarithmic heatmap contrast more meaningful.

## Limits intentionally left explicit

- This is not yet an RF-calibrated radiometer.
- Radio/receiver AGC can still change absolute levels.
- Narrow-carrier rejection is robust, not total: very dense comb noise is still
  correctly treated as part of the RFI environment.
