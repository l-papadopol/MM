# MIND training-learning lab

MIND is an experimental background learner based on MadModem's internal MIND Eigen MLP engine. It is intentionally fail-closed.

Current phase:

- FT8/FT4, RTTY and CW decoders continue to be the only trusted decoders.
- MIND observes receive audio feature snapshots.
- When the classic decoder emits confirmed text, MIND uses that text as a training label.
- The model trains only in short idle slices.
- A checkpoint is saved under the application data directory.
- MIND Assist is selectable as `Off`, `Training` or `Active`; `Active` remains fail-closed and never accepts FT text without CRC/unpack/parser validation.

This version does not inject AI-only decodes into the QSO list, AutoQSO, logbook or TX scheduler. The next phase can add guarded AI-assisted candidate proposals after we have validation reports from real radio and WAV tests.


## CW manual label teaching

CW does not provide a mathematical truth label like FT8/FT4 CRC-valid messages. For this reason MadModem must not train MIND from every raw CW character emitted by the classic decoder. In the lab build, CW training is conservative:

1. The operator listens/inspects the CW terminal.
2. If a fragment is known or confidently corrected, the operator opens the MIND side tab.
3. In **Manual label teaching**, choose `CW`, enter the corrected text, for example `CQ IZ6NNH K`, `5NN TU`, `TNX FER QSO`, and press **Teach corrected text**.
4. MIND uses the most recent CW audio features as an operator-confirmed high-trust label.

This is only a first lab workflow. A later build should store a short audio ring buffer and let the operator select the exact RX time span before teaching the corrected phrase.


## Neural matrix display

The MIND tab shows a rectangular live activity map. Each 3x3 pixel square represents an input, hidden or output neuron from the current shadow network. The widget is diagnostic only: it does not control decoding, PTT, CAT or AutoQSO.

## Training completion

The progress bar reports validation decode-success percentage over the rolling validation window. `Active` mode may be selected by the operator, but it remains guarded by model-readiness checks and the deterministic FT validation chain.

## Persistent MIND model

MIND saves its Eigen MLP checkpoint in the writable MadModem application data directory, inside a `MIND` subdirectory. The current lab model file is `mind_native_ft_eigen_v2.model`, with companion statistics in `mind_stats.json`.

The checkpoint is written through a temporary file and then atomically renamed over the previous checkpoint, so an interrupted save should not destroy the last good model. On startup MadModem loads the checkpoint if the architecture and model version match; otherwise it starts from a fresh model.

The active lab architecture is:

```text
464 → 128 → 64 → 174
```

The MIND tab reports validation success and readiness separately. Readiness is validation success weighted by the amount of validation history, so a model does not become ready merely because it guessed a few early samples correctly.

## CPU-only backend decision

MIND uses the CPU Eigen backend only. GPU/OpenCL acceleration is intentionally not part of this lab build. Training is deferred outside timing-critical FT decode/auto-test sections and is limited by the idle budget control so the classic decoders remain the real-time priority.

### MIND v2 native FT candidate training
- Replaced the old FT audio/text fingerprint lab path with native FT8 candidate samples.
- MIND now learns from `dataMagnitudes[58][8]` flattened to 464 inputs and the CRC-valid 174-bit LDPC codeword as target.
- The checkpoint is now `mind_native_ft_eigen_v2.model`; old v1 fingerprint checkpoints are intentionally ignored.
- UI now separates bit accuracy, message/codeword exact accuracy and readiness.
- No MIND inference/training runs inside the FT decoder timing-critical path; the decoder only emits queued gold-label samples after LDPC+CRC+unpack succeeds.
- MIND remains shadow-only and cannot key TX, CAT, PTT or AutoQSO.

