# MadModem changelog

## 0.5.8 — integral source hardening — 2026-08-17

- Preserved the validated FT8/FT4 candidate, gate, LDPC and sequencer core.
- Isolated capture from the GUI and bounded UI/DSP audio delivery queues.
- Made CAT audio routing, mode selection, PTT transitions and shutdown fail closed.
- Consolidated strict WAV parsing and fixed streaming resampling/period assembly.
- Replaced detached MSK144 work with owned, joined, generation-guarded jobs.
- Gated Q65 RX on the real FFTW/MSHV backend and moved it to a decoder thread.
- Made ADIF writes atomic and corrected contest STX/SRX import mapping.
- Bounded runtime logs, terminals, decode tables and online map tile payloads.
- Kept normal TLS certificate verification for OpenStreetMap requests.
- Made classic-RIFF recorder overflow and finalization errors explicit.
- Removed blocking clock probes, forced thread termination and nested UI event loops.
- Added runtime-hardening, CAT/FT synchronization and layout regression guards.
- Registered the guards and native regressions with CTest, made every package
  build test-gated and added a Linux build/CTest workflow for every push and PR.

## 0.5.79 — RTTY contest engine and stable full-band waterfall — 2026-08-15

- Added a dedicated RTTY Contest mode side tab between Mode and Rotator, with mirrored QSO fields and a second view of the data-driven contest macro buttons; both views write through the existing single RTTY QSO/log transaction.
- Added live RTTY decode text to the Mark/Space tuning CRT and a single automatic polarity resolver: CAT USB/LSB/RTTY mode supplies the initial prior while parallel normal/reverse ITA2 framing evidence can correct it.
- Fixed `rtty_rules` deployment so the external file is always beside the executable and added release-package checks for Linux, Windows and macOS.
- Added reloadable external `rtty_rules` contest profiles with dynamic exchange parsing, macro sets, serial policy, duplicate rules, multiplier/scoring policy and official-rule metadata.
- Added RTTY contest session UI, click-to-fill structured RX fields, CAT-derived band selection and live QSO/multiplier/score counters.
- Added the bundled `RTTY_RULES_CATALOG_2026.txt`.
- Added MSK144 `F Tol` presets at 50/100/200/500 Hz.
- Promoted the stabilized linear FT8/FT4 sequencer and caller queue into the release baseline.
- Removed signal-dependent waterfall passband masking. Quiet but valid receiver-edge noise now participates in full-band lower-envelope leveling instead of being forced to exact black; strong carriers therefore cannot make the visible waterfall narrow/widen.
- Kept one FFT history row per DSP row and preserved OpenGL ring history across transient presentation changes.

## 0.5.78 — CW Bayesian posterior beam — 2026-08-05

- Created a separate live-RX comparison branch from `CW_ADAPTIVE_BEAM`.
- Added per-hypothesis Normal-Inverse-Gamma posteriors for dit, dah and the
  element/character/word SPACE families in log-duration space.
- Replaced heuristic distance costs with robust predictive Student-t
  log-likelihoods mixed with a broad outlier model.
- Exported mean MARK, QSB and estimated noise probabilities from completed
  carrier-discriminator runs and included them with SNR, coherence and carrier
  centering in fractional Bayesian updates.
- Replaced raw beam-margin commits with 98.5% credible-mass consensus and a
  99.2% decisive-posterior path guarded by absolute observation confidence.
- Added `bayes`, best-posterior and posterior-odds Runtime Log diagnostics.
- Added posterior-bound and discriminator-probability regressions; all existing
  clean, WPM-mismatch, Farnsworth, human, QSB, adjacent-carrier, noise-tail,
  micro-run and noise-only regressions remain passing.
- Kept exactly two user CW receivers and introduced no extra DSP threads,
  AudioEngine, waterfall, FT8/FT4 or CW-TX changes.

## 0.5.78 — CW adaptive beam decoder — 2026-08-05

- Added `CwMorseBeamDecoder` to the production RX A/RX B temporal worker.
- Kept up to 16 concurrent dot/dash and element/character/word boundary paths.
- Added short-window replay whenever the learned timing model changes, so an
  incorrect WPM hint cannot permanently corrupt the leading dash.
