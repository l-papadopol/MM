# CW silence timing freeze fix — 0.5.78

## Failure observed on air

The receiver could decode a real station near 17–22 WPM and then, during a
long transmission pause, start following receiver noise. The public WPM rose
to 50 WPM (and older builds could display 70 WPM), while the timing diagnostic
showed impossible or unstable families such as 25/153 ms. The resulting text
was dominated by short Morse symbols and `E/T/I/S`-like garbage.

The supplied screenshots separate the two situations clearly:

- ambiguous/noise-dominated lane: about 15.9 dB prominence but only 0.4 dB
  advantage over the second peak, while timing reported roughly 48 WPM;
- real selected carrier: narrow peak with about 6.2 dB advantage over the
  second peak and a plausible timing estimate near 17 WPM.

The visible energy in the first case was not sufficient proof that the marker
still identified one unique keyed carrier.

## Root causes

1. Carrier evidence used a slow symmetric `+1/-1` integrator. After a strong
   transmission, stale evidence could survive for more than two seconds.
2. Session hold was fixed at 0.9 s and carrier-loss confirmation at another
   0.8 s, independent of WPM. At high speed this admitted many noise runs;
   at very low speed it could still be too short for a legitimate word gap.
3. The same permissive PSD test was used both to maintain a session and to
   authorize timing adaptation.
4. An established clock was still refined by isolated MARK durations. A long
   train of small but locally coherent noise pulses could therefore walk the
   short cluster down toward the 24 ms lower bound.
5. A reset that retained timing means did not retain the evidence that made the
   prior trustworthy, allowing the next random pair to become authoritative.
6. A relative pair accepted an unrealistically tiny separating SPACE, so a
   short Schmitt hole inside noise could manufacture a false fast clock.

## New carrier boundary

The tracker now keeps two distinct PSD decisions:

- `spectrumLanePresent`: permissive maintenance gate for an already valid
  session, QSB and known neighbouring lanes;
- `spectrumTimingCentered`: strict timing gate requiring a prominent, narrow
  and sufficiently unique selected peak.

Only the strict gate can acquire or adapt the dit/dah model. A maintained
session may rescue text when the first element begins before the next 256 ms
PSD frame, but that rescue is explicitly text-only and cannot move WPM.

A new timing acquisition requires repeated evidence and the strict selected
carrier. The ambiguous 0.4 dB peak-margin case is therefore not allowed to
install a clock, while a narrow carrier with a clear peak advantage remains
eligible.

## Adaptive silence handling

Session hold is now expressed in Morse time rather than a fixed wall-clock
constant:

- carrier hold: approximately 8.5 dit, bounded from 0.30 to 2.20 s;
- carrier-loss confirmation: approximately 1.8 dit, bounded from 0.08 to 0.45 s.

This preserves a normal seven-dit word gap at slow and fast speeds, then freezes
the temporal feed shortly after a genuinely long pause.

Carrier evidence has asymmetric attack/release:

- valid PSD observation: `+2`;
- invalid PSD observation: `-4`.

Two bad spectrum observations now remove a stale lane instead of requiring up
to eight 256 ms decrements.

When the carrier is lost the tracker:

1. flushes the last real character;
2. stops submitting MARK/SPACE runs;
3. preserves the trusted dit/dah prior and displayed WPM;
4. resets only the carrier discriminator;
5. waits for a new strict carrier acquisition.

The Runtime Log reports:

```text
carrier lost: timing frozen; WPM and dit/dah prior retained
```

## Pair-only timing adaptation

The temporal decoder no longer changes dit or dah from a single MARK. Timing
moves only when two trusted MARKs form an informative short/long pair separated
by a plausible intra-character SPACE.

For an established clock:

- pair durations must remain compatible with the existing short and long
  families;
- the geometric threshold must remain compatible with the previous threshold;
- the separating SPACE must be between 0.32 and 2.10 times the short MARK;
- each accepted pair changes a cluster by at most 1.5 percent;
- the long/short model is bounded to a maximum ratio of 4.2.

This retains the K4ICY-inspired relative-pair/geometric-mean principle while
preventing isolated noise pulses from changing speed.

## Publication behaviour

Characters remain live and character-by-character. A maintained known carrier
session may authorize a duration-compatible MARK for text when a PSD frame
arrives late, but every timing update still requires strict centred-carrier
support. Unknown patterns are not appended as `?`.

## Regression derived from the live failure

The production-path test now verifies a real 20 WPM message followed by six
seconds of beating narrow-band noise. It asserts all three conditions:

- the decoded message remains exact;
- the carrier gate closes;
- final WPM remains between 16 and 25 instead of moving to 50.

The suite also retains the direct 60/180 ms timing model followed by a storm of
7–50 ms uncentred runs; it must emit no text and retain the original clock.

## Unchanged subsystems

This fix does not modify FT8, FT4, the FT ghost-candidate gate, waterfall DSP or
OpenGL rendering, AudioEngine, CW TX, sequencer, CAT/PTT, rotator or logbook.
