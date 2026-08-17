# CW carrier/timing fix for MadModem 0.5.79

## Evidence from the supplied recording

The selected RX A carrier is at 1790.04 Hz.  Its three strong calls are keyed
at about 17 WPM:

- dit: 67-80 ms;
- dah: 204-214 ms;
- intra-element gap: 61-72 ms;
- word gap: about 475-480 ms.

The first two calls are exact `-.-. --.-` sequences (`CQ`); the third contains
short detector perturbations but the same underlying Morse sequence.

The failing runtime log showed that these runs reached the temporal decoder,
but a provisional 24/40 or 29/68 ms clock was retained while the real
68-75/207-210 ms geometry arrived.  Sub-15 ms false MARKs also replaced the
previous real MARK, preventing the genuine short/long pairs from becoming
adjacent.  Completed seven-dit spaces could additionally be classified as
character gaps without a forced word-boundary commit.

## Changes

- A contradictory clock must be confirmed by consecutive coherent pairs when
  the old geometry is malformed; a normal common-scale operator change keeps
  the existing fast path.
- Replacing a provisional clock is atomic: the old uncommitted beam history is
  discarded and only runs compatible with the new epoch are replayed.
- MARKs below 15 ms are bridged into the surrounding SPACE and cannot replace
  the previous timing endpoint.
- Weak marginal fragments near the bottom of the dit family are rejected,
  while strong coherent marks remain eligible for a genuine speed change.
- A completed SPACE crossing the adaptive 7-dit boundary now forces the same
  word commit as the live open-gap timer.

The CW path remains single-owner and single-publisher; no fallback decoder was
added.

## Regression coverage

Run the complete native CW suite:

```sh
bash scripts/run_cw_native_regression.sh
```

The suite covers clean 12/20/35 WPM, wrong initial hints, human timing, AWGN,
QSB, an adjacent carrier, noise-only input, abrupt same-lane operator changes,
the malformed established clock, and the centred micro-MARK found in the
recording.

To repeat the recorded-WAV diagnostic without bundling private audio:

```sh
bash scripts/run_cw_recorded_wav_probe.sh \
  /path/to/MadModem_RX_20260817_052756_CW_Morse.wav 1790 25 1577
```

On the supplied file, the strong disputed section is emitted as four separate
events: `CQ `, `CQ `, `CQ `, `DE `.
