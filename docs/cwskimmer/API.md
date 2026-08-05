# Native CW API

## `CwCarrierDiscriminator`

Input: one-millisecond narrow-band carrier observations containing timestamp,
envelope, SNR, coherence and lane-centering evidence.

Output: current key state, calibrated thresholds and completed `CwLogicRun`
objects carrying mean MARK, QSB and estimated noise probabilities in addition
to SNR, coherence and centering. It assigns no Morse meaning.

## `CwRelativeTimingTask`

Owns the timing model and Morse beam on a dedicated C++ worker thread. The DSP
owner submits logic runs and time advances; the owner thread drains snapshots
and committed text. Reset, configuration and flush commands are synchronous
barriers.

## `CwRelativeTimingDecoder`

Consumes completed MARK/SPACE runs. It repairs qualified QSB splits, estimates
robust dit/dah pairs, trains element/character/word SPACE families by relative
likelihood and passes accepted observations to `CwMorseBeamDecoder`.

## `CwMorseBeamDecoder`

Maintains a bounded replay window and up to 16 concurrent hypotheses. Each
hypothesis owns Normal-Inverse-Gamma log-duration posteriors for dit, dah and
three SPACE families. MARKs branch into dot/dash (plus a probability-controlled
noise/QSB skip); SPACEs branch into element, character and word interpretations.
Invalid Morse-tree prefixes are removed, equivalent states are merged, and only
prefixes shared by credible posterior mass or a decisively dominant path are
committed. Results include immutable patterns, sequence confidence, best
posterior, posterior odds and current hypothesis count.

## `SelectedToneCwTracker`

Combines exact-tone DSP, discriminator, temporal task, adaptive carrier-session
probability, bounded AFC, diagnostics, spectrum frames and live logging for one
receiver.

## `CwSkimmerEngine`

Discovers persistent full-passband carrier lanes. It does not decode Morse or
publish text.
