## 0.5.25 - fldigi-aligned text modem core pass

MadModem 0.5.25 applies a single consolidated text-mode DSP pass using fldigi as the GPL reference. PSK31/63 no longer averages a whole symbol across phase reversals and now samples at the recovered symbol eye centre with a stronger envelope-dip clock resync and corrected Varicode quality/inversion feedback. MFSK16 soft decisions now follow fldigi's softdecode more closely: full-scale soft bits, hard-symbol vote weighting and persistent single-tone CWI avoidance feed the existing R=1/2 K=7 FEC/Varicode path. Feld Hell keeps the corrected bottom-to-top visual raster orientation and compact paper scaling. CW keeps the heavy MIND human-fist path while preserving the fldigi/ggmorse-style timing, adaptive WPM, gap tracking and native event decoder.

## 0.5.24 - CW heavy MIND human-fist recovery

MadModem 0.5.24 makes CW MIND Active materially affect CW copy. In Active mode ggmorse remains available for speed/cost estimation, but its text stream is suppressed and the MIND-biased native Morse event decoder becomes the output path. MIND can now steer dit/dah, intra-letter, letter-gap and word-gap decisions with lower confidence thresholds designed for human straight-key timing, while Training remains non-invasive and Off remains hard-bypass. CW profile training is allowed to run in very small low-priority slices during live CW so the event profile becomes usable while the user is actually copying.

## 0.5.23 - Hell orientation and visible CW/RTTY MIND assist

MadModem 0.5.23 keeps the 0.5.22 standard MFSK16 work and fixes the Feld Hell visual paper orientation: received Hell glyphs are no longer drawn upside-down. It also makes CW/RTTY MIND behaviour explicit and useful: CW/RTTY profile training now catches up during safe idle gaps instead of waiting only for heavy settings/logbook idle, Active shows ready/training state, and low-level CW/RTTY assist becomes available once the small profile validation gate is reached. Training mode remains non-invasive and Off remains a hard bypass.

## 0.5.22 - Standard MFSK16 Varicode/FEC

MadModem 0.5.22 keeps the validated 0.5.19/0.5.21 baseline and replaces the old internal/framed MFSK16 receiver with a standard MFSK16 text pipeline: 16 tones at 15.625 baud, Gray tone weighting, IZ8BLY/gMFSK-style Varicode, R=1/2 K=7 convolutional FEC and the 10-stage 4x4 diagonal deinterleaver. MFSK16 TX now uses the same standard chain. MFSK32 remains marked legacy/experimental until a matching standard path is added.

## 0.5.21 - Text-mode RX cleanup

MadModem 0.5.21 returns to the validated 0.5.19 baseline and focuses on text-mode receive cleanup. BPSK31/BPSK63 RX adds conservative symbol-clock reacquisition from PSK envelope dips and early auto-polarity recovery for inverted Varicode streams. Feld Hell now uses the standard 14-row raster timing with a half-height paper view and a more compact waterfall layout. RTTY remains unchanged because the supplied 170 Hz / 45.45 baud test is already decoding. The MFSK16 receiver is still marked experimental: it is a framed internal receiver, not yet a fldigi-compatible MFSK16 Varicode/FEC decoder.

## 0.5.19 - CW dual RX usability / filter markers

- Colour/tag CW terminal output with A>/B> prefixes for green and blue RX markers.
- Add a Mode-panel button to disable RX B and remove the secondary blue marker.
- Show CW passband cutoff lines as yellow dashed markers around selected tones.
- Report A/B tone and tracked WPM in the CW Mode panel; RX B keeps independent AFC/Auto-WPM tracking while sharing bandwidth/WPM settings.


## MadModem 0.5.18

- Fix PSK/BPSK31 receive lock acquisition: the decoder no longer learns a clean PSK carrier as noise before opening squelch.
- Varicode validity is now used as an additional PSK lock validator before text is emitted, reducing blank RX terminals on valid BPSK31 signals.
- Keep Windows dual CPU builds from 0.5.11 and all 0.5.17 Settings/User-QTH fixes.

## 0.5.17 - User/QTH settings page rebuild hotfix

- Rebuilt the User / QTH / Macros Settings tab as a native QWidget page instead of embedding the old TextMacroSettingsDialog.
- Fixes the blank User / QTH / Macros tab regression seen after 0.5.11 on some Qt platforms.
- Preserves synchronization of User/QTH fields with FT callsign/grid settings and keeps macro editing in the unified Settings dialog.

