
## 0.5.0 MIND Eigen/OpenMP batch trainer

- Quarantined the external trainer process experiment and kept the single-process MIND trainer path.
- Added required OpenMP support for the optimized MIND build.
- Enabled Eigen parallel initialization and automatic CPU-thread selection.
- Added AVX2/FMA/SSE4.2 release flags for the optimized x86_64 baseline.
- Reworked FT native MIND training from repeated Matrix×Vector sample loops to true batched Matrix×Matrix training.
- Added MIND stats for Eigen threads, batch size and training throughput.


## 0.5.0-mind-cw-bootcamp-compilefix

- Fixed GCC/Qt build failure in `ai/DeepDspProfileNet.cpp` by explicitly serializing Eigen matrix/vector dimensions as `qint32` before passing them to `QDataStream`.
- No decoder, TX, CAT/PTT, scheduler or AutoQSO behavior changed.

## MadModem 0.5.0-i18n-title-fix

- Rolled back from the MSK144 experimental branch to the stable i18n-full-review source line.
- Fixed the main window title so `MadModem 0.5.0` is treated as a non-translatable product/version identifier.
- Removed erroneous localized dictionary keys for `MadModem 0.5.0` and hardened the translation harvester so they are not regenerated.

# Changelog

## MadModem 0.5.0-i18n-full-review

- Added a shared `MadModemI18n::RuntimeI18n` bridge so runtime-created dialogs/widgets can use the same INI dictionaries as the main UI.
- Reworked dynamic UI text in AutoQSO Flow Editor, rotator side panel, QSO map widgets, help dialog, SSTV editor and text macro/user-QTH dialog.
- Extended the translation harvester/audit to include `MadModemI18n::text`, `MadModemI18n::placeholder`, `L18n`, `P18n`, `tr` and `QObject::tr` call sites.
- Refreshed English, Italian, French, German, Norwegian and Czech dictionaries to 1580 aligned runtime keys with zero non-technical English fallbacks in the static audit.
- Decoder, FT scheduler, AutoQSO radio logic, audio DSP, CAT/PTT and TX paths are unchanged from the production baseline plus ADIF time-filter patch.

## MadModem 0.5.0-adif-time-filter

- Added ADIF export options for precise UTC time intervals based on `QSO_DATE` + `TIME_ON`.
- Interval matching uses `start <= TIME_ON < end` to avoid duplicate QSOs when exporting consecutive activity windows.
- Updated multilingual UI strings and online help for the new logbook export filter.
- Decoder, FT scheduler, AutoQSO and radio/audio paths are unchanged from the production baseline.

## MadModem 0.5.0 — production consolidation

- Consolidated the validated `0.5.0-alpha.26` FT8 GF(2) OSD full order-1 decoder as the production baseline.
- Expected FT8 Auto Test total remains 88 decodes on the bundled WAV set: 26 / 25 / 16 / 21.
- Rejected later FT8 beta lab experiments (`beta02`..`beta08`) from the production baseline because they did not improve the validated decode count.
- Cleaned production documentation: removed scattered historical v2.x one-off notes and replaced them with consolidated release/architecture notes.
- Included `compare_ft8_wsjtx_madmodem.sh` in the full source tree for optional WSJT-X/jt9 comparison.
- Preserved executable permissions for shell scripts.

## Historical baseline

The decoder core in this release comes from the last validated improvement line:

- `0.5.0-alpha.26_ft_osd_gf2_order1_full`
- GF(2) OSD fallback: order-1 complete over 91 information bits.
- Order-2 search disabled.
- Later micro-sweep/reinject/coherent-metric beta experiments were laboratory-only.

## 0.5.0-ddsp-shadow-learning-lab

- Added the experimental MIND side tab.
- Integrated bundled tiny-dnn header-only sources under `third_party/tiny_dnn/`.
- Added `DeepDspController`, a fail-closed shadow-learning backend that trains only from messages already accepted by the existing classic decoders.
- MIND observes FT8/FT4, RTTY and CW receive audio and learns in the background from confirmed decode text.
- MIND Assist remains disabled until the model reaches a perfect local validation window; it does not currently inject extra decodes, key PTT, affect AutoQSO, change CAT, or replace the stable decoders.

## 0.5.0-madnness-shadow-learning-lab-fix1

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

## 0.5.0-madnness-persistent-eigen-profile
- Standardized the experimental neural lab name to MIND across the UI, logs, documentation and translations.
- Moved the active Eigen MLP profile from the generic 68→96→64→32 laboratory shape to the FT-family shadow profile 464→128→64→77.
- Added persistent MIND checkpoint storage under the writable MadModem application data directory, with atomic model writes and a companion `mind_stats.json` statistics file.
- The MIND panel now shows architecture, validation success, readiness percentage, checkpoint path, last checkpoint time and stats path.
- MIND remains fail-closed: no assisted decodes, AutoQSO, TX, CAT or PTT path is enabled by this lab package.

## 0.5.0-madnness-offline-prime-fix
- Fixed MIND shadow learning during offline FT WAV analysis and bundled Auto Test: the selected WAV is now used to prime MIND audio features before CRC-valid FT8/FT4 decode labels arrive.
- The MIND matrix activity and sample counters now update after offline WAV decodes instead of staying at zero.
- The visible learned-sample counter now survives checkpoint/stat reload by using the persisted FT/RTTY/CW sample counters when the in-memory training queue is empty.
- MIND remains shadow/fail-closed and does not add decodes, drive AutoQSO, key TX, or touch CAT/PTT.

## 0.5.0-mind-compact-panel
- Shortened the experimental neural side tab to the user-facing name MIND.
- Reorganized the neural side tab into compact Status, Brain activity, Control, Model and Teach sections.
- Removed long checkpoint paths from visible labels; full paths are now available as tooltips.
- Kept the neural lab fail-closed: no assisted decode injection, AutoQSO, TX, CAT or PTT control.

## 0.5.0-mind-perf-guard
- Isolated MIND from FT decoder timing: FT WAV analysis and FT Auto test now suspend neural training/checkpoint writes while the decoder is running; labels are queued and trained later during idle slices.
- Reduced default MIND idle training budget to 2 ms and capped it to 25 ms; the user may set 0 ms to collect labels without training.
- Throttled MIND UI/status updates and changed auto-checkpointing from frequent 15-second saves to a conservative 5-minute cadence.
- Offline WAV priming now uses a lightweight fingerprint instead of feeding the full WAV into the MIND feature extractor.

## 0.5.0 MIND CPU-only guard

- Confirmed MIND as a CPU-only Eigen shadow-learning backend.
- GPU/OpenCL acceleration is intentionally not included: MIND training remains deferred and low-budget on CPU so decoder timing stays predictable.
- Added visible MIND backend status in the side panel: `Backend: CPU Eigen (CPU only)`.
- Kept MIND fail-closed: no assisted decodes, no AutoQSO control, no TX/PTT/CAT interaction.
## 0.5.0-mind-ui-warmup-fix
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

## 0.5.0 MIND multi-profile visual panel
- MIND panel now exposes a Profile view selector: Auto, FT8, FT4, CW, RTTY.
- Brain activity visualization now changes layer geometry according to the selected profile:
  - FT8/FT4: 464 → 128 → 64 → 174
  - CW: 256 → 96 → 48 → 6
  - RTTY: 96 → 64 → 32 → 8
- Added split FT8/FT4 sample counters in MIND status/statistics.
- Auto profile view follows the currently observed mode/profile; manual view can inspect each planned dedicated network.
- No decoder, TX, CAT, PTT, scheduler or AutoQSO behavior changed.

## 0.5.0 MIND CW bootcamp
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
