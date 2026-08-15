# CW Bayesian beam decoder — 0.5.78

## Purpose

This checkpoint is a separate live-RX comparison branch derived from
`CW_ADAPTIVE_BEAM`. RX A/RX B, exact-tone filtering, AFC, AudioEngine, waterfall
and the carrier-session gate are unchanged. Only the probabilistic interpretation
of completed MARK/SPACE runs is replaced.

## Production path

```text
selected exact-tone complex carrier
  -> CwCarrierDiscriminator
       1 ms posterior MARK evidence
       completed runs + mean state/QSB/noise probabilities
  -> CwRelativeTimingTask
       robust station timing prior
  -> CwMorseBeamDecoder
       per-hypothesis Bayesian duration posteriors
       posterior-mass commit
  -> RX A/RX B text
```

The application still has two user CW receivers. `beamWidth=16` means at most
16 temporary interpretations inside each receiver; these are neither extra DSP
receivers nor extra threads.

## Bayesian state carried by each hypothesis

Every hypothesis owns five Normal-Inverse-Gamma posteriors in log-duration
space:

- dit;
- dah;
- intra-character element SPACE;
- inter-character SPACE;
- inter-word SPACE.

The external timing estimator supplies the initial prior. Low timing confidence
creates a broad, weak prior; a stable short/long-pair lock creates a narrower,
stronger prior. Fractional updates use the reliability of each RF observation,
so a clipped edge or low-confidence QSB fragment cannot move the model as much
as a centred, coherent high-SNR element.

Predictive scoring uses a robust Student-t kernel. The implementation omits the
degrees-of-freedom-only normalization term because it does not change ordering
inside the bounded beam and avoids `lgamma` in the live path.

## RF evidence

`CwCarrierDiscriminator` now exports, for each completed run:

- mean MARK probability;
- QSB probability;
- estimated noise probability;
- centred-carrier fraction;
- existing SNR, coherence and confidence.

`CwRelativeTimingDecoder` converts these to the probability that the reported
MARK or SPACE state is genuine. Duration likelihood is mixed with a broad
outlier model. A likely noise/QSB MARK may therefore be skipped by one path,
while clean paths continue to interpret it as a dot or dash.

## Commit rule

Raw score margins are no longer the primary commit rule. The beam is normalized
into posterior mass after every event. Text is committed when:

1. the hypotheses covering 98.5% credible mass share the same token prefix; or
2. the best path exceeds 99.2% posterior and its absolute RF evidence is high
   enough; or
3. the receiver is explicitly flushed, in which case the best complete path is
   closed without adding an artificial trailing word space.

Runtime Log lines identify this branch with:

```text
bayes=<hypotheses>/<confidence> post=<best posterior> odds=<best/second dB>
```

## Computational cost

The Bayesian layer runs on completed logic runs, not on 48 kHz audio samples.
At normal CW speeds this is only a few dozen observations per second for each of
two receivers. Arrays are bounded, the beam has at most 16 paths, pruning uses
partial selection, and there are no allocations proportional to reception time.
No AVX2 requirement or additional worker thread is introduced.

## Validation scope

The native production-path suite includes:

- exact `CQ CQ OG50YL OG50YL CQ CQ OG50YL` at 30.5 WPM with 8.8-dit word gaps;
- 38 WPM Farnsworth with an 18 WPM hint;
- 12 WPM with a 28 WPM hint;
- human timing with jitter and a 2.45 dah/dit ratio;
- AWGN, deep QSB, adjacent stronger carrier, broad-noise tail and noise-only;
- replay after a wrong initial timing prior;
- posterior metadata bounds and discriminator probability export;
- timestamp, restart, exact-pattern and micro-run regressions.

These tests validate the same C++ classes used in live RX; they are not a second
offline decoder. Real on-air A/B comparison against `CW_ADAPTIVE_BEAM` remains
the deciding test.