## MadModem 0.5.14 - CW dual RX and clearer MIND panel

## 0.5.16 - Settings embedded pages hotfix

- Fixed the unified Settings dialog embedding path: child QDialog pages are now re-parented and explicitly shown after changing them to Qt::Widget. This prevents the User / QTH / Macros page from opening as a blank tab on Windows/Linux builds.
- Keeps the 0.5.15 fix that ignores stale serial PTT COM-port values when PTT is set to CAT/Hamlib, so rotator endpoint conflict detection only reserves the PTT port for Serial RTS/DTR.


- Fixed a false rotator endpoint conflict when the old serial PTT combo still contained a COM port but the active PTT method was CAT/Hamlib.
- Rotator endpoints are now compared with the PTT serial port only when PTT is actually Serial RTS or Serial DTR.
- The live rotator warning now uses the current Settings dialog state, so switching PTT mode inside the dialog clears stale warnings immediately.


- Added real CW dual-RX operation: left-click waterfall sets green RX A, right-click sets/enables blue RX B.
- RX A and RX B run separate CW decoders on the same conditioned audio with shared WPM/bandwidth/AFC settings.
- RX B output is tagged as `B>` in the CW terminal; markers are shown separately on the waterfall.
- Simplified the MIND panel wording: Off/Training/Active are explained in user language and profile-specific text explains FT, CW and RTTY roles.
- Kept MIND Off hard-bypass semantics and the dual Windows CPU build outputs from 0.5.11.

## MadModem 0.5.12 - Logbook statistics PDF and display settings

- Removed the "strike through worked calls" checkbox from the Logbook toolbar. The option now lives in Settings -> Logbook / FT colours -> Logbook display.
- Added a Logbook action to generate a printable statistics PDF from selected QSOs, the current search result, or the full ADIF logbook.
- Fixed FT decode row highlighting so the heavy red dashed outline is no longer applied to every valid not-yet-worked callsign; it is reserved for high-priority new-DXCC style highlights.
- The statistics PDF includes QSO totals, unique calls, unique DXCC/countries, unique grids, first/last QSO, distribution by mode, distribution by band, top countries, top grids, and time summaries.
- Kept the dual Windows CPU build workflow from 0.5.11: MadModem.exe for AVX2/FMA CPUs and MadModem-Legacy.exe for older systems such as Xeon X5680.

## MadModem 0.5.11 - Dual Windows CPU builds

- Updates `build_all.sh` so Windows/MXE builds produce two executables by default.
- `MadModem.exe` is now the AVX2/FMA optimized Windows executable for modern CPUs.
- `MadModem-Legacy.exe` is the portable Windows executable for older CPUs without AVX/AVX2, including Xeon X5680-era systems.
- Updates `tools/package_mm_zip.sh` so the final `mm.zip` can include both Windows executables.
- Keeps Linux build behavior unchanged and preserves the 0.5.10 CPU portability fix for the legacy executable.

## MadModem 0.5.10 - Windows portable CPU hotfix

- Fixes a Windows 10 startup crash traced from a ProcDump minidump to exception `0xC000001D` (illegal instruction) inside `MadModem.exe`.
- Disables global AVX2/FMA/SSE4.2 compiler flags by default; generic Windows builds now keep a portable x86-64 baseline.
- Keeps runtime AVX2/FMA FT kernels guarded by CPU feature detection, instead of making the whole executable AVX2-only.
- Leaves the 0.5.9 CW Active Event Assist and MIND Training/Active behavior unchanged.
- Developer-only AVX2 builds remain possible with `-DMADMODEM_AVX2_BUILD=ON`, but must not be used for general Windows packages.


## MadModem 0.5.9 - CW Active Event Assist

- Integrates CW Human Fist Recovery with MIND Training/Active through real event samples.
- The classical CW detector remains the authoritative text decoder; MIND learns key-down timing events, not final text.
- Emits live CW event samples for dit, dah, intra-character gap, letter gap and word gap using a 256-point robust normalized tone-envelope feature.
- Adds DeepDspController::submitCwEventSample() and routes CwDecoder event samples into the separated CW profile queue.
- MIND Off remains hard-bypassed: no CW event feature queueing or training is performed when Off is selected.

