# MSK144 implementation status — 0.5.8

The active MSK144 decoder is the self-contained implementation in:

- `modems/msk144/Msk144Decoder.cpp`;
- `modems/msk144/Msk144Decoder.h`.

It performs whole-period energetic candidate selection, local time/frequency
refinement, coherent frame averaging, MSK144 synchronization, LDPC decoding
and message unpacking. The candidate budget is distributed over the complete
UTC period, so a strong early ping cannot hide later events.

The same path handles 144-bit frames and 40-symbol hashed short messages.
MSK40 results are resolved against the configured local/DX calls; SWL mode can
show an unresolved numeric hash. TX chooses the short frame only for a valid
`<CALL1 CALL2> REPORT` message and never generates a non-protocol substitute.

Candidate ordering and final LDPC/CRC validation are deterministic. MIND is not
part of the active MSK144 path.

The live input uses a streaming 12 kHz resampler and assembles real UTC protocol
periods. Decode jobs are owned and joined by the decoder and carry a generation
token, so reset/mode changes cannot publish stale results.

TX uses the selected audio centre and a continuous-phase 1000 Hz tone pair.
FT4/FT8, MSK144 and Q65 serialize the bundled process-global message/hash state
with one `WeakSignalCodecLock`; Q65 uses the same lock for its QRA workspace.
Separate per-mode locks cannot protect shared codec state. The 77-bit packer and
callsign hash are compiled once, preventing duplicate symbols and divergent
hash state in the final executable.
The shared weak-signal UTC scheduler honours the operator's first/second-period
choice, leaves RX active while armed and starts only a complete protocol frame.
If the selected boundary has already been missed, transmission is deferred to
the next matching period. The waveform is prepared when the scheduler is armed,
leaving only RX stop, PTT and audio-device start on the timed boundary path.

`madmodem_msk144_native_regression` round-trips generated audio through both a
normal message and an MSK40 short message as part of normal CTest.
