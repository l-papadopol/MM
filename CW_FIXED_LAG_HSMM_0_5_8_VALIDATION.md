# MadModem 0.5.8 R8 — CW fixed-lag segmental decoder

## Scope

R8 changes only the selected-tone CW receive path and its native regression
test. FT8/FT4, RTTY, audio capture, transmit and waterfall code are unchanged
from R7.

## Architecture

The former immediate hysteretic MARK/SPACE edge decision is replaced by one
causal, bounded fixed-lag explicit-duration beam:

- input: the existing one-millisecond soft carrier posterior, coherence, SNR,
  carrier-session probability and the current soft timing estimate;
- hypotheses: simultaneous MARK and SPACE segment histories;
- duration priors: broad log-normal 1/3-unit MARK families and 1/3/7-unit
  SPACE families, with a small heavy tail for manual sending and Farnsworth
  spacing;
- uncertainty: low timing confidence broadens the duration distributions; WPM
  remains a hint and is never a hard clock;
- QSB: a plausible in-element dip is treated as soft erasure evidence, so the
  continuous-MARK and real-SPACE explanations compete until later samples
  resolve them;
- decision: the oldest posterior state is committed after an adaptive 1.30-dit
  lag, clamped to 28–180 ms;
- resources: 24 bounded hypotheses, one temporal owner and no fallback or
  parallel legacy decoder.

The present-time best state is still exposed to AFC and diagnostics. Only the
fixed-lag resolved state advances the timing decoder, preserving causality.

R8 also bounds the learned word-gap decision threshold. A long receiver-idle
SPACE can no longer poison it and merge later words such as `CQ CQ` into
`CQCQ` or `DX DX` into `DXDX`.

## Supplied recording benchmark

File used only as an external test input (not included in the source archive):

- `MadModem_RX_20260818_072100_CW_Morse.wav`
- SHA-256: `2c245461aad48ce24efbb462f0c7b1aecf960d8ba13004efe8e3ddab0d1da21f`
- PCM16 mono, 48 kHz, 76.245333 s
- measured carrier peaks: approximately 788.0, 1784.5 and 2801.75 Hz

Central carrier at 1784.5 Hz, R7:

```text
K CQ DE F5IN ER5IN F5INK CQ CQ CQ DE F5IN F5IN F5IN PSE K RQ CQCQCQ DXDX BF5 T5IN F5IN PSE DX K
```

Central carrier at 1784.5 Hz, R8:

```text
K CQ DE F5IN F5IN F5INK CQ CQ CQ DE F5IN F5IN F5IN PSE K E E I E CQ CQ CQ CQ DX DX DE F5 F5IN F5IN PSE DX K
```

The recording has no authoritative transcript, so the uncertain faded/QRM
region is not counted as ground truth. The objective improvements visible in
the repeated traffic are the corrected `ER5IN` occurrence and restoration of
word boundaries in `CQ CQ` and `DX DX`. The remaining `E E I E` and incomplete
callsign in the deepest fade are retained as uncertainty rather than hidden by
a callsign or language-model guess.

On the deterministic weak coherent AWGN regression, edit similarity against
`CQ CQ DE IZ6NNH` changes from 0.733333 in R7 (`NQ CQ DE P6 NNH`) to
0.866667 in R8 (`RQ CQ DE IP6NNH`). This is a 13.33 percentage-point gain; it
is deliberately reported as a partial improvement, not human-level parity.

With probe logging redirected, the 76.245333-second central-carrier run took
0.807 seconds wall time in the validation container (about 94.5 times faster
than real time for one selected-tone tracker). This is a local measurement,
not a cross-platform performance guarantee.

## Regression coverage

`tests/CwNativeRegression.cpp` now additionally verifies that:

- fixed-lag resolution remains causal and within its configured bound;
- an eight-millisecond in-element fade remains one MARK;
- flush drains every unresolved tail sample;
- an already-running stream reset cannot create a multi-second synthetic run.

The full native suite also covers clean 12/20/30/35 WPM, wrong WPM hints,
Farnsworth spacing, AWGN, coherent weak-signal reception, QSB, an adjacent
carrier, carrier loss, noise-only input, speed changes and timing epochs.
