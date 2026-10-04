> **Superseded for live decoding by `FT8_FT4_SENSITIVITY_REGRESSION_FIX_0_5_78.md`.** The algorithms below remain available only for controlled offline A/B where stated; the first live deployment reduced sensitivity.

# FT8/FT4 wideband sensitivity upgrade — 0.5.78

## Scope

This change set applies source-derived receiver improvements to the native MadModem FT8/FT4 decoder. The implementation was compared directly against the attached WSJT-X Improved 3.1.0 source archive (`f159cde764ababb2c63ee8f307f18f29db0db322e152622cc5b4e2c7575fb5a0`) and the bundled MSHV/Decodium sources.

The objective is to increase the number of CRC-valid signals recovered from crowded, weak, partially captured, or interfered slots without replacing the existing realtime scheduler.

## Explicit invariant: FT8 ghost-candidate gate

The user explicitly required that the FT8 `ldpcGhostCandidate` gate remain untouched. Its complete comment and code block is byte-identical to the prior adaptive-runtime checkpoint.

SHA-256 of the preserved block:

```text
6e7b4f9c15b21a01f3351442c08c916a7f57f49686a98dfc7acb2296adee1e36
```

No threshold, branch, comment, or rejection result in that block was changed.

## FT4 changes

### Candidate generation

- Replaced the uniform 20.833 Hz candidate sweep with an averaged 4096-point Nuttall spectrum.
- Added 15-bin smoothing.
- Detects receiver passband edges from the middle-half 30th percentile and trims regions more than 20 dB below passband noise.
- Fits a ten-segment, 10th-percentile, fourth-order lower-envelope baseline with the WSJT-X 0.65 dB lift.
- Normalizes the smoothed spectrum by that baseline.
- Finds local maxima and applies parabolic sub-bin frequency interpolation.
- Converts the observed spectral centre to the FT4 lowest-tone frequency with the `-1.5 tone-spacing` offset.
- Preserves the RX marker and a bounded strongest-bin fallback so a flat or notched spectrum cannot create an absolute blind spot.
- Searches DT only for the selected spectral seeds and retains up to three timing maxima per seed.

### Complex coherent demodulation

The complete boundary/offline path now mixes and low-pass filters each candidate to a complex 666.67 Hz baseband with 32 complex samples per symbol. It then:

- refines timing over ±4 baseband samples;
- keeps correlations for all 103 symbols, including sync symbols;
- forms coherent 1-, 2-, and 4-symbol metric families across the full symbol stream;
- applies the reference tail patches for the final incomplete groups;
- extracts the three FT4 data ranges (global bits 8–65, 74–131, and 140–197, zero based);
- adds the single-symbol ratio family and a cherry-picked family;
- retains normalized min-sum as the first FEC path and runs sum-product only as a measured fallback on the same LLR vector.

The early gate remains a low-latency path and does not run the complex coherent recovery.

### Exact successive-interference cancellation

FT4 cancellation no longer re-detects the strongest tone independently in each data symbol. After LDPC and CRC success it now:

- reconstructs the exact 103-tone sequence from the validated 174-bit codeword;
- regenerates the continuous GFSK reference waveform;
- starts the reference one symbol before the first sync block, matching the three-symbol GFSK pulse support;
- refines cancellation timing over a bounded local offset set;
- estimates a smooth complex amplitude/phase envelope from `RX × conj(reference)`;
- subtracts `2 × real(envelope × reference)` from the residual.

This prevents an overlapping station or interferer from being mistaken for the decoded codeword during subtraction.

## FT8 changes

### Robust Costas candidate baseline

The existing 4096-point Costas FFT matrix is retained. For every frequency bin the candidate finder now calculates a 40th-percentile temporal floor from the 21 Costas-symbol spectra and lightly smooths it across frequency. Candidate energies are normalized by this local floor before the existing Costas score is calculated.

This compensates for sloping passbands, persistent carriers, and local noise variation without changing the current FFT engine.

### Bucket-pruning shadow rescue

The proven primary per-frequency bucket cap remains in place. Complete boundary/offline passes receive a bounded shadow-rescue tail containing close candidates that only failed the bucket cap. Rescue candidates use tighter DT/DF de-duplication and are explicitly tagged.

Runtime telemetry reports both:

- rescued candidates attempted;
- CRC-valid decodes originating from the rescue tail.

The early gate receives no rescue tail.

### Complex 200 Hz boundary path

When the normal and existing multi-metric paths fail, complete boundary/offline decoding can create a complex 200 Hz candidate baseband with 32 complex samples per symbol. The recovery path:

- refines coherent timing over ±4 baseband samples;
- keeps all 79 symbol correlations;
- forms coherent 1-, 2-, and 3-symbol metrics, as used by the FT8 reference decoder;
- intentionally allows the final group in each 29-symbol data half to extend into the following Costas block while discarding non-data output bits;
- adds a single-symbol ratio metric and a cherry-picked metric;
- tries these families only after the cheaper established paths fail.

This path is never run by the early gate and does not alter the ghost-candidate gate.

### FEC comparison and cancellation refinement

- Added a log-domain sum-product LDPC fallback using the same 174 LLRs as normalized min-sum.
- Normalized min-sum remains first; telemetry counts attempts and CRC-valid candidate recoveries from sum-product.
- FT8 already regenerated the CRC-valid message waveform. Cancellation now tests a bounded set of nearby timing offsets and uses the one with the highest correlation, without changing the displayed DT.

## Deliberately deferred

Deeper AP hypotheses and higher-order OSD are not expanded in this checkpoint. The agreed dependency was to correct wideband candidate generation, coherent demodulation, LLR quality, and SIC first. Increasing AP/OSD depth before measuring those stages would add CPU cost while obscuring where recovered decodes actually originate.

## Runtime telemetry

The log adds one compact line when any new recovery path is used:

```text
FT wideband recovery: SPA recovered/attempted R/A, coherent recovered/attempted R/A, bucket rescue decoded/candidates R/C
```

These counters are diagnostic only and do not change scheduling or decoder decisions.

## Validation requirements

Static checks in this source package verify structure, source invariants, metric indexing, tone mapping, and the preserved ghost block. A full sensitivity claim still requires a same-audio comparison against the previous checkpoint and WSJT-X/MSHV, recording the union and differences of message, frequency, DT, and SNR per slot.

No claim of additional real-world decodes should be made solely from source inspection or execution time.