- Replaced hard SPACE-family assignment with relative-likelihood training.
- Added measured-plus-canonical 1/3/7-unit spacing likelihoods and soft
  semi-Markov family floors, so large WPM-hint errors do not create false word
  boundaries or split jittered human characters.
- Preserved the completed SPACE that triggers carrier acquisition before replay,
  fixing first-character/next-character fusion at high speed.
- Replaced the fixed 12-dit session hold with an adaptive carrier-session
  probability derived from learned word spacing and prior lane stability.
- Added beam hypothesis/confidence values to CW Runtime Log commit lines.
- Added direct replay, 38 WPM/11-dit Farnsworth with an 18 WPM hint, and
  12 WPM with a 28 WPM hint regressions; retained the exact live 30.5 WPM,
  8.8-dit `CQ CQ OG50YL...` case plus all previous RF/noise cases.
- Passed GCC `-Wall -Wextra -Werror` and ASan/UBSan on the production CW path.
- AudioEngine, waterfall, AFC, CW TX, FT8/FT4 and other modes are unchanged.

## 0.5.78 — CW 30 WPM word-boundary fix — 2026-08-05

- Fixed intermittent losses and inserted E/T/N around the repeated live text
  `CQ CQ OG50YL OG50YL` despite a clean 39/125 ms element stream.
- Increased the adaptive carrier-session hold from 8.5 to 12 dits because the
  measured inter-word gap reached about 8.8 dits and could expire the gate just
  before the next word.
- Bounded pre-lock replay to 6.5 dits so fragmented idle noise cannot be walked
  back into the first acquired C or Q.
- Added a controlled first-element boundary rescue while preserving the rule
  that Auto-WPM adaptation requires centred carrier evidence.
- Added an exact 30.5 WPM regression with 8.8-dit word gaps for
  `CQ CQ OG50YL OG50YL CQ CQ OG50YL`; all prior CW regressions remain passing.
- AudioEngine, waterfall, CW TX, FT8/FT4 and all other modem paths are unchanged.

## 0.5.78 — CW live gate and timing-integrity follow-up — 2026-08-05

- Fixed post-reset MARK/SPACE runs using timestamp zero, which produced false
  multi-second SPACE durations in live reception.
- Removed duplicate RX B clean restarts caused by activation and first
  sample-rate initialization.
- Added a frozen/pre-lock run gate that keeps only credible carrier-backed
  pre-roll and suppresses receiver-idle micro-run log storms.
- Preserved the exact Morse pattern beside each committed character across the
  asynchronous timing worker, fixing mismatched commit diagnostics.
- Required established short/long timing pairs to imply a consistent common
  speed change before adapting Auto-WPM.
- Added native regressions for live timestamp anchoring, restart count and exact
  commit-pattern association; all previous CW regressions remain passing.
- AudioEngine, waterfall, CW TX, FT8/FT4 and all other modem paths are unchanged.

## 0.5.78 — CW silence timing freeze fix — 2026-08-04

- Fixed the remaining on-air failure where long transmission pauses let the CW
  receiver follow background noise and raise Auto-WPM to 50.
- Split PSD qualification into a permissive lane-maintenance gate and a strict
  timing-centred gate; an ambiguous peak only 0.4 dB above its neighbour can no
  longer acquire or adapt the clock.
- Replaced fixed 0.9 s + 0.8 s carrier-loss timing with an adaptive 8.5-dit hold
  and 1.8-dit confirmation, preserving normal word gaps at every WPM.
- Changed carrier-evidence attack/release from +1/-1 to +2/-4 so a dead lane no
  longer remains trusted for multiple seconds.
- Made dit/dah adaptation pair-only, tightened relative-pair consistency and
  capped the long/short timing ratio at 4.2.
- Added a regression asserting exact text, closed carrier gate and WPM retained
  near 20 after six seconds of beating narrow-band noise.
- FT8, FT4, Gate "ghost candidate", waterfall, audio, CW TX and sequencer remain
  unchanged.

## 0.5.78 — CW carrier-gated relative timing fix — 2026-08-04

