# FT RX comparison: MSHV 2.76.5 vs MadModem v2.19/v2.20

## Executive summary

MadModem v2.19 was not a complete copy of MSHV's FT receiver.  It reused MSHV protocol components and had an MSHV-derived compact decoder, but the candidate search and some report baseline details were still MadModem-specific.

v2.20 makes the first careful assimilation step: the FT8 candidate search now follows MSHV's `sync8()` scoring principle instead of the old MadModem log heuristic.

## MSHV FT8 algorithm points

From `src/HvDecoderMs/decoderft8.cpp` in MSHV 2.76.5:

1. `sync8()` computes symbol spectra at 12 kHz.
2. `NSPS = 1920`, `NSTEP = NSPS/4 = 480`, `NFFT1 = 3840`.
3. Frequency bin spacing is `12000 / 3840 = 3.125 Hz`.
4. The three FT8 Costas blocks are tested.
5. MSHV computes both full ABC Costas sync and BC-only sync.
6. Sync is a ratio of expected Costas tone power to the mean of off-Costas tones.
7. Candidates are normalized with percentile/baseline logic.
8. Near duplicates are suppressed within about 4 Hz and 0.08 s.
9. Candidates go through `ft8b()` for LDPC/CRC/AP work.
10. Valid decodes may be subtracted and further passes run.
11. SNR/report uses reconstructed tones and the `sync8()`/`sbase` baseline.

## MadModem v2.19 differences

Before v2.20:

- search grid was coarser: 160 ms and 6.25 Hz;
- scoring used log(expected tone / guard tone);
- near-dupe rule was much wider: 25 Hz and 0.35 s;
- report baseline was not the exact MSHV `sbase` path.

## MadModem v2.20 assimilation

v2.20 changes the compact decoder without importing the whole MSHV QObject tree:

- FT8 candidate score is now `expected Costas power / off-tone mean`, matching the MSHV sync law;
- BC-only sync fallback is included;
- frequency grid is 3.125 Hz;
- time grid is 80 ms, chosen as a safe compromise because MM still uses compact tone-energy evaluation instead of MSHV's FFT matrix;
- candidate floor is 1.33;
- near-dupe rule is 4 Hz / 80 ms;
- candidate baseline is carried into the report path.

## Remaining gap

For true MSHV-equivalent decoding, MM still needs either:

- a clean wrapper around the full MSHV `DecoderFt8` pipeline; or
- a deeper port of exact `sync8()`, `get_spectrum_baseline()`, `baseline()`, AP paths, and subtraction passes.

v2.20 is intentionally conservative: it imports the behaviour that is low-risk and directly beneficial first.
