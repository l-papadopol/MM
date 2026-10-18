# CW Neural R13: final results and integration

> Historical 0.6.2 report. MadModem 0.6.3 ships only Native R12 and Neural R13; active model files are in `modems/cw/neural/model/`. See `RELEASE_0_6_3.md`.

**Application 0.6.2, source R13.** A new optional **MM Neural Search (experimental)** / **MM Neural Ricerca (sperimentale)** engine combines separately adapted MM weights, a causal 20% amplitude envelope floor, and bounded CTC prefix search. Native stays the default. Legacy CTC and Envelope choices retain R8 weights. All three neural choices use the optimized vectorizable inference layout.

## Accuracy: frozen final comparison

CER includes spaces, insertions, substitutions and deletions. All engines receive the same waveform through the actual `CwDecoder` application route, RX A, with the same scanner/diagnostic settings. All parameters and candidate weights were frozen before these results were inspected. The new clean recording is independent of training/development; the two RF excerpts are existing development material. These small sample sets do not establish population-wide superiority.

| Scope | Reference characters | Native R12 | Neural CTC R12 | Neural Envelope R12 | Neural Search R13 |
|---|---:|---:|---:|---:|---:|
| 32 new complete synthetic QSO | 1,017 | **6 (0.59%)** | 113 (11.11%) | 96 (9.44%) | 21 (2.06%) |
| 40 new arbitrary strings | 361 | 26 (7.20%) | 13 (3.60%) | 13 (3.60%) | **7 (1.94%)** |
| New ARRL recording, 20 whole-word clips | 592 | **0** | 528 (89.19%) | 152 (25.68%) | 28 (4.73%) |
| Same new recording, continuous passage | 204 | **0** | 168 (82.35%) | 66 (32.35%) | 8 (3.92%) |
| Existing VARC RF development excerpts | 249 | 134 (53.82%) | 127 (51.00%) | 127 (51.00%) | **92 (36.95%)** |
| Six fixed CQ/RST/callsign regressions | 203 | **0** | 10 | 7 | 2 |
| False characters on 20 new six-second negative inputs | — | 5 | 1 | 1 | **0** |

On the new QSO set, exact messages improve from **5/32 to 25/32** compared with Envelope; Native achieves 27/32. Correct whole callsign tokens improve from **20/30 to 26/30**; Native achieves 27/30. The six fixed fixtures include `CQ CQ CQ DE IZ6NNH IZ6NNH PSE K`, `UR 599 599 BK` and complete reply/closing turns. They are known regression phrases, not a fresh text holdout.

Search reduces new-QSO errors by **78.1%** and held-out ARRL clip errors by **81.6%** relative to Envelope R12. RF development errors decrease by **27.6%**. The RF error rate is still high, and the RF subset is not independent. Native remains more accurate on the clean QSO and ARRL conditions. Search is a useful experimental improvement over the previous neural, not a universal replacement for Native.

The continuous passage may overlap sampled clips from the same recording; do not add these rows as independent recordings. Synthetic strings are not a substitute for operator diversity or new annotated on-air HF QSO.

## Search versus new weights

The frozen untrained-R8 search ablation separates decoder/normalization gains from adaptation. It has 56/1,017 QSO errors, 12/592 ARRL clip errors, 4/361 random-string errors and 128/249 RF development errors. Adapted Search has **21, 28, 7 and 92**, respectively. Thus adaptation helps QSO/RF and negative suppression but loses some clean-ARRL/random-string performance versus the new untrained search ablation. This tradeoff is retained in the public results rather than concealed. The ablation is a research comparison, not a fifth UI choice.

The chosen checkpoint is full-network step **12,000**, selected on development only, with a constant +3 space-logit calibration. It came from **900.3 s / 12,038 completed updates**. An additional input-layer/output-bias-only run used **420.3 s / 6,599 updates**, including the two newly acquired training bulletins, and was not selected. Total training wall budgets were about 22 minutes, partly overlapping. The selected model used the original article training groups; the extra bulletins were tested in the unsuccessful frozen-stack alternative. No held-out recording, VARC transcript or decoder-generated pseudo-label entered training or selection.