## 0.5.9 — CW Active Event Assist

- Improves CW copy for human straight-key/keyer timing by adding robust moving-window dot tracking.
- Adds adaptive character/word gap thresholds to reduce both split callsigns and glued radio tokens.
- Enables ggmorse internal high/low-pass filtering around the selected CW note.
- Uses asymmetric envelope attack/release for sharper dits with smoother QSB resilience.
- Adds conservative CW text normalization for common glued/split radio tokens such as `DEIW6DRH`, `CQIZ6NNH`, and `IZ6NN H`.

## 0.5.6 — MIND UI terminology and layout cleanup

- Renamed visible MIND modes from `Shadow` to `Training` and from `Assisted` to `Active`; internal persisted values remain backward-compatible.
- Reorganized the MIND side-panel status block into wrapped single-column rows to avoid clipped labels in narrow layouts.
- Localized the new MIND strings and the compact status labels for English, Italian, French, German, Norwegian and Czech.
- AutoTest reports now display `training`/`active` terminology while keeping the same Off hard-bypass and Active ultra-deep behavior.

## 0.5.6 — MIND Multi-Mode Assist Foundation

- Keeps the structural MIND Off hard bypass from 0.5.2: Off touches no FT candidate feature extraction, callback, queue, lock or training path.
- Removes the AutoTest-specific MIND freeze: benchmarks now use the same native candidate-driven deferral used during live FT operation.
- Every FT training sample/candidate burst resets a short neural-work cooldown, so heavy background training starts only after the decoder has stopped pushing candidates.
- AutoTest reports explicitly show the native deferral policy.

- Added a hard bypass for MIND Assist Off: the FT decoder no longer builds ranker feature vectors, emits native MIND training samples, invokes callbacks or touches MIND locks when MIND is Off.
- Decoder integration now distinguishes sample export from scoring: model-missing Training/Active can collect data, but cannot score/prune; Active scores only when the model is loaded and ready.
- MIND training remains suspended during FT AutoTest/decode-critical windows and now uses a short cooldown before resuming.
- AutoTest report adds explicit MIND status such as `bypassed` or `model missing (data collection)`.
- The target is to restore pure native 0.5.0/0.5.x FT timing whenever MIND Assist is Off.

## 0.5.1-mind-autonomous-trainer

## 0.5.1 — MIND Ranker v1

- Version bump from 0.5.0 to 0.5.1 for the MIND candidate-ranker development line.
- 0.5.0 remains the pre-ranker FT decoder baseline; 0.5.1 adds the new probabilistic FT candidate ranker/pruner architecture.


- Removed user-facing MIND trainer controls: no Training checkbox, no Trainer budget spinbox, no Save/Load/Reset model buttons.
- Converted MIND training to an autonomous low-priority background service with adaptive idle budget.
- Training now backs off to zero during FT timing-critical decode sections and active CW receive/operate periods, uses tiny slices during realtime receive, and may use larger slices while the user is in logbook/settings/runtime-log idle workflows.
- Kept `MIND Assist: Off / Training / Active` visible; final FT text remains accepted only through LDPC/CRC/unpack/parser validation.
- Updated `mind_stats.json` to version 9 with adaptive trainer budget diagnostics.
- FT8/FT4 decoder core, TX, CAT/PTT, scheduler and AutoQSO behavior are unchanged.


## 0.5.1 MIND Eigen/OpenMP batch trainer

- Quarantined the external trainer process experiment and kept the single-process MIND trainer path.
- Added required OpenMP support for the optimized MIND build.
- Enabled Eigen parallel initialization and automatic CPU-thread selection.
- Added AVX2/FMA/SSE4.2 release flags for the optimized x86_64 baseline.
- Reworked FT native MIND training from repeated Matrix×Vector sample loops to true batched Matrix×Matrix training.
- Added MIND stats for Eigen threads, batch size and training throughput.


## 0.5.1-mind-cw-bootcamp-compilefix

- Fixed GCC/Qt build failure in `ai/DeepDspProfileNet.cpp` by explicitly serializing Eigen matrix/vector dimensions as `qint32` before passing them to `QDataStream`.
- No decoder, TX, CAT/PTT, scheduler or AutoQSO behavior changed.

## MadModem 0.5.1-i18n-title-fix