- Fixed the on-air failure where a valid 20 WPM clock later collapsed to 70 WPM
  and emitted long E/T-heavy garbage after the selected carrier disappeared.
- Separated current centred-carrier evidence from the Morse-space session hold;
  acquisition-only coherent evidence can no longer keep a dead lane active.
- Added per-run centred-sample fractions, strict PSD carrier qualification and
  automatic temporal-feed closure after carrier loss.
- Protected established dit/dah timing from micro-run storms, bounded timing
  adaptation, rejected implausible MARK fragments and gated character commits
  on carrier-backed evidence.
- Restored Auto-WPM to 5-50 WPM, suppressed unknown-pattern `?` output and
  prevented duplicate sub-hertz marker updates from resetting the RX.
- Added regressions derived from the supplied live log, including valid CW
  followed by beating narrow-band noise.
- FT8, FT4, Gate "ghost candidate", waterfall, audio, CW TX and sequencer are
  unchanged.

## 0.5.78 — CW carrier discriminator / relative timing clean restart — 2026-08-04

- Removed the previous `CwBayesianDecoder`, `CwCausalSemiMarkovDecoder` and
  `CwGeometricEdgeWorker` source branches, their experiment tree and obsolete
  decoder-specific audit scripts.
- Split each CW RX lane into an exact-tone `CwCarrierDiscriminator` and a real
  parallel `CwRelativeTimingTask` connected by timestamped MARK/SPACE runs.
- Added continuous short/long MARK-pair acquisition, robust histories and a
  geometric-mean dit/dah threshold; element, character and word spaces are
  learned independently.
- Restored immediate character-by-character publication instead of phrase-level
  end-of-transmission commits.
- Added bounded QSB split repair with one-element look-ahead, preserving normal
  one-dit gaps.
- Added a CW Runtime Log using the existing FT diagnostics dialog and a retained
  5000-line buffer.
- Added production-path regressions for wrong WPM hint, noise, QSB, human timing,
  a non-ideal 2.45 dash ratio, stronger +70 Hz neighbour and noise-only input.
- Passed GCC, Clang, ASan/UBSan and ThreadSanitizer checks.

## 0.5.78 — WSJT-X-style waterfall flattening

- Replaced the slow temporal waterfall floor AGC with the source-derived WSJT-X Wide Graph Flatten algorithm.
- Each row is converted to dB, split into ten frequency segments, and a fourth-order lower-envelope polynomial is fitted from the lowest ten percent of each segment.
- Subtracts the fitted baseline in the same row, so receiver/sound-card AGC steps no longer leave the full passband orange for many seconds.
- Changed the Colour scale transfer to the WSJT-X exponential waterfall-gain law, with the saved 80% setting as unity gain.
- Retained MadModem's persistent passband detector to exclude digitally silent monitor bins, plus the OpenGL circular texture, HiDPI and smooth-scroll fixes.
- FT8, FT4, CW, audio capture, scheduler, sequencer, CAT/PTT and protocol code are unchanged.

## 0.5.78 — FT8/FT4 sensitivity regression fix

- Restored the exact validated adaptive-runtime FT8 live candidate finder after the first wideband patch reduced real-air decodes.
- Removed the incorrect FT8 per-tone/per-Costas-row 40th-percentile normalization from the live path. WSJT-X applies its 40th-percentile normalization to the frequency-domain sync maxima after the Costas statistic, not independently to each tone energy before the statistic.
- Removed the active FT8 bucket-rescue tail from live decoding. The first log showed 57–77 rescued candidates per boundary and zero CRC-valid rescue decodes; reserving 36 candidate slots for that tail displaced validated candidates.
- Restricted FT8 sum-product, complex 200 Hz metrics and post-decode timing-search SIC to controlled offline A/B. In the first live log they produced zero recoveries while consuming boundary time.
- Restored the exact validated adaptive-runtime FT4 live candidate finder. The new spectral finder and coherent/SPA paths remain source-available for offline A/B only.
- Retained the source-derived FT4 CRC-codeword 103-tone continuous-GFSK SIC, but kept its additional timing-offset search offline; live SIC uses the candidate timing already validated by the decoder.
- Kept the FT8 `ldpcGhostCandidate` gate byte-identical (SHA-256 `6e7b4f9c15b21a01f3351442c08c916a7f57f49686a98dfc7acb2296adee1e36`).
- No audio, scheduler, sequencer, waterfall, CW, CAT/PTT or protocol-format logic changed.

