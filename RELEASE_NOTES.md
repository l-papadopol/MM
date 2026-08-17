# MadModem 0.5.8 — hardened runtime checkpoint

## 0.5.8 release focus

- Preserved the restored, high-sensitivity FT8/FT4 candidate search, gate,
  boundary, LDPC and sequencer path without speculative decoder changes.
- Moved audio capture to its own thread and added bounded, drop-oldest UI/DSP
  relays so a slow GUI or waterfall cannot grow an unbounded queued-copy backlog;
  the FT worker keeps its existing direct queued input from AudioEngine.
- Made CAT/PTT fail closed: rear/data routing never silently falls back to normal
  PTT, requested USB/Data mode failures abort TX, shutdown attempts an acknowledged
  PTT OFF, and logs no longer claim OFF unless Hamlib confirms it.
- Added strict RIFF/WAV validation and a shared reader, streaming resampling state
  for FT/MSK144/Q65, UTC-period assembly, joined MSK144 jobs and generation guards.
- Made Q65 capability reporting truthful. TX remains available in every build;
  RX starts only when the optional full FFTW/MSHV decoder backend is compiled.
- Added atomic ADIF persistence, bounded terminal/table history, debounced terminal
  highlighting, bounded OpenStreetMap tile downloads with normal TLS verification,
  and an explicit classic-RIFF recorder size limit.
- Removed UI-thread blocking NTP probes, decoder-thread termination, detached
  MSK144 jobs and nested event processing in offline SSTV/WEFAX imports.
- Kept the compact RTTY contest panel inside the side-tab viewport and retained
  the CAT-synchronized FT band/frequency behavior.
- Registered source guards and native regressions with CTest. Distribution
  packages are test-gated on Linux, Windows and macOS, while a portable Linux
  build plus the complete CTest suite runs on every push and pull request.
- Qualified the shared WAV-reader calls explicitly and removed the duplicate
  local wrappers, fixing the GCC/MinGW argument-dependent lookup ambiguity
  found by the first GitHub 0.5.8 build.
- Removed the final locale-dependent Python reads from CTest source guards, so
  non-ASCII UTF-8 source is decoded correctly on MSYS2/Python 3.14.
- Stabilized the synthetic human-timing CW corpus across libc++ and libstdc++;
  macOS and Linux now exercise the same jitter/noise waveform while retaining
  an exact `CQ CQ DE IZ6NNH` assertion.

## Retained 0.5.79 feature baseline

- Added a dedicated RTTY `Contest mode` side tab between Mode and Rotator. It owns the contest operating surface (rules/profile, exchange fields, transactional serial, live score and contest macros) and mirrors the normal RTTY QSO form bidirectionally while logging through the same single QSO owner.
- Added live decoded RTTY text inside the Mark/Space CRT scope. Auto polarity now uses the CAT demodulation mode (USB/LSB/RTTY/RTTYR when Hamlib reports it) as a prior and continuously compares normal/reverse ITA2 framing evidence; manual Reverse remains authoritative when Auto polarity is disabled.
- Fixed deployment of external `rtty_rules`: the runtime still loads exactly one file from `applicationDirPath()`, and Linux/Windows/macOS packaging now verifies that the file is installed beside the executable (`bin/rtty_rules` or `MadModem.app/Contents/MacOS/rtty_rules`).
- Added the external, reloadable `rtty_rules` contest engine and bundled 2026 RTTY contest catalog. Contest exchange fields, macros, serial policy, duplicate scope, multipliers and scoring are data-driven rather than hard-coded per contest.
- Added RTTY contest click-to-fill fields, CAT-derived band handling, transactional serial numbering and live session/contest score display.
- Added MSK144 `F Tol` presets (50/100/200/500 Hz).
- Consolidated the linear FT8/FT4 sequencer, caller queue and fixed live decode resource budget from the late 0.5.78 stabilization work.
- Fixed the waterfall "accordion" effect at quiet V/U/SHF receiver edges: display leveling now always uses the complete selected spectrum and never converts a signal-dependent partial-band estimate into a hard black mask.
- Preserved the OpenGL stable-scroll/minimize handling: GUI presentation state cannot change waterfall time scale or reset decoder state.


## CW live Bayesian posterior beam comparison branch

