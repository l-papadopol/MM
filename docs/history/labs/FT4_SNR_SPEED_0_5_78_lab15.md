# FT4 SNR + speed pass — 0.5.78-lab15

Base: 0.5.78-lab14.

## Why FT4 showed fixed -21 dB

`findFt4Candidates()` ranks FT4 candidates with a log-domain sync score and does not fill the WSJT-X `candidate(2)` SNR-ratio equivalent. The lab14 display path later called `wsjtxFt4ReportDb(candidate.syncRatio - 1.0)`, so valid FT4 decodes fell through to the FT4 minimum report, `-21 dB`.

lab15 now computes the FT4 displayed report after CRC/unpack succeeds by reconstructing the selected 4-FSK codeword powers and comparing them with the average off-tone power in the same symbols. This changes only the report shown in the UI/log; it is not a decode gate.

## Speed changes

FT8 already used an optimized multi-tone Goertzel bank. FT4 was still evaluating many 4-tone symbols as separate single-tone Goertzel passes. lab15 adds a FT4 four-tone bank with AVX2/FMA runtime dispatch, SSE2 fallback and scalar fallback.

Live FT4 residual rescans are now budget-gated. Deep/DSP+ may still use a second residual pass, but only if the first pass leaves enough time. Offline/WAV analysis keeps the wider multi-pass path.

## Expected test signs

- FT4 decode lines should no longer all show `-21 dB`.
- FT4 live slots that previously reached 1.1--1.6 s should more often stay close to the first-pass decode time.
- The FT4 profiler `search` and `LDPC` fields should now include residual-pass cost instead of reporting only the first pass.
