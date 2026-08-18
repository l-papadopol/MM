# CW live gate and timing-integrity follow-up — 0.5.78

## Failure reproduced by the first live test

The first radio log of the silence-freeze decoder showed that the new carrier
boundary worked, but exposed four implementation defects around it:

1. after a mid-stream discriminator reset, the next completed SPACE could start
   at timestamp zero and appear as a pause lasting several or tens of seconds;
2. enabling RX B produced multiple clean restarts from tone selection,
   activation and first sample-rate initialization;
3. pre-lock receiver noise still produced a very large MARK/SPACE diagnostic
   storm even though the temporal decoder correctly refused most of it;
4. the asynchronous timing log could print a committed character together with
   the next open pattern, for example `commit="T" pattern=..`.

The same log also showed that asymmetric clipped short/long pairs could move an
established Auto-WPM clock. Timing adaptation now requires the short and long
members of an accepted pair to imply the same relative speed change.

## Corrections

### Live timestamp anchoring

`CwCarrierDiscriminator` now has an explicit active-run state. The first
observation after every reset starts a new run at the current live timestamp;
it cannot complete a synthetic run whose start time was zero. `flush()` also
refuses to emit a run when none is active.

### One restart per actual receiver change

The first audio block now configures the previously unknown sample rate without
performing another full receiver reset. A genuine sample-rate change during
reception still resets the DSP state. RX B is cleaned when disabled; enabling a
receiver that is already clean no longer resets it again. Duplicate sub-hertz
tone settings return before clearing diagnostics or touching the tracker.

### Pre-lock noise-run gate

While timing is frozen, only carrier-centred MARKs and high-quality elements
compatible with the retained timing family enter the short pre-roll. A SPACE is
held provisionally and retained only when a trustworthy following MARK proves
that it belongs to a possible Morse pair. Unqualified runs are neither sent to
the temporal task nor printed one by one. A compact suppression count is logged
when a valid carrier lock is recovered.

### Exact commit diagnostics

Every committed character now carries an immutable copy of the Morse pattern
that produced it through the timing worker queue. Word spaces carry an empty
pattern entry. Runtime log lines therefore report the pattern belonging to the
actual committed character rather than `currentPattern` from a later state.

### Safer established-clock adaptation

For an already established clock, an accepted short/long pair must scale the
short and long timing families consistently. A clipped dot paired with a normal
dash remains available for character classification but cannot move dit/dah or
Auto-WPM.

## Regression coverage

The production pure-C++ CW regression now additionally verifies:

- a reset at timestamp 10 s cannot emit a run starting at zero;
- the first sample-rate initialization creates no extra clean restart;
- a real sample-rate change still resets once;
- a committed `C` retains the exact `-.-.` pattern.

All previous clean, wrong-hint, AWGN, QSB, human-timing, adjacent-carrier,
message-plus-noise-tail and noise-only tests remain passing.

## Scope

Only the CW decoder and its native regression were changed. AudioEngine,
waterfall rendering/DSP, CW TX, FT8/FT4, CAT/PTT, sequencer, rotator, maps and
logbook were not modified.
