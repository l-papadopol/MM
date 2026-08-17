# Native CW receiver architecture

MadModem contains one production CW receive implementation in this package.
This checkpoint is the Bayesian comparison variant of the adaptive sequence
beam; historical geometric-rescue and unrelated experimental branches remain
absent from the active source and build.

## RX A / RX B pipeline

Each receiver owns:

1. an exact-tone complex mixer and bounded neighbouring-lane separator;
2. a fourth-order narrow I/Q filter and edge-safe carrier envelope;
3. `CwCarrierDiscriminator`, which emits timestamped logic runs plus quality;
4. `CwRelativeTimingTask`, a dedicated temporal worker;
5. `CwRelativeTimingDecoder`, which estimates robust dit/dah and SPACE families;
6. `CwMorseBeamDecoder`, whose paths carry Bayesian dit/dah/gap duration
   posteriors and emit only stable credible-posterior token prefixes.

The task boundary is a queue of `CwLogicRun` values. Durations are measured in
the DSP path, so temporal-worker scheduling cannot alter Morse timing.

## Timing and sequence replay

The timing model locks from relative short/long MARK pairs. Established timing
moves only when both pair members imply a coherent speed change. Element,
character and word spaces are trained by relative likelihood. Each SPACE is
scored against both the robust measured family and a canonical 1/3/7-unit
fallback at the acquired dit scale. Soft semi-Markov duration penalties retain
multiple plausible boundaries without allowing a one-unit hand-key gap to
become a word merely because the initial WPM hint was wrong.

The beam keeps a bounded window of observations that have not yet been
published. Whenever timing changes, that window is replayed. A wrong WPM hint
therefore cannot make the first dash an irreversible dot. Up to 16 equivalent-
merged hypotheses are retained. Each owns Normal-Inverse-Gamma posteriors for
dit, dah and the three gap families. MARK/SPACE, QSB, noise, SNR, coherence and
centering evidence update predictive Student-t log likelihoods. Publication
uses shared 98.5% credible posterior mass or a 99.2% decisive path, normally one
following element/gap later.

## Carrier session

A carrier session is represented by a probability rather than a fixed N-dit
hold. It attacks quickly with a narrow qualified PSD lane and decays according
to the learned word-space family and prior lane stability. This supports normal
and stretched word gaps without keeping a dead noisy lane active indefinitely.

## QSB

Very short OFF notches between two compatible MARK fragments can be repaired as
a single MARK. The rule is duration- and evidence-bounded; a normal one-dit
intra-character space is not automatically merged.

## Isolation

CW changes are confined to the selected-tone RX path and its native tests. They
do not modify AudioEngine, FT8/FT4 timing, waterfall rendering, CW TX, CAT/PTT,
sequencer or other modem paths.
