# Neural R13 — training and provenance

MadModem 0.6.3 ships one neural CW receiver: **Neural R13**, model
`mm-cw-r13-search`, alongside **Native R12**, the default. RX A and RX B use
the selected engine independently. The band skimmer stays native. TX audio
and serial KEY are independent of the RX engine.

The model is an independently trained, causal temporal CNN: four layers,
48 channels, kernel 5, dilations 1/4/16/64, nine coherent frequency-bank
features, 32 ms windows and 8 ms hops. Its causal peak envelope releases over
0.5 seconds with a 20% amplitude floor. Acoustic CTC prefix search retains
four hypotheses, with no dictionary or callsign correction. Inference is
standard C++17; it requires no external ML runtime or model download.

The sole deployed header, training checkpoint and model card are in
`modems/cw/neural/model/`. The 0.6.3 cleanup preserves the R13 numerical
weights, calibrated checkpoint and decoding parameters. The header namespace
was standardized, so the model card records the new header hash. Historical
training checkpoints and rejected candidates have been removed from this
source package; their audit reports and training scripts remain. Use the
corresponding older source release for historical checkpoints.

## Further training

Install the training-only dependencies in `requirements.txt`, then run a
separate experiment directory:

```sh
python tools/cw_neural/adapt_audio.py --pool /path/to/audited-pool \
  --resume modems/cw/neural/model/checkpoint.pt --floor .2 \
  --out /path/to/experiment --seconds 900
```

`train.py` defines the model and exporter; `adapt_audio.py` provides labelled
audio adaptation, `ctc_prefix.py` mirrors the C++ search, and `select_search.py`
documents the R13 development-only selection. Do not overwrite the released
header with an unvalidated checkpoint. Compare frozen candidates on disjoint
audio and exact QSO/callsign/RST content before promotion. Historical scripts
that compare legacy models require probes/checkpoints from those releases.

Enable `BUILD_TESTING` and `MADMODEM_BUILD_CW_TESTS` to build the neural feature
probe, native regression and full-engine routing checks. `CwNeuralProbe`
accepts `features` or `audio` and always runs R13. `CwRouteProbe` accepts only
`native` or `neural_search`.

R13 improves the previous neural receiver, but Native remains better on the
clean QSO/ARRL sets in the frozen comparison. See
`docs/architecture/CW_NEURAL_R13_RESULTS.md` for measured results and limits,
and `CW_NEURAL_R13_RESEARCH.md` for papers and source-backed research. These
are historical reports, not evidence of a universal neural advantage.
