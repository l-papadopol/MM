# MadModem 0.5.0 release notes

MadModem 0.5.0 is the production consolidation of the validated `0.5.0-alpha.26` source line.

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
