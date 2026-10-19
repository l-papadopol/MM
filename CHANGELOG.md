# MadModem changelog

## 0.6.3 source R5 — FT replies and selected QRZ station

- Preserve the final opposite RX period before declaring an FT8/FT4 QSO unanswered; consume each TX attempt once and invalidate obsolete timeouts.
- Publish validated active-QSO results between existing passes and during FT8 residual recovery while retaining remaining decoder work, thresholds, candidates and recovery budgets.
- Apply coalesced FT DX/marker updates after the running job without blocking the capture-owner thread.
- Remove repeated bounds/COW work from FT8 cancellation loops with sample-for-sample parity checks.
- Refresh QRZ explicitly when the FT correspondent changes under blocked widget signals.
- Preserve R4 updater support, R3 Windows/Hamlib fixes, Native R12, Neural R13 and the pinned clock.

## 0.6.3 source R4 — updates from promoted builds

- Accept automatic `build-VERSION-N` tags after a release is promoted to stable and Latest, alongside numeric version tags.
- Keep draft/prerelease exclusion, numeric version comparison, package selection and verified downloads. Release titles remain display text.
- Retain the R3 Windows/Hamlib fixes, Native R12, Neural R13 and the FT clock layout.

## 0.6.3 source R3 — bundled Hamlib Windows build

- Added the missing assert header in Hamlib's Windows timer and used GCC's constructor attribute for startup initialization; retained the MSVC CRT path.
- Normalized the Autoconf header/function lists and separated two concatenated BSD header names, eliminating malformed feature probes.
- Kept UCRT64 packaging, both CW engines and the FT clock layout unchanged.

## 0.6.3 source R2 — Windows UCRT64 packaging

- Migrated the installer workflow and its complete package list from deprecated MINGW64 to UCRT64.
- Aligned build/deploy paths, isolated Hamlib build/install prefixes by environment and removed cross-environment DLL fallback.
- Kept public version 0.6.3, both CPU variants, installer/portable asset names, Native R12, Neural R13 and the FT clock fix.

## 0.6.3 — Native R12 and Neural R13

- Two CW RX A/B choices only: Native R12 (default) and Neural R13 (experimental). Native algorithms, R13 numerical weights and R13 CTC search parameters are unchanged.
- Removed the older neural runtime paths and their bundled weights. Old saved CTC/Envelope selections migrate to Neural R13; unknown settings fall back to Native R12.
- Pinned the FT clock outside the mode settings scroll area, above QRZ. Removed the nested FT sequencer summary and duplicated period/next-message labels above the clock. Sequencer details remain in the clock tooltip; the live TX banner and message selection continue to update.
- Updated public version, Windows resources, localized help and UI strings to 0.6.3. Removed obsolete training checkpoints, caches and generated build products from this source delivery.
- Validation details: `docs/architecture/RELEASE_0_6_3.md`. Historical R12/R13 benchmark reports describe those earlier source packages.


## 0.6.2 source R13 — adapted neural CW with causal CTC search

- Added optional MM Neural Search for RX A/B, with a separately adapted MM checkpoint, causal envelope frontend and four-hypothesis acoustic CTC search. Native stays default; legacy CTC and Envelope remain available.
- New QSO errors: Envelope 96/1,017 → Search 21/1,017. New clean ARRL clips: 152/592 → 28/592. Existing RF development: 127/249 → 92/249. Native remains ahead on clean QSO/ARRL.
- Vectorized the unchanged-weight inference core with bit-identical output; full-app CPU timings remain variable and do not establish a general speed win over Native.
- Added source-backed research, 118 minutes of independently pulse-audited archive material acquisition, training/replay tools, comparative results and causal streaming checks.
- Full Linux build, Native regressions, Search A/B routing and four-choice UI checks passed. Details: `docs/architecture/CW_NEURAL_R13_RESULTS.md`.

## 0.6.2 source R12 — native CW acquisition and processing cost

