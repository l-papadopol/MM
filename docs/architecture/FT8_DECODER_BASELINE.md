# FT8 validated decoder baseline — retained in MadModem 0.5.8

The frozen decoder reference is the validated production line carried into
`0.5.76ad`. On the four standard WAV recordings, the required MadModem counts
are:

| WAV | SHA-256 prefix | Expected decodes |
|---|---|---:|
| `websdr_test6.wav` | `0bae0c9f679c…` | 26 |
| `test_21.wav` | `3185aa62f933…` | 25 |
| `test_18.wav` | `f7490294eeed…` | 16 |
| `test_05.wav` | `8fbc4e70e477…` | 21 |
| **Total** | | **88** |

The complete hashes and counts are stored in
`tests/ft8/validated_baseline.tsv`. Audio files are deliberately not bundled in
the production archive.

After building MadModem, validate an external copy of the four original WAVs:

```bash
scripts/check_ft8_validated_baseline.sh ./build-linux/MadModem /path/to/wavs
```

The script rejects files whose SHA-256 does not match, invokes the headless
`--ft-regression` decoder and fails when any per-file count falls below the
validated baseline. This prevents a higher total from hiding a regression on a
specific recording.
