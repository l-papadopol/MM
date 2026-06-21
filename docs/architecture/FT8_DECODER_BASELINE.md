# FT8 decoder baseline — MadModem 0.5.1

MadModem 0.5.1 consolidates the validated FT8 decoder state from the `0.5.1-alpha.26` line.

Validated Auto Test baseline on the four bundled WAV files:

| WAV | Expected MadModem decodes |
|---|---:|
| `websdr_test6.wav` | 26 |
| `test_21.wav` | 25 |
| `test_18.wav` | 16 |
| `test_05.wav` | 21 |
| **Total** | **88** |

The decoder core keeps the GF(2) OSD fallback with full order-1 over 91 information bits and order-2 disabled. Later beta02..beta08 experiments, including micro-sweep reinjection and partial WSJT-X coherent metric tests, are not part of the production baseline because they did not improve the validated total decode count.

`compare_ft8_wsjtx_madmodem.sh` is included as a developer/support tool for comparing the bundled WAV set against WSJT-X/jt9.