This package is a separate live-RX alternative to `CW_ADAPTIVE_BEAM`. It keeps
both user receivers RX A/RX B and the same exact-tone DSP, AFC, carrier gate and
robust global timing prior. The Morse beam now assigns every hypothesis its own
Normal-Inverse-Gamma posteriors for dit, dah and element/character/word gaps.
Predictive Student-t likelihoods are updated with fractional RF evidence derived
from mean MARK probability, QSB probability, estimated noise probability, SNR,
coherence and carrier centering.

Commit decisions use normalized posterior mass: shared prefixes across the
98.5% credible set are published, while a single path may commit only above a
99.2% posterior and sufficient absolute evidence. Runtime Log lines use
`bayes=`, `post=` and `odds=` fields so this branch can be distinguished during
on-air A/B testing. The implementation remains in the production RX live path;
there are still only two CW receivers and no additional decoder threads.

The complete native RF/timing regression suite passes, including the exact
30.5 WPM `CQ CQ OG50YL...` case, wrong WPM hints in both directions, Farnsworth,
human timing, AWGN, QSB, adjacent carrier, noise tail, micro-run storm and
noise-only cases. AudioEngine, waterfall, CW TX, FT8/FT4 and all other modes are
unchanged.

## Previous CW live adaptive beam decoder

The CW RX production path now separates carrier discrimination, robust timing
family estimation and Morse sequence decisions. `CwMorseBeamDecoder` keeps a
bounded replay window of still-uncommitted MARK/SPACE observations, re-scores
them whenever dit/dah or spacing centres change, and delays publication until
near-best paths share a stable token prefix or one path is decisive. A wrong WPM
hint can therefore no longer irreversibly classify the first dash before the
first trustworthy short/long pair is available.

SPACE-family learning now uses relative likelihood instead of one hard boundary.
Measured and canonical 1/3/7-unit spacing models are evaluated together with
soft semi-Markov duration constraints, allowing both 38 WPM/18 WPM-hint and
12 WPM/28 WPM-hint cases to decode without separate thresholds. The acquisition
replay also preserves the completed SPACE that triggers lock, preventing the
recovered first character from being fused with the following character.

The previous 12-dit carrier-session hold has also been replaced by a session
probability whose decay follows the learned word-space duration and prior lane
stability. The 30.5 WPM `CQ CQ OG50YL...` regression with 8.8-dit word gaps still
passes, together with 38 WPM/11-dit Farnsworth and 12 WPM/opposite-hint
regressions, clean, AWGN, QSB, human-timing, adjacent-lane, noise-tail,
micro-run and noise-only cases. Runtime commits report the exact
Morse pattern plus beam hypothesis count/confidence.

The implementation is live in RX A/RX B; it is not an offline-only decoder.
AudioEngine, waterfall, AFC, CW TX, FT8/FT4 and all other modes remain unchanged.

## Previous interim CW 30 WPM word-boundary follow-up (superseded)

The first live test of the gate-integrity build showed a clean, strong 1548 Hz
carrier (about 40 dB prominence, 5.9 Hz width and 91% coherence) but intermittent
losses and insertions while receiving the repeated text `CQ CQ OG50YL OG50YL`.
The virtual paper showed correct 39/125 ms elements, so the failure was not the
filter or dit/dah classifier: the carrier-session hold was only 8.5 dits, while
the measured inter-word gap reached about 8.8 dits. The timing feed could
therefore freeze immediately before the first element of the next word and
reacquire from noisy pre-roll.

An interim build raised the hold from 8.5 to 12 dits and added a bounded
first-element rescue, proving that the gate boundary caused the missing leading
elements. The current adaptive-beam checkpoint supersedes that fixed hold with
the carrier-session probability described above while retaining the exact
30.5 WPM/8.8-dit OG50YL regression.

## CW live gate and timing-integrity follow-up

The first on-air test of the silence-freeze decoder exposed a timestamp-zero
run after mid-stream carrier reset, duplicate RX B restarts, pre-lock micro-run
log storms and commit/pattern diagnostic mismatches. Runs now have an explicit
active lifetime anchored to the current stream timestamp. First sample-rate
initialization no longer performs another reset, RX B is reset when disabled
rather than again when enabled, and frozen noise runs are filtered before the
temporal worker and Runtime Log. Each committed character now carries its exact
Morse pattern through the asynchronous worker. Established clock adaptation
also requires short and long pair members to indicate the same relative speed
change.

