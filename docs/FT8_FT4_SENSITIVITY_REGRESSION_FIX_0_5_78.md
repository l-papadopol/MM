# FT8/FT4 sensitivity regression fix — 0.5.78

## Evidence from the first wideband live test

The attached FT8 log showed that the audio and scheduler were healthy:

- zero sequence gaps, sample gaps, overlaps and timestamp jumps;
- 14/14 boundary workers;
- waterfall render around 1 ms with zero dropped rows;
- 672–703 boundary candidates per slot.

Despite that, the three complete FT8 boundary jobs decoded only 0, 2 and 0 messages. The new recovery telemetry also showed:

- sum-product: 0 recoveries from 212–311 attempts per boundary;
- complex coherent metrics: 0 recoveries from 11–21 attempts;
- bucket rescue: 0 decodes from 57–77 injected candidates.

Therefore the loss was inside the decoder changes, not the audio pipeline or resource controller.

## Source-level error in the FT8 baseline port

The first patch divided every Costas tone energy by a separate frequency-dependent floor calculated from the 21 Costas rows before constructing the Costas expected/off-tone ratio.

That is not what the inspected WSJT-X source does. In `sync8.f90` and `ft8var/sync8var.f90`, WSJT-X first calculates the complete Costas sync statistic over frequency and time, then forms the per-frequency maximum `red(i)`, and finally normalizes those frequency scores by their 40th percentile. It does not independently normalize each expected and off-tone energy before the Costas ratio.

The incorrect port altered the statistic itself, changed candidate ranking, promoted noise/notch artefacts and reduced the number of useful candidates reaching LDPC. The live FT8 candidate finder is therefore restored byte-for-byte to the prior adaptive-runtime checkpoint.

## Bucket rescue was not a shadow test

The implementation reserved 36 positions from the fixed live candidate budget and filled them with candidates rejected by the normal bucket cap. This displaced ordinary validated candidates. The log reported dozens of rescue candidates and zero valid rescue decodes.

The active rescue tail is removed. A future A/B test must be genuinely non-displacing: it may run only after the complete normal candidate list and only with spare time, while recording what it would have recovered.

## Experimental recovery paths

The sum-product and complex coherent implementations remain in the source for controlled offline comparison, but are not allowed to consume the realtime boundary budget until same-WAV tests show CRC-valid recoveries. Likewise, the post-decode timing search for FT8/FT4 cancellation remains offline-only.

## FT4 policy

The live FT4 candidate finder is restored byte-for-byte to the previous adaptive-runtime checkpoint because no same-audio evidence yet supports replacing it. The corrected FT4 SIC remains active: it subtracts the continuous GFSK waveform derived from the actual CRC-valid 103-tone codeword rather than re-detecting the strongest observed tone symbol by symbol. Live cancellation uses the decoder's validated timing; the extra timing-offset search is offline-only.

## Protected invariant

The FT8 ghost-candidate gate is unchanged.

```text
SHA-256: 6e7b4f9c15b21a01f3351442c08c916a7f57f49686a98dfc7acb2296adee1e36
```

## Required validation

The next live test should show the established candidate/LDPC balance returning. For the same 12-core/16-thread machine, expected qualitative signs are:

- FT8 boundary search time near the previous checkpoint rather than the enlarged baseline path;
- no live SPA/coherent/bucket attempts;
- candidate counts and LDPC attempts comparable to the adaptive-runtime version;
- decode count judged only against the same band/time/audio or, preferably, identical WAV files.
