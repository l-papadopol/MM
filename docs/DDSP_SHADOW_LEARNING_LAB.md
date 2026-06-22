# MIND training-learning lab

MIND is an experimental background learner based on MadModem's internal MIND Eigen MLP engine. It is intentionally fail-closed.

Current phase:

- The production MIND panel is FT8/FT4-only.
- In non-FT modes the MIND tab is hidden and runtime MIND is forced Off.
- CW text is produced by ggmorse only; MIND is FT-only in production builds.
- RTTY text is produced by the classical matched-filter/Baudot chain.
- FT8/FT4 use MIND only as a candidate-priority/ranker helper.
- MIND does not key TX, control CAT/PTT, feed AutoQSO or accept AI-only text.

This version does not inject AI-only decodes into the QSO list, AutoQSO, logbook or TX scheduler.


## Neural matrix display

The MIND tab shows a rectangular live activity map. Each 3x3 pixel square represents an input, hidden or output neuron from the current shadow network. The widget is diagnostic only: it does not control decoding, PTT, CAT or AutoQSO.

## Training completion

The progress bar reports validation decode-success percentage over the rolling validation window. `Active` mode may be selected by the operator, but it remains guarded by model-readiness checks and the deterministic FT validation chain.

## Persistent MIND model

MIND saves its Eigen MLP checkpoint in the writable MadModem application data directory, inside a `MIND` subdirectory. The current lab model file is `mind_native_ft_eigen_v2.model`, with companion statistics in `mind_stats.json`.

The checkpoint is written through a temporary file and then atomically renamed over the previous checkpoint, so an interrupted save should not destroy the last good model. On startup MadModem loads the checkpoint if the architecture and model version match; otherwise it starts from a fresh model.

The active production architecture is the FT candidate ranker:

```text
58 × 8 candidate features → Conv2D-style ranker → success probability
```

The MIND tab reports FT ranker progress and sample balance for the candidate dataset.

## CPU-only backend decision

MIND uses the CPU Eigen backend only. GPU/OpenCL acceleration is intentionally not part of this lab build. Training is deferred outside timing-critical FT decode/auto-test sections and is limited by the idle budget control so the classic decoders remain the real-time priority.

### MIND FT candidate ranker
- The active MIND path is now the FT8/FT4 candidate ranker.
- The checkpoint is `mind_ft_candidate_ranker_v1.model`.
- The dataset is `mind_ft_ranker_samples_v1.dat`.
- Positive samples are candidates that become accepted FT decodes.
- Negative samples are candidates rejected by the classical decoder chain.
- MIND remains fail-closed and cannot key TX, CAT, PTT or AutoQSO.