- Rolled back from the MSK144 experimental branch to the stable i18n-full-review source line.
- Fixed the main window title so `MadModem 0.5.1` is treated as a non-translatable product/version identifier.
- Removed erroneous localized dictionary keys for `MadModem 0.5.1` and hardened the translation harvester so they are not regenerated.

# Changelog

## 0.5.1-mind-assist-cw-ftneeded

- Added `MIND Assist: Off / Training / Active` in the MIND panel and persisted the selected mode in `mind_stats.json`.
- Kept FT message acceptance fail-closed: MIND mode selection does not bypass CRC, unpack or standard-message parser validation.
- Changed the FT decode dashed red outline from CQ-only/new-country-only to “needed station” logic for any valid decoded callsign not already present in the ADIF logbook.
- Preserved CQ colouring while allowing non-CQ ongoing-QSO lines to be marked as needed so the operator can call the station after 73.
- Removed the generic DSP tab and moved `Software AGC` into the CW Mode settings under AFC.
- Updated runtime translations for the new MIND Assist and CW Software AGC strings.
- FT8/FT4 decoder core, TX, scheduler, CAT/PTT and AutoQSO logic are unchanged.

## MadModem 0.5.1-i18n-full-review

- Added a shared `MadModemI18n::RuntimeI18n` bridge so runtime-created dialogs/widgets can use the same INI dictionaries as the main UI.
- Reworked dynamic UI text in AutoQSO Flow Editor, rotator side panel, QSO map widgets, help dialog, SSTV editor and text macro/user-QTH dialog.
- Extended the translation harvester/audit to include `MadModemI18n::text`, `MadModemI18n::placeholder`, `L18n`, `P18n`, `tr` and `QObject::tr` call sites.
- Refreshed English, Italian, French, German, Norwegian and Czech dictionaries to 1580 aligned runtime keys with zero non-technical English fallbacks in the static audit.
- Decoder, FT scheduler, AutoQSO radio logic, audio DSP, CAT/PTT and TX paths are unchanged from the production baseline plus ADIF time-filter patch.

## MadModem 0.5.1-adif-time-filter

- Added ADIF export options for precise UTC time intervals based on `QSO_DATE` + `TIME_ON`.
- Interval matching uses `start <= TIME_ON < end` to avoid duplicate QSOs when exporting consecutive activity windows.
- Updated multilingual UI strings and online help for the new logbook export filter.
- Decoder, FT scheduler, AutoQSO and radio/audio paths are unchanged from the production baseline.

## MadModem 0.5.1 — production consolidation

- Consolidated the validated `0.5.1-alpha.26` FT8 GF(2) OSD full order-1 decoder as the production baseline.
- Expected FT8 Auto Test total remains 88 decodes on the bundled WAV set: 26 / 25 / 16 / 21.
- Rejected later FT8 beta lab experiments (`beta02`..`beta08`) from the production baseline because they did not improve the validated decode count.
- Cleaned production documentation: removed scattered historical v2.x one-off notes and replaced them with consolidated release/architecture notes.
- Included `compare_ft8_wsjtx_madmodem.sh` in the full source tree for optional WSJT-X/jt9 comparison.
- Preserved executable permissions for shell scripts.

## Historical baseline

The decoder core in this release comes from the last validated improvement line:

- `0.5.1-alpha.26_ft_osd_gf2_order1_full`
- GF(2) OSD fallback: order-1 complete over 91 information bits.
- Order-2 search disabled.
- Later micro-sweep/reinject/coherent-metric beta experiments were laboratory-only.

## 0.5.1-ddsp-shadow-learning-lab

- Added the experimental MIND side tab.
- Integrated bundled tiny-dnn header-only sources under `third_party/tiny_dnn/`.
- Added `DeepDspController`, a fail-closed shadow-learning backend that trains only from messages already accepted by the existing classic decoders.
- MIND observes FT8/FT4, RTTY and CW receive audio and learns in the background from confirmed decode text.
- MIND Assist remains disabled until the model reaches a perfect local validation window; it does not currently inject extra decodes, key PTT, affect AutoQSO, change CAT, or replace the stable decoders.

## 0.5.1-madnness-shadow-learning-lab-fix1