Raw training logs monitor greedy output; final selection uses calibrated, causal prefix-search output matching the runtime. The raw greedy losses/CER are therefore not the delivered decoder's score. `SEARCH_SELECTION.json` records every eligible/ineligible candidate and the comparison with the released Envelope frontend.

## Processing cost and word delay

An isolated **unchanged-weight core** test over 32,000 frames / 1,344,000 logits gives median CPU **1.266 s → 0.438 s**, about **65% less**, with every output bit identical. Five repetitions ran in rotated order without concurrent compute work. This measures the matrix/activation core, not the entire application.

| Actual application route, CPU ms per second of audio | Native R12 | Envelope R12 | Search R13 |
|---|---:|---:|---:|
| 8 kHz input | 40.31 | 45.83 | 44.92 |
| 48 kHz input | 107.01 | 103.47 | 102.83 |

These are medians of three rotated repetitions on the same 60 s RF passage plus 3 s flush. Ranges overlap substantially: e.g. 48-kHz Search is 5.77–7.31 CPU seconds and Native 6.01–6.94. Consequently **there is no robust whole-application CPU victory over Native**. Search's median is close to the preceding Envelope engine; the core speedup offsets additional search work. Scanner and native diagnostics are retained in all modes. Full per-run timings and bounds are in `CPU.json`; no claim of a fixed hardware speedup is made.

On the newly held-out ARRL clips, median last-character display delay for correctly aligned words is **499 ms Native versus 271 ms Search**, at 124.625 ms timestamp resolution. Coverage is **97/97 versus 70/97 words**, respectively. On the continuous passage it is 538 ms / 31 of 31 versus 249 ms / 23 of 31. These conditional delays exclude sound-card and GUI painting time, and do not imply that Search correctly decodes every word sooner. Envelope's shorter conditional delay covers only 19/97 clip words. Accuracy, coverage, processing cost and output delay must be considered together.

## Verification and remaining work

- Complete Linux Release application build with Qt 5.15 and Hamlib 4.5.5: passed. Existing offline map/HTML-help fallbacks were used; LTO was disabled.
- Native CW regression suite: passed. Selected-engine routing, Search RX A/B, engine switching, clear, capture gaps and chunk invariance: passed.
- Four-choice Italian UI selection, persistence, mode switching, reopen and return to Native: passed with Qt offscreen. Native Windows/macOS builds and radio hardware remain unverified here.
- Selected C++ versus PyTorch: 67,200 logits, maximum absolute difference **9.54e-6**, zero argmax mismatches. Legacy logits remain bit-identical to R8.
- Python versus C++ prefix decoder: **200 nonempty emission events identical**, including their frame positions. Marginal alignment recovery, blank-separated repeats, reset and quiet-section tests pass.
- Search output is identical for audio chunks of 1, 64, 257, 997 and 4,096 samples. Existing engine tests also exercise 48-kHz input.

The next useful data improvement is **more independently annotated off-air QSO from different operators, radios and noise/interference conditions**. The new 118-minute archive broadens text, but all three files are clean 18-WPM practice recordings. It does not supply this operator/RF diversity. Keep new RF evaluation separate from tuning; never turn either decoder's guessed text into unquestioned training truth.

## Files and reproduction

Research references and rationale: [CW_NEURAL_R13_RESEARCH.md](CW_NEURAL_R13_RESEARCH.md). Metrics, freezes, selection and test/build logs: [cw-r13-checks](cw-r13-checks/). Active Search weights/checkpoint/card: `modems/cw/neural/search_model`. Raw research checkpoints and histories: `tools/cw_neural/experiments/r13`.

Acquire/audit the new material with `python3 tools/cw_validation/expand_arrl.py --out /path/to/extra-pool`. Build old/new probes using the standalone CMake project in `tools/cw_validation`, with `MADMODEM_SOURCE_DIR` pointing to each source revision. Then:

```sh
python3 tools/cw_validation/compare_r13.py --baseline-probe /path/to/r12/cw_route_probe --new-probe /path/to/r13/cw_route_probe --extra-pool /path/to/extra-pool --out /path/to/comparison
```

Optionally supply the existing RF manifest via `--rf-manifest`; it remains development material. `--search-r8-probe` accepts the frozen untrained-search ablation executable when separately available. Private predictions/transcripts generated by these local tools stay local. The application itself requires no Python, PyTorch or external ML runtime.
