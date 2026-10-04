# CW carrier discriminator + relative timing clean restart — 0.5.78

## Scope

The previous CW Bayesian, geometric-rescue and causal semi-Markov decoders have
been removed from the source tree and from the CMake target. MadModem now ships
one live CW receive architecture with two independent responsibilities:

1. `CwCarrierDiscriminator` converts the selected narrow-band carrier into
   timestamped MARK/SPACE runs.
2. `CwRelativeTimingTask` owns a dedicated temporal worker thread and feeds
   those runs to `CwRelativeTimingDecoder`.

RX A and RX B each own their own discriminator, temporal task, timing model,
AFC state and output text.

## Signal flow

```text
48 kHz mono audio
  -> exact-tone complex mixer
  -> bounded multicarrier lane separator
  -> fourth-order narrow I/Q filter + edge-safe envelope
  -> adaptive carrier/noise model
  -> Schmitt/debounce carrier discriminator
  -> CwLogicRun queue (MARK/SPACE, timestamps, confidence, SNR, coherence)
  -> dedicated relative-timing task
  -> short/long MARK families + geometric-mean threshold
  -> independent element/character/word SPACE families
  -> Morse lookup and immediate character commit
```

The discriminator does not know dots, dashes, WPM or Morse characters. The
temporal decoder never receives audio, FFT bins or display pixels.

## Relative timing acquisition

The timing decoder does not require a fixed calibration window. It searches
continuously for neighbouring short/long MARK pairs. A pair is informative when:

- both MARKs are plausible and carrier-qualified;
- the long/short duration ratio is approximately 1.8 to 4.2;
- their separating SPACE is compatible with an intra-character gap.

The short and long families are maintained with bounded robust histories. The
classification threshold is:

```text
Tmark = sqrt(meanShort * meanLong)
```

SPACE durations are learned separately as element, character and word families.
The WPM field is derived from the current short-MARK family; the UI value is only
an acquisition prior.

This is an original C++ implementation. The relative-pair/geometric-mean idea
was inspired by the public K4ICY CW Decoder description and Arduino sketch; no
K4ICY source code is incorporated into MadModem.

## QSB repair

A deep fade can split one dash into:

```text
MARK fragment -> very short SPACE notch -> MARK fragment
```

The temporal decoder uses one-element look-ahead. It merges the triplet only
when the gap is much shorter than the learned element-space family, the two
fragments have compatible lengths/evidence, and the combined duration is
compatible with a long MARK. Normal one-dit gaps between two dots are preserved.

## Parallel task boundary

`CwRelativeTimingTask` owns the temporal decoder on a dedicated `std::thread`.
The DSP path submits timestamped runs and coalesced time-advance commands. A
mutex/condition-variable queue preserves event order. Results are accumulated
inside the task and drained by the tracker thread, so application/UI callbacks
are never executed from the temporal worker.

The task is linked through `Threads::Threads` and is covered by ThreadSanitizer.

## Live runtime log

The existing Runtime Log button is visible in CW mode. The log records, for RX A
and RX B:

- completed MARK/SPACE runs and durations;
- confidence, SNR, coherence and QSB tags;
- carrier lock and pre-roll replay;
- timing state (`SEARCH`, `PAIR LOCK`, `TRACK`, `REACQUIRE`);
- committed characters, dit/dah centres, geometric threshold and WPM.

The log buffer is retained while the dialog is closed and is limited to 5000
lines.

## Removed components

The following old decoder sources and their dedicated experiments/audits are no
longer included:

- `CwBayesianDecoder.*`
- `CwCausalSemiMarkovDecoder.*`
- `CwGeometricEdgeWorker.*`
- the obsolete CW algorithm-lab tree and old decoder-specific audit scripts.

The full-band carrier scanner, bounded lane separation, AFC protection, CW TX,
waterfall, audio engine and FT8/FT4 paths remain separate.
