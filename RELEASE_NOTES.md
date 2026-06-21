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

This release makes CW dual-RX easier to use on air: A/B decoded streams are tagged and colour-coded, RX B can be disabled from the Mode panel, and the waterfall shows dashed yellow CW filter boundaries around each selected tone.


## MadModem 0.5.18

- Fix PSK/BPSK31 receive lock acquisition: the decoder no longer learns a clean PSK carrier as noise before opening squelch.
- Varicode validity is now used as an additional PSK lock validator before text is emitted, reducing blank RX terminals on valid BPSK31 signals.
- Keep Windows dual CPU builds from 0.5.11 and all 0.5.17 Settings/User-QTH fixes.

## 0.5.17 - User/QTH settings page rebuild hotfix

This hotfix replaces the unreliable embedded QDialog used for User / QTH / Macros with a direct QWidget implementation inside the unified Settings dialog. The page no longer depends on embedding a standalone dialog, so it should not appear as a blank white tab. All previous 0.5.16/0.5.15 fixes are retained.

## MadModem 0.5.14 - CW dual RX and clearer MIND panel

## 0.5.16 - Settings embedded pages hotfix

- Fixed the unified Settings dialog embedding path: child QDialog pages are now re-parented and explicitly shown after changing them to Qt::Widget. This prevents the User / QTH / Macros page from opening as a blank tab on Windows/Linux builds.
- Keeps the 0.5.15 fix that ignores stale serial PTT COM-port values when PTT is set to CAT/Hamlib, so rotator endpoint conflict detection only reserves the PTT port for Serial RTS/DTR.


- Fixed a false rotator endpoint conflict when the old serial PTT combo still contained a COM port but the active PTT method was CAT/Hamlib.
- Rotator endpoints are now compared with the PTT serial port only when PTT is actually Serial RTS or Serial DTR.
- The live rotator warning now uses the current Settings dialog state, so switching PTT mode inside the dialog clears stale warnings immediately.


This release makes CW operation match the planned dual-marker workflow. In CW mode, left-clicking the waterfall tunes RX A on the green marker; right-clicking tunes and enables RX B on the blue marker. Both decoders share CW speed/filter settings but maintain independent decoder state. The MIND panel has also been simplified so users can understand what Off, Training and Active actually do.

## MadModem 0.5.12 - Logbook statistics PDF and display settings

This release moves the worked-call strike-through preference out of the Logbook toolbar and into Settings, where it belongs. The Logbook window now has a dedicated "Save statistics PDF..." command. FT decode highlighting also stops painting every merely not-yet-worked call with the red dashed outline; that strong visual style is reserved for high-priority new-DXCC highlights. The report is designed for printing and summarizes the selected records, current search result, or full logbook with MyQSO-style KPI cards and tables.

Windows packaging remains dual-target: use MadModem-Legacy.exe on older non-AVX2 CPUs, and MadModem.exe on modern AVX2/FMA CPUs.

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

This maintenance build renames the operator-facing MIND modes to `Off`, `Training` and `Active`. `Training` is the safe learning/scoring mode that does not change decoder output; `Active` is the guarded operating mode that may rank/prioritize candidates and run mode-specific helpers. The MIND tab has also been reorganized to prevent clipped labels in the narrow right-side panel, and the new strings are covered by the runtime translation dictionaries.

## 0.5.6 — MIND Multi-Mode Assist Foundation

This release keeps MIND Off as a true baseline bypass, while changing Training/Active benchmarking to use the same native candidate-driven cooldown used in live FT operation. AutoTest no longer applies a special MIND freeze; if Training/Active still slow the decoder, the report now exposes real feature extraction, queueing, inference or threading overhead rather than a sterile lab result.

This build fixes the critical issue where MIND Assist Off could still slow FT AutoTest by leaving ranker instrumentation, feature extraction, queued samples or locks in the native decoder path. Off is now a hard bypass: no MIND scoring, no sample export and no MIND controller calls from the FT candidate loop. Training/Active with no ranker model can still collect data, but scoring/pruning only starts after a model is loaded and ready.

