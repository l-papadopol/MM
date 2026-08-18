# Waterfall WSJT-X-style flattening — MadModem 0.5.78

## Reason for the change

The previous display leveler estimated one temporal noise-floor reference and
moved it with a deliberately slow dB/second slew.  That protected the waterfall
from narrow carriers, but a fast receiver/sound-card AGC step could raise the
whole occupied passband much faster than the reference was allowed to follow.
The result was a broad orange/yellow field that remained hot for many seconds.
This affected only the display; decoder samples were unchanged.

## Source-derived WSJT-X behaviour

The implementation was derived directly from the supplied WSJT-X Improved
3.1.0 source tree, not from documentation or memory:

- `lib/flat4.f90`
  - converts each waterfall row from linear power to dB;
  - divides the row into ten frequency segments;
  - selects the lowest ten percent of points in every segment;
  - fits a fourth-order polynomial to those lower-envelope points;
  - subtracts the polynomial from the current row.
- `widgets/plotter.cpp`
  - applies the fixed waterfall transfer `y1 = 10 * gain * y + zero`;
  - uses `gain = fac * 10^(0.015 * PlotGain)`.
- `widgets/widegraph.cpp`
  - enables Flatten by default;
  - restores waterfall gain and zero independently of the spectral data.

WSJT-X therefore does not require a slow temporal full-band AGC to maintain the
waterfall background.  A common-mode level step is removed in the same row by
the lower-envelope fit.

## MadModem implementation

MadModem now follows the same display architecture:

1. FFT magnitudes are converted to dB as before.
2. The existing persistent passband detector excludes digitally silent monitor
   bins that WSJT-X normally does not encounter inside its selected range.
3. The valid row is split into ten segments.
4. The lowest ten-percent samples from each segment form the lower envelope.
5. A numerically conditioned fourth-order polynomial is fitted in normalized
   frequency coordinates.
6. The fitted baseline is subtracted from the current row immediately.
7. Colour intensity uses `10 * flattened_dB`; the existing Colour scale control
   applies the WSJT-X exponential gain law, with the saved 80% position as
   unity gain.

The old per-frequency temporal EMA and slow floor slew were removed.  They are
not needed after per-row flattening and could retain a receiver AGC transient.

## Deliberate differences from WSJT-X

- The circular OpenGL texture, smooth one-row presentation, HiDPI viewport,
  labels, markers and frequency zoom remain MadModem-native.
- The active MadModem palette is retained; this patch changes level behaviour,
  not palette selection.
- Bins outside a persistently detected sound-card passband map to black instead
  of entering the polynomial fit.
- A wide numerical clamp protects against a singular/pathological polynomial
  fit.  It is not a temporal AGC.

## Scope and safety

Only display code changed:

- `dsp/DspEngine.cpp`
- `dsp/DspEngine.h`
- `dsp/WaterfallLeveler.cpp`
- `dsp/WaterfallLeveler.h`
- `widgets/WaterfallWidget.cpp`
- `widgets/WaterfallWidget.h`
- waterfall regression tests and documentation

FT8, FT4, CW, audio capture, scheduler, sequencer, CAT/PTT and protocol code are
unchanged.
