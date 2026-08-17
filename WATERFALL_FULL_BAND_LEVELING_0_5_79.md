# Waterfall full-band leveling — MadModem 0.5.79

## Root cause

The remaining "accordion" effect was not a scroll-timing defect. `WaterfallLeveler`
contained a dynamic receiver/passband detector driven by the current row's 90th
percentile (`highReference`) and a relative threshold 24 dB below it. On radios
whose V/U/SHF audio response has a much lower noise floor at the outer part of
the selected spectrum, a strong carrier could raise the reference enough that
quiet but valid edge bins were reclassified as being outside the passband.

The old code then set the baseline of every bin outside the detected band equal
to that bin's current dB value. After subtraction this produced exactly 0 dB,
which maps to the palette's black entry. As the detected begin/end positions
moved, the visible coloured waterfall therefore appeared to narrow and widen.

## 0.5.79 invariant

Waterfall display geometry must never depend on instantaneous signal amplitude.
The complete selected spectrum is always included in the per-row lower-envelope
fit. There is no dynamic partial-band masking and no outside-band `baseline =
dbLine` hard-black operation.

Steep or quiet receiver edges are still handled by the existing robust local
lower-envelope and one-sided edge-anchor corrections. They may be darker than
the centre when the receiver really has less noise there, but they remain part
of the same continuous colour scale and cannot disappear merely because a
strong signal arrives.

This is display-only. Decoder audio and modem DSP are unchanged.

## Regression case

`tests/WaterfallLevelerRegression.cpp` includes a TS-790-style case with a
24 dB quieter outer noise floor and a strong central carrier. The regression
requires the full 0..767 bin range both before and after the carrier, non-black
edge residuals, stable edge medians, and preserved carrier prominence.