## 0.5.1 — MIND autonomous trainer

## 0.5.1 MIND Ranker v1

This package is versioned as 0.5.1. The validated 0.5.0 FT decoder baseline remains the reference for timing; 0.5.1 introduces the new MIND probabilistic FT candidate ranker/pruner and must be tested against the 0.5.0 timing target.


This maintenance package removes the manual MIND training controls from the operator UI. Training is now automatic: MadModem runs the Eigen/OpenMP trainer on a low-priority background thread, computes its own adaptive budget, pauses during FT timing-critical work, avoids active CW operation, and uses larger idle slices only when the user is in non-critical workflows such as logbook/settings/runtime-log consultation.

The MIND panel no longer exposes trainer budget, Save, Load or Reset controls. `MIND Assist: Off / Training / Active` remains available, but final FT8/FT4 text acceptance is still guarded by the deterministic LDPC/CRC/unpack/parser path.

## 0.5.1 — MIND Assist / CW cleanup / FT needed highlight

This maintenance package keeps the validated FT8/FT4 decoder core unchanged. It adds the operator-facing `MIND Assist` selector with `Off`, `Training` and `Active` modes; Active remains guarded so final FT text is accepted only by the deterministic CRC/unpack/parser path.

The FT decode table needed-station outline is no longer CQ-only: any decoded valid callsign that is not in the ADIF logbook can receive the dashed red outline, including stations currently inside another QSO. CQ rows keep their CQ colouring.

The old generic DSP side tab is removed. The only useful CW conditioning control, `Software AGC`, is now placed directly in the CW Mode settings under AFC. Multi-marker CW reception remains the next planned CW architecture step.


## 0.5.1 — MIND Eigen/OpenMP batch trainer

This build focuses on the MIND training bottleneck without changing FT8/FT4 decode, TX, CAT/PTT, scheduler or AutoQSO. The MIND trainer remains a dedicated thread, but the FT-native neural training path now uses Eigen/OpenMP batched matrix-matrix operations instead of single-sample matrix-vector loops. OpenMP is required for the optimized MIND build; AVX2/FMA/SSE4.2 is used as the x86_64 optimization baseline.


## MIND CW Bootcamp compile fix

This maintenance package fixes the Linux/GCC build error in `ai/DeepDspProfileNet.cpp` caused by ambiguous `QDataStream << Eigen::Index` overload resolution. Eigen dimensions are now cast to fixed Qt integer types during checkpoint serialization. Runtime DSP, FT decoders, TX, CAT/PTT and AutoQSO are unchanged.

## 0.5.1 i18n title fix

This maintenance build is based on the stable `0.5.1-i18n-full-review` source line. It abandons the experimental MSK144 branch and fixes the localized main-window title duplication/corruption issue. `MadModem 0.5.1` is now kept canonical and is not translated.

# MadModem 0.5.1 release notes

MadModem 0.5.1 is the production consolidation of the validated `0.5.1-alpha.26` source line.

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

This build starts the MIND AI phase from the stable `0.5.1_i18n_titlefix` baseline. The new MIND tab is a laboratory monitor for MIND/Eigen shadow learning across FT8/FT4, RTTY and CW. It learns only from messages that the existing decoder has already accepted. The feature is intentionally fail-closed: assisted decoding is not used for QSO/AutoQSO/TX until validation proves the model is ready.

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

## 0.5.1 MIND CPU-only guard

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

### MIND autonomous latency/status fix
This build keeps MIND training fully autonomous while making the runtime latency rules stricter. During FT candidate bursts the native cooldown defers trainer GEMM work, checkpointing and UI churn without applying an AutoTest-only freeze. The MIND panel now reports the loaded model/dataset state explicitly and follows the selected runtime mode instead of stale profile state.
