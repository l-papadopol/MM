# MSK144 implementation status — 0.5.8

The active MSK144 decoder is the self-contained implementation in:

- `modems/msk144/Msk144Decoder.cpp`;
- `modems/msk144/Msk144Decoder.h`.

It performs bounded time/frequency search, coherent frame averaging, MSK144
synchronization, LDPC decoding and message unpacking. TX support is under
`third_party/mshv_gpl/port/HvGenMsk/` and the associated pack/unpack files.

Candidate ordering and final validation are classical and deterministic. MIND
is not part of the active MSK144 path.

The live input uses a streaming 12 kHz resampler and assembles real UTC protocol
periods. Decode jobs are owned and joined by the decoder and carry a generation
token, so reset/mode changes cannot publish stale results.
