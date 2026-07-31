# MadModem 0.5.78 release notes

## FT8 live efficiency and sequencer integrity

The 90% FT8 gate is now strictly a low-latency pass. It performs one candidate
pass with the normal sync/LDPC path, but leaves GF(2) OSD, heavy metric retries
and subtraction-driven deep passes to the complete boundary snapshot. A late RX
restart no longer launches a decode over a slot made mostly of UTC padding.
Candidate DT/DF clouds receive stronger local non-maximum suppression, reducing
duplicate LDPC attempts without merging genuinely separate FT8 signals.

Decoded rows are still displayed as soon as they are delivered, but the
sequencer consumes them atomically at the end of each decoder job. It evaluates
the full batch against one immutable QSO context and commits only the best state
transition. Auto-QSO CQ ranking is completed at the same point instead of adding
a 420 ms selection delay.

FT transmit timing now has a hard safety invariant: MadModem never removes the
start of an FT4/FT8 waveform to catch a missed slot. A late frame is cancelled,
PTT is released, RX is restored and the unchanged message is scheduled for the
next selected period.

This candidate requires the normal four-WAV benchmark and live Linux/Windows
legacy testing. The source-preparation environment did not contain Qt development
packages, so no claim of measured speed-up or unchanged 88-decode sensitivity is
made until those tests are run on the target build.

## Clean native CW receive

MadModem now contains exactly one CW receive implementation. It was written
natively in C++ for MadModem and replaces the former experimental CW source
chain completely. There is no legacy decoder, hidden fallback, external CW
adapter or inactive CW branch in the build.

RX A and RX B each own an independent exact-tone receiver. The DSP mixes the
selected carrier to complex baseband, applies explicit-Hz selectivity and
bounded AFC, estimates MARK and noise levels separately and emits soft
one-millisecond observations. A bounded Bayesian timing beam is the only layer
that assigns dot, dash and gap meanings.

The decoder follows these rules:

- measured MARK/SPACE durations are never rewritten or fused;
- SNR affects confidence, not duration or symbolic meaning;
- WPM is a weak acquisition hint and a derived 5–50 WPM display value;
- low-confidence QSB is retained as uncertainty instead of immediately becoming
  a certain Morse space;
- text is committed as soon as the posterior mass agrees on each completed
  character, without waiting for the end of a phrase;
- one stable public timing clock is maintained per lane and Auto-WPM movement is
  bounded between committed characters;
- noise-only input cannot train the speed model or publish text.

## Carrier discovery and operator display

The full-passband scanner discovers persistent carrier lanes with FFT sub-bin
interpolation and can keep two carriers 25 Hz apart as separate lanes in the
standalone regression. RX A/RX B remain the decoding receivers in this release.
The waterfall displays carrier labels and markers only; decoded characters are
shown continuously in the RX panes.

## RX WAV recorder

**File → Start RX audio recording…** (`Ctrl+Shift+R`) records the exact normalized
mono stream consumed by the waterfall and decoders. Files are mono 16-bit PCM
WAV at the active sample rate and are finalized safely on RX stop, error,
sample-rate change or shutdown.

## Waterfall and diagnostics

The passband-aware leveler keeps a persistent mask of the real audio passband,
rejects digitally silent regions and derives its floor from a slow low-quantile
history. Partial-band input, broad keyed energy and one strong carrier therefore
cannot pump the display orange. The zoom/pan bar is below the frequency labels.
CW diagnostics remain passive and do not feed GUI state back into the decoder.

## Validation

The pure-C++ native CW regression passes with GCC and Clang and checks:

- `CQ CQ DE IZ6NNH 599` at 20 WPM;
- the same message at 30 WPM with an intentionally wrong 18 WPM hint;
- short deep QSB notches plus noise;
- noise-only input with no published text;
- two discovered carrier lanes separated by 25 Hz;
- first completed character visible within one second of its on-air completion;
- bounded Auto-WPM steps while acquiring 27 WPM from a 20 WPM hint;
- correct decoding with deliberate phase discontinuities that drive instantaneous
  coherence close to zero, confirming coherence is a soft weight rather than a
  hard receive gate.

The waterfall-level regression passes five checks, including persistent-passband
behaviour under broad keyed energy. Full Qt linking and live-device tests must be
completed on a machine with Qt development packages. On-air recordings remain
the decisive CW acceptance test; no claim of perfect deep-QSB reconstruction is
made.
