# MIND Phase 3 passive telemetry — 0.5.78-lab14

Base: 0.5.78-lab13, the last A/B-validated build where FT8/FT4 decodes are visible in the receive table.

This lab reintroduces Phase 3 only as passive telemetry:

- reliability is computed after normal decode/deduplication;
- only CRC-valid emitted decodes are sampled;
- the hot candidate/LDPC path is not modified;
- no LLR, LDPC, CRC, unpack or parser decision uses the reliability value;
- no pruning, erasure-like assist or multi-period combining is enabled.

The runtime log line is:

`FT MIND Reliability passive: decoded frames ..., symbols ..., avg reliability ...%, weak symbols ...%, QSB depth ...%`

FT4 live speed change:

- offline/WAV analysis keeps the full 4-pass FT4 DSP+ path;
- live FT4 DSP+ is capped to two passes to avoid 1.6–3.0 s slots and keep the scheduler responsive.

This is intentionally conservative. If lab14 regresses visible decodes, the cause is not the UI path and the passive telemetry block must be disabled before proceeding to erasure-like assist.
