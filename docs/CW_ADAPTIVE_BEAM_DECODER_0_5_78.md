# CW adaptive timing and probabilistic Morse beam — 0.5.78

## Objective

Replace the remaining threshold-driven character commit with a live adaptive
sequence decoder. The change is confined to RX A/RX B after the existing
exact-tone carrier discriminator. AudioEngine, waterfall, AFC, CW TX and all
other modem modes are unchanged.

## Production data flow

```text
selected-tone complex DSP
  -> CwCarrierDiscriminator
       completed MARK/SPACE runs
       confidence, SNR, coherence, centred-carrier fraction
  -> CwRelativeTimingTask (one worker per RX)
       robust short/long pair timing
       likelihood-based element/character/word spacing families
       QSB split repair
  -> CwMorseBeamDecoder
       bounded replay window
       concurrent dot/dash and spacing-boundary hypotheses
       stable-prefix commit
  -> live RX text callback
```

## Why replay is required

A WPM setting is only an acquisition hint. With a wrong 15 WPM hint, an actual
132 ms dash can initially lie between the old 80 ms dot and 240 ms dash centres.
A one-pass classifier makes that early decision irreversible. The beam stores
only the short uncommitted observation window and re-scores it every time the
relative timing model changes. Once a 54/132 ms short/long pair is learned, the
first 132 ms observation is reclassified as a dash before publication.

## Beam state

Each hypothesis contains:

- the current Morse-tree prefix;
- complete but not yet published character/space tokens;
- a cumulative negative log-likelihood score;
- observation quality evidence;
- the event boundary of each token.

MARK observations branch to dot, dash and—only at low quality—an expensive skip
path. SPACE observations branch to element, character and word hypotheses.
Invalid Morse-tree prefixes are pruned immediately. Equivalent states are
merged and at most 16 hypotheses are retained.

A token is emitted when the near-best hypotheses share the same token and event
boundary. A single path can commit only with a decisive score margin. After a
commit, the consumed observations are removed and the remaining tail is replayed.
This keeps latency near one following element/gap while avoiding phrase-level
or end-of-transmission buffering.

## Timing and spacing adaptation

Dit/dah centres still move only from credible neighbouring short/long pairs.
An established clock accepts an update only when short and long scale together,
which rejects clipped-dot drift. Element, character and word SPACE families are
selected by relative log-distance likelihood.

The spacing likelihood combines two independent priors: the measured robust
family centre and the canonical 1/3/7-unit centre at the acquired dit scale.
Soft semi-Markov minimum-duration penalties prevent a jittered one-unit gap from
being promoted to a character or word boundary. Consequently the operator's WPM
setting remains useful for acquisition but cannot force every 12 WPM character
gap to look like a word when the hint was 28 WPM. Ambiguous observations may be
kept by the beam without strongly training any family.

## Carrier-session continuity

The previous 8.5/12-dit hold has been removed as the primary rule. Each selected
receiver now maintains a carrier-session probability:

- fast attack while a narrow PSD lane is present;
- decay using the learned word-space duration;
- slower decay after strong prior carrier evidence;
- bootstrap support from a coherent exact-tone observation only before lock.

The normal 8.8-dit live word gap remains qualified without assigning one fixed
hold to every WPM and every operator. Confirmed carrier loss still has a bounded
final confirmation time as a safety mechanism.

## Live diagnostics

Every commit log line contains:

- timing state;
- exact immutable Morse pattern;
- dit/dah and threshold values;
- WPM;
- current beam hypothesis count and sequence confidence.

## Validation

The production pure-C++ regression includes:

- wrong initial clock with replay of the leading dash;
- exact pattern preservation;
- `CQ CQ OG50YL OG50YL CQ CQ OG50YL` at 30.5 WPM with 8.8-dit word gaps;
- 38 WPM with 11-dit Farnsworth word gaps and an 18 WPM hint;
- 12 WPM with a deliberately wrong 28 WPM hint;
- clean 20 and 35 WPM;
- AWGN, deep QSB and human timing jitter;
- non-ideal 2.45 dash ratio;
- stronger known adjacent carrier at +70 Hz;
- valid message followed by broad beating noise;
- micro-run storm and noise-only suppression;
- reset/restart/timestamp integrity.

GCC and Clang pass with `-Wall -Wextra -Werror`. The complete native suite
also passes AddressSanitizer and UndefinedBehaviorSanitizer. On this container
the optimized regression completes in about 1.5 seconds with about 8 MB peak
RSS. The decisive acceptance remains RX live/on-air testing.

## Reference-study scope

The architecture was informed by study of the two open-source skimmer projects
provided by the user: one demonstrated independent channel state and delayed
window interpretation; the other demonstrated stage separation and robust
wideband-floor concepts. MadModem uses an independent implementation and does
not copy their source code.
