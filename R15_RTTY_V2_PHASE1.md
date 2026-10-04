# R15 — RTTY V2 phase 1

This revision starts the RTTY robustness work with a measurable baseline rather than another ad-hoc filter tweak.

## Runtime changes

- One Mark/Space demodulator path inside `RttyDecoder`.
- Independent complex Mark and Space mixers.
- 2nd-order Butterworth baseband filters derived from the selected baud rate.
  - Normal: about 1.5 x baud total channel bandwidth.
  - Narrow: about 1.2 x baud total channel bandwidth.
- Selective-fading ATC with separate Mark/Space envelopes and noise estimators.
- Carrier gate no longer divides selected-tone energy by total wideband audio power.
- `DspConditioner` no longer owns a second RTTY Mark/Space filter bank.
- AFC retunes the same oscillators used by the channelizer; there is no separately centred narrow prefilter.
- Parallel RTTY tracks use the same decoder core after wideband candidate discovery.

## Synthetic testbench

`madmodem_rtty_synthetic_bench` generates deterministic ITA2 AFSK and checks:

- clean 45.45 baud / 170 Hz shift;
- AWGN at -6 dB;
- Mark selective fade at -18 dB;
- Space selective fade at -18 dB;
- CW interferer +20 dB, 300 Hz from the selected pair centre;
- CW interferer +30 dB, 400 Hz from the selected pair centre;
- close CW where Narrow must improve over Normal;
- +15 Hz common mistuning with decoder retuned as AFC would do;
- noise-only input must not emit characters.

The benchmark prints JSON lines containing decoded length, edit distance and CER for each scenario, then returns failure if the agreed regression limits are exceeded.

## Prototype observations before C++ CI

A mathematical replica of the new channelizer/ATC path was used to tune the initial constants. On the deterministic 48 kHz test message it produced 0% CER for clean audio, -6 dB AWGN, +/-18 dB selective fading cases, +20 dB CW at +300 Hz and +30 dB CW at +400 Hz. In the deliberately close +10 dB CW / +150 Hz case, the normal channel failed badly while Narrow reduced the model CER to about 2%. These are design-stage numbers; the actual C++ CTest output is the release criterion.

## Literature used for the design

- Kok Chen, W7AY, *Improved Automatic Threshold Correction Methods for FSK*.
- Harold Hallikainen, W6IWI, DSP Terminal Unit experiments on RTTY tone-filter bandwidth and dynamic threshold control.
- fldigi RTTY demodulator architecture: separate Mark/Space baseband processing and ATC-style decision logic.
