# FT capture timeline and two-stage live decode — 0.5.78

## Problem corrected

The old FT live assembler used the UTC time at which a queued audio block was
processed. Under load, samples acquired before a 7.5/15-second boundary could
therefore be assigned to the next slot. Decoder work was asynchronous, but the
sample-to-UTC relationship was not explicit.

## Continuous sample timeline

`AudioEngine` now timestamps capture sample index zero once and derives every
later block timestamp from its monotonically increasing sample index and the
corrected input sample rate. Callback jitter cannot create artificial timing
gaps or move an entire block across a UTC boundary.

`AudioBlock` carries:

- first sample index;
- first-sample UTC nanoseconds;
- first-sample monotonic nanoseconds;
- capture sequence number.

The 12 kHz FT assembler splits a block at the exact UTC slot boundary. Real
gaps are zero-filled and counted; overlaps are skipped and counted. A sequence
counter detects missing queued blocks.

## Decode scheduling

The FT8 early gate remains around 13.5 seconds and the FT4 gate around 6.1
seconds. It decodes an immutable snapshot in a worker while capture continues.
At the UTC boundary a complete-slot snapshot is queued for a second, deeper
wideband pass. If the gate worker is still active, the final snapshot is held
and launched immediately after that worker completes; the next slot continues
to be captured throughout.

## Runtime telemetry

Each live profiler report includes:

- blocks received;
- capture-sequence gaps;
- exact boundary splits;
- initial UTC pre-padding;
- sample gaps and overlaps;
- maximum capture-to-decoder queue latency.

A healthy run should show zero sequence gaps and normally zero gap/overlap
samples after the initial stream alignment.