- Fixed Linux/GCC build failure in `ai/DeepDspController.cpp` caused by ambiguous `tanh()` resolution between libm and neural activation layers.
- Made old neural activations explicitly namespace-qualified.
- Added MIND manual label teaching controls for CW/RTTY so an operator can enter corrected text checked by ear or by known QSO patterns.
- Disabled unsafe automatic CW character-level MIND labels; CW has no CRC/parity, so raw decoder characters are not treated as truth labels.
- Updated MIND runtime translations and translation harvesting for `T(QStringLiteral(...))` strings.


MIND Eigen update:
- Replaced active TinyDNN dependency with MadModem internal MIND Eigen MLP.
- Added MIND training-completion/decode-success percentage.
- Added live neural matrix activity widget.
- MIND remains shadow/fail-closed; no decoder, TX, CAT or AutoQSO assist is enabled by default.


MIND Eigen rename/update:
- Renamed the user-facing DDSP tab/labels to MIND.
- Replaced the manual neural inner loops with Eigen-backed matrix operations inside the internal MIND engine.
- Bundled Eigen under `third_party/eigen/` as header-only source.
- Checkpoint path now uses `mind_native_ft_eigen_v2.model`.

## 0.5.1-madnness-persistent-eigen-profile
- Standardized the experimental neural lab name to MIND across the UI, logs, documentation and translations.
- Moved the active Eigen MLP profile from the generic 68→96→64→32 laboratory shape to the FT-family shadow profile 464→128→64→77.
- Added persistent MIND checkpoint storage under the writable MadModem application data directory, with atomic model writes and a companion `mind_stats.json` statistics file.
- The MIND panel now shows architecture, validation success, readiness percentage, checkpoint path, last checkpoint time and stats path.
- MIND remains fail-closed: no assisted decodes, AutoQSO, TX, CAT or PTT path is enabled by this lab package.

## 0.5.1-madnness-offline-prime-fix
- Fixed MIND shadow learning during offline FT WAV analysis and bundled Auto Test: the selected WAV is now used to prime MIND audio features before CRC-valid FT8/FT4 decode labels arrive.
- The MIND matrix activity and sample counters now update after offline WAV decodes instead of staying at zero.
- The visible learned-sample counter now survives checkpoint/stat reload by using the persisted FT/RTTY/CW sample counters when the in-memory training queue is empty.
- MIND remains shadow/fail-closed and does not add decodes, drive AutoQSO, key TX, or touch CAT/PTT.

## 0.5.1-mind-compact-panel
- Shortened the experimental neural side tab to the user-facing name MIND.
- Reorganized the neural side tab into compact Status, Brain activity, Control, Model and Teach sections.
- Removed long checkpoint paths from visible labels; full paths are now available as tooltips.
- Kept the neural lab fail-closed: no assisted decode injection, AutoQSO, TX, CAT or PTT control.

## 0.5.1-mind-perf-guard
- Isolated MIND from FT decoder timing: FT WAV analysis and FT Auto test now suspend neural training/checkpoint writes while the decoder is running; labels are queued and trained later during idle slices.
- Reduced default MIND idle training budget to 2 ms and capped it to 25 ms; the user may set 0 ms to collect labels without training.
- Throttled MIND UI/status updates and changed auto-checkpointing from frequent 15-second saves to a conservative 5-minute cadence.
- Offline WAV priming now uses a lightweight fingerprint instead of feeding the full WAV into the MIND feature extractor.

## 0.5.1 MIND CPU-only guard

- Confirmed MIND as a CPU-only Eigen shadow-learning backend.
- GPU/OpenCL acceleration is intentionally not included: MIND training remains deferred and low-budget on CPU so decoder timing stays predictable.
- Added visible MIND backend status in the side panel: `Backend: CPU Eigen (CPU only)`.
- Kept MIND fail-closed: no assisted decodes, no AutoQSO control, no TX/PTT/CAT interaction.
## 0.5.1-mind-ui-warmup-fix
- Polished the MIND side panel after field testing: group-box title spacing was increased to prevent clipped/overlapping labels on Qt themes with larger title metrics.
- The progress bar now shows a warm-up state while confirmed labels are being collected, instead of presenting early validation as a misleading 0% ready state.
- MIND remains CPU-only, deferred, fail-closed and outside timing-critical FT decode sections.