New native regressions cover all four defects. The complete pre-existing CW
regression suite remains passing.

## CW long-silence timing freeze

The second radio test exposed a remaining boundary error: after a valid station
stopped for a long pause, stale carrier evidence could keep the temporal task
alive, let background noise create short MARK pairs and drive Auto-WPM toward
50. Carrier qualification is now split into maintenance and strict timing gates.
Hold/loss timing follows the measured dit, carrier evidence releases rapidly,
and dit/dah adaptation occurs only from trusted short/long pairs. Long silence
therefore freezes the previous WPM instead of learning the noise floor.

The production regression now requires exact text, carrier-gate closure and a
final WPM between 16 and 25 after a valid 20 WPM message followed by six seconds
of beating narrow-band noise.

## Previous CW live-log correction: carrier-gated relative timing

The first radio test of the clean-restart CW receiver exposed a real failure not
covered by the original synthetic suite: after a correct 19-22 WPM lock, noise
fragments could keep the temporal task active, collapse the clock to 70 WPM and
publish pages of E/T-heavy garbage. The receiver now distinguishes current PSD
carrier evidence from the normal word-gap hold, closes temporal feeding after
carrier loss, protects an established clock from micro-run pairs and requires
carrier-backed evidence before publishing a character. Auto-WPM is again
limited to 5-50 WPM. Duplicate marker notifications no longer reset an active
receiver.

New regressions reproduce the observed 7-50 ms run storm and a valid message
followed by beating narrow-band noise. Both complete without tail text or clock
collapse.

## CW clean restart: carrier discriminator + relative timing task

The CW RX path has been rebuilt around one source-visible architecture. Old
Bayesian, geometric-rescue and causal semi-Markov decoder sources, experiments
and audit scripts have been removed.

Each RX A/RX B receiver now uses:

- exact-tone complex baseband and bounded neighbouring-lane separation;
- a dedicated `CwCarrierDiscriminator` that emits timestamped MARK/SPACE runs;
- a real parallel `CwRelativeTimingTask` worker;
- a relative temporal decoder that acquires short/long MARK pairs continuously;
- a geometric-mean dit/dah threshold;
- separate element, character and word SPACE families;
- immediate character publication at the character gap;
- bounded repair of short QSB notches that split one dash;
- no dictionary, language model or offline-only decoder.

The existing Runtime Log button is visible in CW mode. It buffers up to 5000
lines and shows discriminator runs, carrier lock, timing state, dit/dah centres,
geometric threshold, WPM and commits for both receivers.

The relative-pair/geometric-mean idea is an independent implementation inspired
by the public K4ICY CW Decoder description and sketch. No K4ICY source code is
included.

## CW validation

The pure-C++ regression compiles the production pipeline and checks:

- non-ideal human dash ratio;
- clean 20 WPM;
- 35 WPM with an 18 WPM initial hint;
- additive noise;
- deep QSB notches;
- human MARK/SPACE jitter;
- a stronger known carrier at +70 Hz;
- noise-only suppression.

The suite passes with GCC, Clang, AddressSanitizer, UndefinedBehaviorSanitizer
and ThreadSanitizer. Qt application compilation was not performed in the
preparation container because Qt development packages are absent.

## FT8 / FT4

The current live FT8/FT4 adaptive runtime, efficient gate/boundary worker
budgets, atomic sequencing, corrected FT4 SIC and sensitivity-regression
rollback remain unchanged by this CW rebuild.

## Waterfall

The circular OpenGL waterfall, HiDPI viewport/overlay correction, smooth
one-row presentation and WSJT-X-style per-row flattening remain unchanged.

## Other subsystems

Audio capture, CW TX, CAT/PTT, rotator, logbook, maps, multilingual UI and all
other modem paths are unchanged.

### 0.5.79 GitHub build fix
- Fixed a C++ declaration-order regression in `mainwindow.cpp`: `rttyContestAdifKey()` is now defined before the RTTY contest log code that uses it. This restores compilation on both Qt5/GCC Linux and Qt6/AppleClang macOS ARM64 without changing runtime behavior.
