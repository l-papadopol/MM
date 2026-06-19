
## 0.5.0 — MIND Eigen/OpenMP batch trainer

This build focuses on the MIND training bottleneck without changing FT8/FT4 decode, TX, CAT/PTT, scheduler or AutoQSO. The MIND trainer remains a dedicated thread, but the FT-native neural training path now uses Eigen/OpenMP batched matrix-matrix operations instead of single-sample matrix-vector loops. OpenMP is required for the optimized MIND build; AVX2/FMA/SSE4.2 is used as the x86_64 optimization baseline.


## MIND CW Bootcamp compile fix

This maintenance package fixes the Linux/GCC build error in `ai/DeepDspProfileNet.cpp` caused by ambiguous `QDataStream << Eigen::Index` overload resolution. Eigen dimensions are now cast to fixed Qt integer types during checkpoint serialization. Runtime DSP, FT decoders, TX, CAT/PTT and AutoQSO are unchanged.

## 0.5.0 i18n title fix

This maintenance build is based on the stable `0.5.0-i18n-full-review` source line. It abandons the experimental MSK144 branch and fixes the localized main-window title duplication/corruption issue. `MadModem 0.5.0` is now kept canonical and is not translated.

# MadModem 0.5.0 release notes

MadModem 0.5.0 is the production consolidation of the validated `0.5.0-alpha.26` source line.

## Multilingual UI review

This package refreshes the runtime localization layer. Runtime-created widgets and dialogs now share the same INI translation dictionaries used by the main UI. The static localization audit reports 1580 aligned keys for English, Italian, French, German, Norwegian and Czech, with zero non-technical English fallbacks. Technical abbreviations such as RX/TX/PTT/CAT/ADIF/FT8 and radio mode names are intentionally preserved where appropriate.

## Logbook ADIF export

The logbook ADIF exporter now has an optional UTC time interval filter. It uses the QSO start timestamp (`QSO_DATE` + `TIME_ON`) and includes records where `start <= TIME_ON < end`, which is useful for short contests, FT8 Activity Day windows, activations and consecutive exports without duplicate boundary records.

## FT8/FT4 decoder baseline

The FT8 decoder baseline is the GF(2) OSD full order-1 implementation with order-2 disabled. The expected bundled WAV Auto Test result is:

| WAV | Expected decodes |
|---|---:|
| `websdr_test6.wav` | 26 |
| `test_21.wav` | 25 |
| `test_18.wav` | 16 |
| `test_05.wav` | 21 |
| **Total** | **88** |

The later FT8 beta lab experiments (`beta02` through `beta08`) are intentionally not promoted into this production tree because they did not improve the validated decode count.

## Packaging and support tools

- `build_all.sh` builds Linux and Windows/MXE targets and can create the binary `MM.zip` package.
- `tools/package_mm_zip.sh` packages the built Linux/Windows executables, help files, legal files, `cty.csv`, test WAVs and documentation.
- `compare_ft8_wsjtx_madmodem.sh` is included for developer/support comparison against WSJT-X/jt9.

## Scope

This package is a clean source release. It removes scattered historical v2.x notes from the production documentation tree and keeps consolidated release notes, architecture notes and multilingual online help.

## MIND shadow-learning lab

This build starts the MIND AI phase from the stable `0.5.0_i18n_titlefix` baseline. The new MIND tab is a laboratory monitor for MIND/Eigen shadow learning across FT8/FT4, RTTY and CW. It learns only from messages that the existing decoder has already accepted. The feature is intentionally fail-closed: assisted decoding is not used for QSO/AutoQSO/TX until validation proves the model is ready.

## MIND shadow learning lab compile fix

This package fixes the first MIND lab compile error on GCC and adds a minimal operator workflow for corrected CW labels. CW is now intentionally conservative: automatic CW decoder characters are not used as supervised truth labels. The MIND panel includes a manual label field where the operator can teach verified CW/RTTY text such as `CQ IZ6NNH K` or `5NN TU`.

MIND remains fail-closed: it does not add decodes, trigger AutoQSO, PTT or CAT.


MIND Eigen update:
- Replaced active TinyDNN dependency with MadModem internal MIND Eigen MLP.
- Added MIND training-completion/decode-success percentage.
- Added live neural matrix activity widget.
- MIND remains shadow/fail-closed; no decoder, TX, CAT or AutoQSO assist is enabled by default.


## MIND / Eigen lab update

The experimental neural DSP tab is now user-facing as MIND. The active neural engine uses bundled Eigen header-only matrix algebra for the small internal MLP while remaining fail-closed: it does not add decodes, drive AutoQSO, key TX, or touch CAT/PTT.

## MIND persistent Eigen profile

This package makes MIND persistent. The Eigen MLP no longer restarts from scratch at every MadModem launch: weights are saved as an atomic checkpoint in the user application data directory and training counters are saved in `mind_stats.json`. The visible MIND panel now reports the active architecture, validation success, readiness percentage, checkpoint path and last checkpoint time.

