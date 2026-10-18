# CW R12: native acquisition, spacing and CPU

> Historical 0.6.2 report. MadModem 0.6.3 ships only Native R12 and Neural R13; active model files are in `modems/cw/neural/model/`. See `RELEASE_0_6_3.md`.

Source revision: `0.6.2-source-r12-cw-acquisition-envelope`. Application version: **0.6.2**.

## Integrated behavior

- Native remains the default RX A/B engine. A short prelude no longer causes the first Morse element to merge with startup noise. Carrier acquisition is confirmed by sustained RF evidence, without a text or callsign prior.
- Native learns character/word gap families from completed confident spaces. Until enough separated clusters exist, the canonical conservative decision threshold remains active. Reset and capture discontinuity clear acquisition and spacing history.
- Exact duration-hazard values and FFT window/response calculations are cached. Repeated spectral logarithms are reused within each frame. Caching does not quantize the duration model.
- Neural convolution history indexing is hoisted outside the multiply loop. The legacy inference path retains its weights and arithmetic accumulation order.
- A third RX choice, **MM Neural Envelope (experimental)**, uses the same R8 weights with a causal envelope floor (10% peak amplitude, 0.5 s peak-power release). This reduces noise amplification during spaces. **MM Neural CTC (experimental)** remains selectable for comparison. The band skimmer remains native.
- Engine choice persists across mode changes and restart. Unknown settings fall back to Native. Neural state resets on engine changes, clear, tone changes, TX resume and capture gaps.

No DeepCW source or weights were added. No R11 candidate weights were promoted. The bundled model remains the original R8 model.

## Measured comparison

CER counts substitutions, deletions and insertions, including spaces. Lower is better. These small, separately identified sets are not a population error-rate estimate.

| Measurement | Native R8 | Native R12 | Neural R8 | Neural Envelope R12 |
|---|---:|---:|---:|---:|
| Fresh synthetic full QSO, 489 reference characters | 13 errors (2.66%) | **0 (0%)** | 35 (7.16%) | 26 (5.32%) |
| Entire fresh messages correct | 4/16 | **16/16** | 4/16 | 4/16 |
| Callsigns correct on those messages | 15/18 | **18/18** | 12/18 | 16/18 |
| False characters, four 10-second negative inputs | 0 | 0 | 0 | 0 |
| Existing RF development excerpts, 249 characters | 143 (57.43%) | **134 (53.82%)** | 127 (51.00%) | 127 (51.00%) |
| CPU ms per second of audio, actual application decoder route | 47.68 | **38.68** | 43.04* | 44.48 |

*The CPU entry uses the optimized R12 executable in legacy neural mode, with unchanged R8 weights and normalization, not the original R8 neural executable. The RF legacy output was unchanged (90 + 37 errors). Neural does slightly better than native on this tiny RF development subset, but does substantially worse on the fresh QSO and ARRL checks; it is not promoted to default.

The native CPU reduction is **18.9%**, measured with three repetitions in rotated engine order and no concurrent build/training/test workload. Each run uses the same 234.1 s input, including six seconds of flush, 997-sample blocks, RX A, native scanner/diagnostics and optional band decoding disabled. Construction, file I/O and audio block packing are excluded. CPU totals include process worker threads. This is offline processing cost, not audio-device latency or a universal hardware guarantee.

On 20 ARRL 18-WPM development segments (570 characters), Native R8 had 12 errors and R12 had zero. Correctly aligned word coverage increased from 92/103 to 103/103. Median correctly decoded word delay was 524 ms versus 476 ms. Timestamps are at DSP block ends (124.625 ms resolution) and exclude the sound card and GUI painting. The neural envelope variant had 103 errors and only 24/103 aligned correct words; its shorter conditional delay is not evidence of a better or faster usable decoder.

## Test separation and remaining limits

The 16 synthetic QSO seeds 124000000–124000015 and negative seeds 125000000–125000003 were evaluated after DSP parameter selection was frozen. They contain complete QSO turns, random callsigns/reports, 12–36 WPM, variable receiver filters and noise. Their zero native errors are specific to this finite sample.

ARRL clean practice audio was development material for selecting the envelope floor. The two VARC RF excerpts had already been inspected in R9/R11 and are **regression/development**, not independent radio validation. No newly acquired radio recording with a complete independently verified transcript was available for a new held-out RF score. Raw third-party recordings and full transcripts are not redistributed here. Further radio validation must use new annotated QSO, with tuning separated from evaluation.

A larger floor improved ARRL development CER but worsened synthetic development CER; it was rejected. Neural Envelope retains limitations in spacing, callsigns and RF robustness. This release does not claim neural superiority or a 5% radio error rate. Native Windows/macOS builds and receiver hardware remain outside the Linux checks.

## Verification and reproduction

Public measurements are in [cw-r12-checks](cw-r12-checks/). Native regressions include cold-start CQ, UR 599 599 and DL1ABC, speed changes, timing reset, noise, QSB, nearby carriers and previously supported cases. Engine integration checks cover A/B routing, envelope selection, resets and fallback. Settings save/load was tested for all three IDs. The optimized hazard calculation matched the original bit-for-bit on 120,000 combinations. The optimized neural core produced bit-identical 67,200 logits over 1,600 random feature frames.

Build the normal project with `-DBUILD_TESTING=ON -DMADMODEM_BUILD_CW_TESTS=ON`; run `MadModemCwNativeRegression` and `madmodem_cw_engine_regression`. `tests/CwEngineUiProbe.cpp` is an application-level UI probe, linked with application objects in place of main.cpp.

A smaller Qt Core/Gui build for old/new actual decoder-route comparisons is provided:

```sh
cmake -S tools/cw_validation -B build-cw-r12 -DCMAKE_BUILD_TYPE=Release
cmake --build build-cw-r12 --target cw_route_probe
cmake -S tools/cw_validation -B build-cw-r8 -DMADMODEM_SOURCE_DIR=/absolute/path/to/R8 -DCMAKE_BUILD_TYPE=Release
cmake --build build-cw-r8 --target cw_route_probe
python3 tools/cw_validation/compare_routes.py --old-probe build-cw-r8/cw_route_probe --new-probe build-cw-r12/cw_route_probe --out comparison
```

Python measurement tools need NumPy, SciPy and the dependencies already documented in `tools/cw_neural`. `prepare_real_pilot.py` documents the existing RF sources and annotation windows; its manifest can be passed using `--real-manifest`. Do not relabel these development recordings as a new holdout. Timing comparisons require the same machine/build flags and an otherwise idle host. The application requires no Python or external ML runtime.

### Final Linux build verification

The complete Release application linked successfully with Qt 5.15 and Hamlib 4.5.5. The final CMake native and engine regression executables passed. The application-level UI probe passed all three choices, settings persistence, mode switching, window reopening and return to Native using the offscreen Qt backend. Headless OpenGL/audio warnings did not prevent the assertions from completing; no hardware radio/audio-device test is implied. Qt Location and Qt Help tools were unavailable, so this build used the existing offline map and HTML-help fallbacks; LTO was disabled.

The standalone route-probe CMake build also passed. All three engines produced the same decoded output as the frozen measurement executable on the smoke input. See the corresponding build and test logs in `cw-r12-checks`.
