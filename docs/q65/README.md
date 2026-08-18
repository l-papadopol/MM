# Q65 engine — 0.5.8

Q65A, Q65B, Q65C and Q65D use one MadModem receive/transmit path under
`modems/q65/`. It is compiled in every normal Linux, Windows and macOS build;
there is no FFTW dependency, optional decoder switch or runtime fallback.

## Receive path

`Q65Decoder` owns UTC-period assembly and runs on the existing Q65 decoder
thread. `Q65NativeEngine` then performs:

1. a whole-search-window scan of the 22 Q65 synchronization symbols;
2. time, carrier-frequency and optional linear-drift refinement;
3. oversampled spectra for the 63 data symbols;
4. soft 64-value intrinsic probabilities for the selected A/B/C/D spacing;
5. QRA message passing, CRC validation and 77-bit message unpacking;
6. optional configured-call candidate assistance through the same codec;
7. bounded averaging in separate even/odd period banks.

The mode supports 15, 30, 60 and 120 second periods. Fast, Normal and Deep
change only bounded search/refinement budgets; they do not select another
decoder. The accepted audio-frequency range is recalculated for each period and
submode so the complete soft-metric fading window remains between DC and
Nyquist, including Q65D at 15 seconds.

## Transmit path

`Q65Transmitter` packs the 77-bit message, applies the Q65 QRA code, inserts the
22 synchronization symbols and produces the complete 85-symbol continuous-
phase waveform. Q65 QRA work and every FT4/FT8, MSK144 and Q65 message/hash
entry serialize access to process-global protocol state with
`WeakSignalCodecLock`. The common 77-bit packer and callsign hash are compiled
once in the shared codec target, rather than being duplicated in modem
libraries. Waveform synthesis runs directly at the selected
audio-device sample rate; there is no fixed-rate intermediate or resampling
backend.

The shared weak-signal UTC scheduler honours the selected first/second period
for 15, 30, 60 and 120 second operation. RX remains active while TX is armed.
A late timer never starts a shortened Q65 frame: it advances to the next
matching UTC period. Message coding and waveform synthesis are completed while
arming, so the boundary path cannot block behind the previous period's QRA
decode.

## Regression

`madmodem_q65_native_regression` generates real TX audio at 12 kHz and requires
the native receiver to recover the exact packed message in Q65A, Q65B, Q65C
and Q65D. It also verifies the symbol geometry for all four supported periods.
The test is part of normal CTest and package CI.