## 0.5.78 — FT8/FT4 wideband sensitivity and exact FT4 SIC

- Rebuilt FT4 candidate generation around an averaged Nuttall spectrum, receiver-edge detection, robust polynomial baseline, normalized local peaks and parabolic frequency interpolation.
- Added FT4 complex 666.67 Hz / 32-sample-per-symbol coherent 1/2/4-symbol, ratio and cherry metric families on complete boundary/offline passes.
- Replaced FT4 observed-strongest-tone subtraction with CRC-codeword-derived 103-tone continuous-GFSK cancellation and bounded post-decode timing refinement.
- Added a robust 40th-percentile per-frequency baseline to the existing FT8 Costas FFT matrix.
- Preserved the primary FT8 bucket cap and added a bounded, tagged boundary/offline shadow-rescue tail with decoded/candidate telemetry.
- Added the FT8 boundary-only complex 200 Hz / 32-sample-per-symbol coherent 1/2/3-symbol, ratio and cherry metric path.
- Added a log-domain sum-product LDPC fallback on the same LLR vectors while retaining normalized min-sum as the first decoder.
- Refined FT8 cancellation timing using the regenerated CRC-valid waveform.
- Kept the FT8 `ldpcGhostCandidate` block byte-identical (SHA-256 `6e7b4f9c15b21a01f3351442c08c916a7f57f49686a98dfc7acb2296adee1e36`).
- Deferred deeper AP and higher-order OSD until same-WAV wideband sensitivity measurements are complete.

## 0.5.78 — FT4 adaptive resources and one-pass gate

- Applied the FT8 live scheduling policy to FT4: the 81% gate is one pass, while the complete boundary is limited to three decode-driven passes with a live time budget.
- Replaced FT4 static candidate chunks with atomic work stealing inside the persistent FT pool.
- Reset the topology-derived worker budget on every new FT capture/mode session, so FT4 cannot inherit a degraded FT8 one-worker target.
- Changed resource pressure input from the historical slot maximum to the current audio-queue sample and required sustained backlog before reducing workers.
- Prevented post-decode table insertion time from falsely ratcheting the decoder worker target down.
- Kept FT4 table/overlay updates in the common GUI batch and removed temporary per-row FT4 diagnostic logging.
- Normalized FT4 `.000`/`.500` slot diagnostics to the visible `HHmmss` table key.
- Preserved FT4 protocol, LDPC/CRC, message parsing, SNR, TX, sequencer, audio and waterfall DSP.

## 0.5.78 — OpenGL HiDPI overlay and smooth-scroll correction

- Kept the dynamic FT8 resource controller and circular OpenGL waterfall.
- Mixed raw OpenGL and QPainter through `beginNativePainting()` / `endNativePainting()` so Qt glyph-cache rendering is isolated from the waterfall texture state.
- Restored neutral texture and pixel-store state before drawing markers and decode callouts.
- Presented shallow two-row FFT bursts one row per frame, with adaptive catch-up only under real queue pressure.
- Reduced the repaint coalescing delay from 16 ms to 8 ms; Qt still coalesces to the compositor refresh rate.
- No decoder, audio, CW, sequencer, CAT/PTT, or logbook logic changed.

# MadModem changelog

## 0.5.78 — OpenGL waterfall HiDPI label fix

- Physical-pixel viewport follows `devicePixelRatioF()` on every frame.
- Removed preserved partial framebuffer to prevent stale/ghosted waterfall labels.
- Reset GL scissor/color-mask state and enabled QPainter text antialiasing.


## OpenGL waterfall HiDPI viewport hotfix

- Fixed the circular OpenGL waterfall occupying only the lower-left fraction of the widget on displays using fractional or 2x desktop scaling.
- The GL viewport now uses the physical backing-framebuffer size (`logical size × devicePixelRatioF()`), matching `QOpenGLWidget` and the full-size QPainter overlays.
- Preserved the circular texture, one-row uploads, frequency zoom, adaptive FT resource controller, decoder thresholds and native CW path.