### MIND v2 native FT candidate training
- Replaced the old FT audio/text fingerprint lab path with native FT8 candidate samples.
- MIND now learns from `dataMagnitudes[58][8]` flattened to 464 inputs and the CRC-valid 174-bit LDPC codeword as target.
- The checkpoint is now `mind_native_ft_eigen_v2.model`; old v1 fingerprint checkpoints are intentionally ignored.
- UI now separates bit accuracy, message/codeword exact accuracy and readiness.
- No MIND inference/training runs inside the FT decoder timing-critical path; the decoder only emits queued gold-label samples after LDPC+CRC+unpack succeeds.
- MIND remains shadow-only and cannot key TX, CAT, PTT or AutoQSO.

- MIND v2 native FT follow-up: stale v1 stats files are now moved aside, v2 stats are saved soon after native FT gold samples arrive, and warm-up progress is based on validation samples rather than raw sample count.

## 0.5.1 MIND multi-profile visual panel
- MIND panel now exposes a Profile view selector: Auto, FT8, FT4, CW, RTTY.
- Brain activity visualization now changes layer geometry according to the selected profile:
  - FT8/FT4: 464 → 128 → 64 → 174
  - CW: 256 → 96 → 48 → 6
  - RTTY: 96 → 64 → 32 → 8
- Added split FT8/FT4 sample counters in MIND status/statistics.
- Auto profile view follows the currently observed mode/profile; manual view can inspect each planned dedicated network.
- No decoder, TX, CAT, PTT, scheduler or AutoQSO behavior changed.

## 0.5.1 MIND CW bootcamp
- Added a dedicated MIND CW synthetic bootcamp path for the CW profile.
- Added a dynamic Eigen profile network for CW: 256 -> 96 -> 48 -> 6.
- Added a Run CW bootcamp control in the MIND panel.
- Kept FT8/FT4 decoder, TX, CAT/PTT and AutoQSO untouched.

### MIND trainer thread / FT gold replay buffer
- MIND training now runs from a dedicated low-priority QThread instead of sharing the UI/decoder path.
- CRC-valid native FT samples are persisted to `mind_ft_gold_samples_v1.dat` and reloaded as the FT gold replay buffer on startup.
- MIND statistics now expose `Validation`, `Ready`, `Bit`, `Best Bit`, replay-buffer size and dataset path more clearly.
- No TX, CAT/PTT, scheduler, AutoQSO, FT decoder or MSK144 logic is changed.

### MIND UI cleanup
- Removed redundant section titles from the MIND side tab to save vertical space and reduce visual noise.
- Removed the separate Ready/Pronto label; the main progress bar now uses the guarded MIND progress score directly.
- Removed the ambiguous visible Validation progress wording; the panel now focuses on Bit, Best Bit, Replay and Loss.
- Removed the bottom safety note label while keeping MIND fail-closed in code: no TX, CAT/PTT, scheduler, AutoQSO or decoder behavior changed.


## MIND dedicated full-speed trainer
- MIND FT/CW training now runs on a dedicated normal-priority trainer QThread with a 20 ms service interval.
- Trainer budget range increased to 0..250 ms; default set to 50 ms so offline training can use meaningful CPU instead of idling around a few percent.
- Replay buffer and gold dataset persistence remain unchanged; decoder/audio/CAT/PTT/AutoQSO are untouched.
- `mind_stats.json` version bumped to 6 and records `trainer_thread=dedicated_fullspeed_qthread` plus `trainer_budget_ms`.

## 2026-06-19 - MIND OpenMP UI no-CW-teach cleanup
- Removed the visible CW/RTTY manual teaching and CW bootcamp controls from the MIND production panel.
- Reduced MIND status repaint frequency while full-speed OpenMP batch training is active to limit visible flicker.
- Kept FT native gold-sample collection, replay buffer, Eigen/OpenMP batch training and model/stat persistence unchanged.

## 0.5.1 MIND autonomous latency/status fix
- Fixed MIND status showing CW protection while the selected runtime mode is FT8/FT4.
- Restored persisted bit/validation readiness on startup so A6 models do not appear as Bit 0 / Best 100 after reload.
- Added explicit model/dataset state in the MIND panel and clearer matrix idle text.
- Suppressed trainer-thread status repaint churn during FT AutoTest/decode-critical windows.
- Updated realtime activity tracking so autonomous training backs off correctly during live RX, especially CW.