The active lab profile is now `464 → 128 → 64 → 174`. This remains a shadow-learning laboratory only and does not inject decodes or affect TX, CAT, PTT or AutoQSO.

## MIND offline WAV priming fix

This build fixes the first visible MIND lab issue: offline FT WAV analysis and Auto Test produced CRC-valid FT8 decodes but MIND stayed idle because the live RX audio observer is intentionally disabled during offline analysis. The WAV is now used to prime MIND features before decode labels arrive, so the MIND panel can show learned samples and neural matrix activity while remaining fail-closed.

## MIND compact panel

The experimental neural side tab is now called MIND and uses a more compact layout. It shows readiness, validation, sample counts and live neural activity without long wrapped labels. Checkpoint and statistics paths are kept in tooltips instead of filling the side panel.

## MIND performance guard

This build fixes the first MIND performance regression: MIND no longer trains, checkpoints, or repaints the neural matrix during FT WAV analysis and FT Auto test. It only queues confirmed labels while the decoder is timing-critical, then resumes tiny idle training slices afterwards. The default idle training budget is reduced to 2 ms and can be set to 0 ms for label collection only.

## 0.5.0 MIND CPU-only guard

This build keeps MIND on the CPU-only Eigen backend. GPU/OpenCL work was not added, avoiding driver dependencies and keeping the FT decoder path predictable. The MIND panel now reports the active backend explicitly.
## MIND UI warm-up fix
This package keeps the CPU-only MIND shadow-learning laboratory but clarifies the panel state: early confirmed samples are shown as warm-up/collection before validation becomes meaningful. No decoder, TX, CAT, PTT or AutoQSO logic was changed.

### MIND v2 native FT candidate training
- Replaced the old FT audio/text fingerprint lab path with native FT8 candidate samples.
- MIND now learns from `dataMagnitudes[58][8]` flattened to 464 inputs and the CRC-valid 174-bit LDPC codeword as target.
- The checkpoint is now `mind_native_ft_eigen_v2.model`; old v1 fingerprint checkpoints are intentionally ignored.
- UI now separates bit accuracy, message/codeword exact accuracy and readiness.
- No MIND inference/training runs inside the FT decoder timing-critical path; the decoder only emits queued gold-label samples after LDPC+CRC+unpack succeeds.
- MIND remains shadow-only and cannot key TX, CAT, PTT or AutoQSO.

- MIND v2 native FT follow-up: stale v1 stats files are now moved aside, v2 stats are saved soon after native FT gold samples arrive, and warm-up progress is based on validation samples rather than raw sample count.

## MIND multi-profile visual panel
MIND now shows the neural geometry for the selected profile instead of a generic fixed matrix. Auto follows the current mode, while FT8/FT4/CW/RTTY can be selected manually. The change is visual/status-only and keeps MIND fail-closed.

## MIND CW bootcamp lab
This build adds the first dedicated CW training path for MIND. The CW profile uses a separate small neural network trained from synthetic dit/dah/gap/noise samples generated inside MadModem. The feature remains shadow-only: it does not alter CW text, FT decodes, TX, CAT/PTT or AutoQSO.

### MIND trainer thread / FT gold replay buffer
- MIND training now runs from a dedicated low-priority QThread instead of sharing the UI/decoder path.
- CRC-valid native FT samples are persisted to `mind_ft_gold_samples_v1.dat` and reloaded as the FT gold replay buffer on startup.
- MIND statistics now expose `Validation`, `Ready`, `Bit`, `Best Bit`, replay-buffer size and dataset path more clearly.
- No TX, CAT/PTT, scheduler, AutoQSO, FT decoder or MSK144 logic is changed.

## MIND UI cleanup
This package simplifies the MIND panel after field feedback. The old Ready/Pronto text label and the ambiguous Validation progress wording were removed. The main progress bar now shows the guarded MIND progress score, while learning quality remains visible through Bit and Best Bit. Section titles and the bottom explanatory note were removed to make the tab more compact. MIND remains fail-closed and does not affect decoder, TX, CAT/PTT, scheduler or AutoQSO behavior.


## MIND dedicated full-speed trainer
- MIND FT/CW training now runs on a dedicated normal-priority trainer QThread with a 20 ms service interval.
- Trainer budget range increased to 0..250 ms; default set to 50 ms so offline training can use meaningful CPU instead of idling around a few percent.
- Replay buffer and gold dataset persistence remain unchanged; decoder/audio/CAT/PTT/AutoQSO are untouched.
- `mind_stats.json` version bumped to 6 and records `trainer_thread=dedicated_fullspeed_qthread` plus `trainer_budget_ms`.

### MIND OpenMP UI no-CW-teach cleanup
This build keeps the Eigen/OpenMP batched MIND trainer but removes the visible CW/RTTY teaching controls from the MIND tab. FT learning remains based on native CRC-valid candidate matrices. The panel update rate is throttled while the trainer is running at high speed to reduce repaint flicker.