## FT8 dynamic runtime and OpenGL waterfall

- First partial RX periods are now synchronization-only; boundary decoding counts only genuinely captured samples and never searches a slot made mostly from UTC padding.
- Added capture-generation, stale-block, non-monotonic timestamp and multi-slot jump protection. Obsolete in-flight work is cancelled when a new RX capture starts.
- Replaced the fixed eight-worker ceiling with a persistent adaptive pool sized from process-visible CPU topology and affinity, with separate gate, boundary and OSD budgets.
- Replaced per-slot `std::async` coordination with one persistent bounded coordinator.
- FT decode rows, logs, overlays and table scrolling are presented as one GUI batch.
- Downward waterfall scrolling now uses a circular OpenGL texture and uploads only new rows, with a QImage fallback.
- Added runtime telemetry for SIMD, CPU topology, worker targets, audio queue latency, GUI presentation and waterfall backend.
- Audited the internal FFT against the bundled MSHV and prior WSJT-X Improved source analysis; no FFT engine was changed without a same-machine measured advantage.

## FT8 live scheduler recovery hotfix

- The >1 s live-decode warning is diagnostic only and never disables later FT8 periods.
- Replaced future-vector polling as the authoritative busy state with an explicit worker-owned decode-job latch and completion callback.
- A complete boundary snapshot is no longer discarded when the previous gate/boundary job is still active. One bounded pending snapshot is retained and launched immediately after worker completion; on severe overload the newest slot replaces an older stale pending slot.
- Added log-visible diagnostics for busy gates, deferred boundaries, deferred-job launch and worker exceptions. These events were previously shown only in the small decoder status label.
- Added throttled diagnostics when AudioEngine is running but MainWindow is not forwarding FT blocks because RX/TX/offline state flags reject them.
- Preserved the efficient one-pass 90% gate, protocol thresholds, atomic sequencer, no-truncated-frame invariant, CW implementation and restored GitHub workflows.

## Shutdown and Settings cancellation hotfix

- Application shutdown no longer uses unbounded blocking queued calls or a separate 3+1 second wait for every worker thread. All worker stops now share one bounded deadline.
- FT8 candidate/OSD work and queued WAV recording abort promptly when shutdown starts.
- Final process exit detaches a potentially stalled Hamlib rotator backend instead of waiting for its serial/TCP timeout; normal user-requested Disconnect remains a clean close.
- Cancelling Settings no longer performs a full runtime reapply or reconnects CAT/rotator backends when no setting was accepted. CAT/PTT test state is restored asynchronously only when a test was actually used.
- Rotator calibration launched from Settings no longer overwrites or saves the application's settings before the user presses OK.

## 0.5.78 — efficient FT8 live decode and atomic sequencer — 2026-07-31

- Converted the FT8 90% live gate into one bounded latency pass: no second/third
  rescue pass, no GF(2) OSD and no heavy multi-metric retry before the complete
  boundary snapshot is available.
- Suppressed gate jobs after a late RX restart unless the slot contains enough
  genuinely captured (non-UTC-padding) audio for one complete protocol frame.
- Strengthened two-dimensional DT/DF non-maximum suppression so adjacent grid
  replicas of one Costas peak are not all sent to LDPC.
- Restored the MSHV/WSJT-X decode-driven pass rule: if no CRC-valid signal was
  found, later subtraction/rescan passes stop immediately.
- Added decode-batch start/end signalling. Decode rows remain visible
  immediately, while the QSO sequencer evaluates the complete batch and commits
  one best state transition instead of arming repeatedly while rows arrive.
- Auto-QSO CQ ranking is flushed at batch completion instead of waiting an
  additional 420 ms close to the UTC boundary.
- Prohibited truncated FT4/FT8 transmission. If the audio deadline is missed,
  MadModem releases PTT, returns to RX and defers the unchanged message to the
  next selected slot instead of skipping the beginning of the waveform.
- Preserved the current CW receiver, waterfall, audio capture, FT protocol
  demodulation/LDPC thresholds and offline reference-decoder path.
