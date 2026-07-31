# Native CW receiver architecture

MadModem contains one CW receive implementation. It is clean-room C++ and has no
legacy decoder, hidden fallback, adapter, or copied external CW core.

## Signal flow

1. `CwSkimmerEngine` performs full-passband FFT carrier discovery with sub-bin
   interpolation and persistent lane association.
2. RX A and RX B each own an independent `SelectedToneCwTracker`.
3. A tracker mixes its selected carrier to complex baseband and applies a
   fourth-order low-pass response whose width is expressed directly in hertz.
4. Separate MARK and noise models produce a one-millisecond soft MARK
   probability. Long-term phase coherence is used to prevent filtered noise from
   opening a decode session.
5. `CwBayesianDecoder` confirms only sustained state changes, measures raw
   MARK/SPACE intervals, and evaluates dot/dash plus element/character/word-gap
   interpretations in a bounded Bayesian beam.
6. Text is committed only when competitive hypotheses agree. The waterfall
   receives carrier lanes, not isolated decoded glyphs.

## Timing and WPM

The user WPM value is an acquisition hint. The decoder maintains a latent Morse
unit inferred from accepted interval geometry. Display WPM is derived from that
unit and clamped to 5–50 WPM. Low-confidence observations and noise cannot train
it.

## QSB

Low-confidence amplitude collapses are softened toward an erasure probability;
they are not automatically converted into certain Morse spaces. Measured
intervals are never resized or fused before Bayesian interpretation.

## Isolation

The CW target is pure C++. It does not alter `AudioEngine`, the FT8/FT4 sample
timeline, global RX start/stop, or other modem paths.
