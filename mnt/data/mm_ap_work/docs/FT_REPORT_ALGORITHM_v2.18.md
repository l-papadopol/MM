# FT report calculation in v2.18

MadModem v2.18 moves the FT8/FT4 report calculation to the same logical place used by
WSJT-X/MSHV-style decoders: after the candidate has produced a valid LDPC/CRC decode
and the 77-bit payload has been unpacked.

## What is no longer allowed

- no report from waterfall brightness;
- no report from UI gain/colour scale;
- no report from sync score;
- no second heavy scan of the audio slot only to invent a parallel report.

## MSHV reference logic used

The bundled MSHV reference decoder computes the report after a valid codeword by:

```cpp
TGenFt8->make_c77_i4tone(message91,i4tone);
xsig += s8_[i][i4tone[i]] * s8_[i][i4tone[i]];
xsnr = db(xsig/xbase - 1.0) - 32.0 - 4.0;
clamp(-25, +49);
```

In v2.18 the compact MM live decoder follows this same structure:

1. reconstruct the transmitted tone index for each CRC-valid FT8/FT4 symbol;
2. sum squared power on the reconstructed codeword tones;
3. build a local baseline from off-codeword tones in the same demodulation matrix;
4. apply the MSHV-style formula and clamp;
5. store the resulting integer in `Decode::snrDb`.

This is still computed inside `Ft8RxDecoder`, not by the UI.  The rest of the program
only propagates the decoder report.