- Fixed startup loss of the first Morse element after a short noise prelude; added adaptive character/word spacing from confident completed gaps.
- Cached exact duration hazards and spectral work. Native CPU cost fell 18.9% in the documented local comparison.
- Added optional MM Neural Envelope RX normalization while preserving legacy Neural CTC and the Native default. No new weights were promoted.
- Fresh synthetic QSO: native errors 13/489 → 0/489. Existing RF development excerpts: 143/249 → 134/249. These scopes are distinct; independent new radio validation remains open.
- Added actual-route comparison tooling and regressions. Details and limits: `docs/architecture/CW_R12_CHECKS.md`.

## 0.6.2 source R6 — CW neural receiver refinement

- Expanded independent training with receiver-filtered messages, narrowband noise,
  steady carriers and more word boundaries. Increased blank and CTC supervision.
- Kept Native as the default and retained the saved RX A/B engine selector.
- Added reproducible refinement and frozen-model comparison tools, with the R5
  training checkpoint retained for lineage and baseline comparisons.
- WebSDR captures are evaluated separately from labelled synthetic tests; no
  unverified decoder predictions are used as training truth.
- Measurements and remaining limitations: `docs/architecture/CW_NEURAL_R6.md`.

## 0.6.2 source R5 — selectable CW receive engine

- Added persistent RX A/B engine selection: Native (default) or independently
  trained MM Neural CTC (experimental). Band skimmer remains native.
- Added a causal temporal-CNN/CTC C++ receiver with bundled original weights and a
  reproducible synthetic-audio training/export tool; no external ML runtime.
- Reset neural state on engine/tone changes, receiver clear, capture gaps and
  local TX resume. TX audio and serial KEY behavior is independent.
- Experimental model trained for 12–36 WPM. Native filter/AFC/WPM controls are
  disabled for this engine. Real-radio and native Windows/macOS validation remain open.
- See `tools/cw_neural/README.md`, `modems/cw/neural/model/MODEL_CARD.json`,
  and `docs/architecture/CW_NEURAL_R5.md` for origin, measurements and limitations.


0.6.2 R4: native CW serial KEY with optional same-port PTT, exclusive CAT port handoff, interruptible timing/Stop and failed-release recovery; shared Morse plan and consistent 80-WPM TX range.

0.6.2 R3: QRZ selected-station panel; clarified local callsign recognition; resizable waterfall with rounded frequency ticks; scrollable mode controls and explicit audio-CW TX explanation.

0.6.2 R2: isolated/adaptive CW band worker; GitHub stable-release updater with verified downloads and portable backup/rollback; IK7IMK Nick tester credit; corrected main installer license footer.

## 0.6.2

RTTY band skimmer with independent receivers, Text/CQ waterfall views and bounded worker processing. Shared CW/RTTY overlay and CQ recognition; persistent pair acquisition suppresses sideband duplicates. Full multilingual help refresh, source cleanup and aligned release metadata. See [release notes](RELEASE_NOTES.md) for operating details.

## 0.6.1

Optional installers alongside portable archives; managed user-data storage and migration; Windows CPU selection; bundled Linux Qt/Hamlib runtime; macOS package/deployment corrections; release asset publication. Resizable windows, clearer FT/CQ/logbook controls, editable rotator memories and standard serial speeds. CW band skimmer and playback-based CW text highlighting were introduced in subsequent source updates.

## 0.6.0

CW acquisition/filter/AFC work; RTTY letters/figures, AFC and contest macros; FT scheduling and TX audio recovery; asynchronous CAT/PTT; RX worker separation and audio-gap recovery. Logbook parsing and contest edition export with QSO preview; narrow-band waterfall and CAT configuration improvements.

Historical build logs, screenshots and per-revision audit reports are not distributed with source releases. Version control preserves earlier implementations. Licenses, attribution and maintained tests remain part of the source.
