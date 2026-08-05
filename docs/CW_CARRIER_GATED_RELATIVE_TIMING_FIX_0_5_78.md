> Superseded by `CW_SILENCE_TIMING_FREEZE_FIX_0_5_78.md`, which replaces the fixed carrier-hold/loss timings and isolated-MARK adaptation described below.

# CW carrier-gated relative timing fix — 0.5.78

## Evidence from the first live test

The first on-air test of the clean-restart CW architecture exposed a failure
that the synthetic suite did not represent.  A valid station could initially
produce a credible clock near 20 WPM, for example approximately 60 ms dits and
170-190 ms dahs.  After the selected carrier disappeared, the discriminator
continued emitting many 4-50 ms MARK/SPACE fragments from narrow-band noise.
The temporal task accepted those fragments, moved the timing model to 16-43 ms
and displayed 70 WPM, then published long E/T-heavy garbage strings.

The runtime log also showed repeated marker notifications at the same frequency,
each causing a complete RX reset.

## Root causes

1. The exact-tone coherent/residual ratio was treated as carrier evidence. In
   narrow-band noise it can report a large local value even without a narrow
   selected carrier in the measured PSD.
2. `carrierCentered` was accumulated with logical OR. One stale centred sample
   qualified an entire run.
3. The carrier hold timer was copied into every run as current carrier evidence.
   Its intended purpose was only to keep a valid session alive across Morse
   spaces.
4. Once temporal feeding started, acquisition-only coherent evidence could keep
   it active indefinitely after carrier loss.
5. One inconsistent short/long pair could immediately replace an established
   timing model. Repeated noise pairs therefore collapsed a 60/180 ms model to
   approximately 16/30 ms.
6. Implausible micro-MARKs were still appended to the current Morse pattern even
   when they were rejected by the timing-cluster update.
7. Unknown Morse patterns were displayed as `?`, increasing visible garbage.
8. Repeated sub-hertz marker updates reset both the discriminator and timing
   task although the selected tone had not materially changed.

## Carrier discriminator boundary

`CwLogicRun` now carries `carrierCenteredFraction`. A MARK is centred only when
at least 55 percent of its samples are backed by the currently measured narrow
PSD lane. A single stale sample can no longer qualify a complete run.

The session hold and per-run evidence are separate:

- current PSD evidence qualifies a MARK;
- the 0.9 s hold preserves the session across normal Morse spaces;
- acquisition-only coherent evidence is allowed only before temporal lock;
- after 0.8 s without qualified PSD evidence, temporal feeding stops and the
  last trustworthy timing prior is retained for the next transmission.

A stable carrier requires repeated PSD evidence, at least 8 dB prominence, a
measured peak no wider than 30 Hz and sufficient separation from the next peak.
The known-neighbour exception remains bounded for the existing multi-lane path.

## Relative timing protection

The relative short/long-pair bootstrap is still immediate before a trustworthy
clock exists. Once at least three valid pair observations have established the
clock:

- an inconsistent pair cannot replace it;
- the state may report `REACQUIRE`, but the previous clock remains authoritative;
- a new transmission can acquire immediately because carrier loss resets pair
  evidence while retaining only a weak timing prior;
- short/long cluster movement is limited to 2.5 percent per accepted update;
- MARKs incompatible with both established duration families are discarded;
- uncentred MARK rescue requires strong coherence/confidence and a duration
  close to the established dit/dah families;
- a rejected micro-MARK is treated as a glitch inside the surrounding SPACE,
  not as a new Morse element.

The automatic WPM range is restored to 5-50 WPM. A stale saved value of 70 WPM
cannot become the displayed or decoder clock.

## Publication gate

A character is published only when its temporal pattern contains at least one
trusted carrier-backed MARK and adequate mean run evidence. This keeps genuine
single-element `E` and `T`, but blocks the post-transmission E/T flood.
Unknown patterns are not appended to user text; they remain diagnosable through
the runtime state rather than becoming visible garbage.

## Marker reset guard

`setToneHz()` is now a no-op for changes smaller than 0.5 Hz. Duplicate UI signal
fan-out therefore cannot repeatedly erase an active CW lock. A real tone change
still performs exactly one clean reset.

## Added live-failure regressions

The native production test now includes:

- a valid 20 WPM 60/180 ms clock followed by the 7-50 ms run storm observed in
  the supplied log; no text may be emitted and the clock must remain near 20 WPM;
- a stale 70 WPM setting, which must be clamped to 50 WPM;
- repeated identical/sub-hertz marker updates, which must not reset the RX;
- a real marker change, which must reset exactly once;
- a complete `CQ CQ DE TEST` reception followed immediately by six seconds of
  beating broad narrow-band noise; the message must remain exact, no tail text
  may appear, and the carrier gate must close;
- the previous clean, noisy, QSB, hand-keyed, adjacent-carrier and noise-only
  cases.

## Validation boundary

The new tests reproduce the concrete failure class visible in the supplied live
log rather than only ideal message generation. They still do not replace a new
on-air test with the user's receiver, audio chain and real stations.
