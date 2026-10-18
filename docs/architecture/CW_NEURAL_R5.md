# CW RX engine selection — R5 experiment

Version: 0.6.2, source revision `0.6.2-source-r5-cw-neural-engine`.

## Operator behaviour

CW settings now offer **Native** (default) and **MM Neural CTC (experimental)**.
The choice is saved and applies to selected receivers A and B. Their neural
states and tone markers are independent. The wideband skimmer remains native;
CW audio TX and serial KEY TX remain independent of the RX engine choice.

Selecting an engine resets pending decoder state but preserves received text.
Clearing a receiver clears its text and neural state. Audio discontinuities,
sample-rate changes, changing a marker and resuming after local TX discard
pending neural context. Native bandwidth, AFC and WPM controls do not configure
the neural feature bank. Native signal diagnostics remain native measurements,
not neural confidence scores. Unknown saved engine names fall back to Native.

## Independent implementation

DeepCW was studied as a reference for sequence recognition from audio. This
implementation uses a different, much smaller causal temporal CNN: four layers,
48 channels, kernel 5, dilations 1/4/16/64, followed by a 42-class head and CTC
best-path collapse. It has 38,970 parameters. Nine coherent magnitude channels
surround each selected tone, using a 32 ms window and an approximately 8 ms hop
(rounded to integer samples). The past receptive field is 341 frames, about
2.728 seconds. This is context length, not a fixed output delay.

Inference is C++17 with compiled weights. Python, PyTorch, GPU, Internet access
and downloads are unnecessary on the radio computer. The optional Python tools
are for offline training and evaluation. No DeepCW source, ONNX model, tensor
weights, recordings or teacher predictions are included or used for training.
The package and original model are released under GPL-3.0-or-later, consistently
with this existing MadModem source distribution. This does not assert that a
reimplementation automatically permits relicensing any other project's code.

## Training performed

Training began from random weights with generated Morse audio and known text.
The 3,150 training records comprise clean 20 WPM messages, mildly impaired
16–26 WPM messages, impaired 12–36 WPM messages, and 150 noise/carrier negatives.
Noise, level, mild fading, frequency drift, timing jitter and adjacent carriers
are randomized. Frame supervision is combined with CTC loss. The first phase
ran 3,000 steps; a second 1,800-step phase refined the same independently trained
model with a larger CTC loss weight. The shipped checkpoint is the best selected
using a separate 80-record validation set: CER 12.74%, 25 exact messages, 730
reference characters. This validation set was used for model selection and is
not the final test set.

The model card records its seed, architecture, checkpoint lineage, validation
history and exported header SHA-256. `tools/cw_neural/README.md` gives the actual
two-phase commands. Numerical reproduction can depend on library versions.

## Separate synthetic test

The model was frozen before testing 60 additional messages, seeds
1,900,000–1,900,059, outside both training and model-selection validation.
Waveforms were evaluated through the C++ audio path at 8, 16, 44.1 and 48 kHz.
The complete references and predictions are in `CW_NEURAL_R5_RESULTS.json`.

| Measurement | Result |
| --- | --- |
| Reference characters, including spaces | 523 |
| Character error rate, including spaces | 14.15% (74 edits) |
| Exactly correct messages | 26 / 60 |
| Maximum Python/C++ logit difference, 700 random frames | 0.00000954 |
| Output with chunks of 17, 997 and 4,096 samples | Identical |
| Eight seconds each of silence, white noise, steady carrier | No text in these three tests |

The clean 20 WPM chunk test decoded `CQ CQDEIZ6NNHTEST73` for reference
`CQ CQ DE IZ6NNH TEST 73`. Missing word spaces are a known defect, not excluded
from the reported error rate. Other records show character insertions and
substitutions. The integration regression separately checks clean character
routing, independent RX B, state changes and text retention; its character
assertion ignores spaces and does not certify full-transcript accuracy.

About 403 seconds of test audio took 2.63 seconds across probe processes on the
build host, including process startup. This is an observation on this machine,
not a performance guarantee for other computers or a comparison with Native.

## Radio recordings and next training stages

The application does **not** train while listening. Ordinary reception uses
fixed weights. Blindly treating its predictions as truth would reinforce errors.
Offline synthetic training provides an initial model; real radio recordings
are needed to establish practical performance and may support later fine tuning.

A suitable next dataset consists of short, original audio excerpts with checked
transcripts and recorded conditions: speed, signal level, interference, fading,
filtering and sample rate. Include empty/noise-only intervals. Keep entire
recording sessions, stations and conditions in separate training, validation
and final-test groups; overlapping cuts must not cross those boundaries.
Use only the training group for weight updates and the validation group for
selection. Freeze the model before measuring the final test group, and compare
Native and neural on the same recordings. A second decoder can assist
transcription, but its guesses alone are not ground truth.

Real-world sensitivity, hand-keyed Morse, long messages, very low SNR, severe
QSB/QRM, prosigns and speeds outside 12–36 WPM remain unvalidated. No superiority
over Native or DeepCW is claimed. Keep Native as the everyday default until
those measurements justify changing it.

## Build and checks

The Linux Qt application was built with the included original model. Existing
native DSP and CwDecoder integration regressions passed. The engine-specific
regression checks A/B routing, unknown-engine fallback, switching, retained
text, local-TX resume, audio gaps and receiver clearing. A Qt UI probe exercises
selection, switching CW/FT8/CW, persistence across window reload and return to
Native. Localized help and UI translations are included for all six languages.
The optional Qt compiled-help generator failed on this host; the supported
embedded multilingual HTML help remains available. Native Windows/macOS builds
and real-radio operation remain unverified here.

To build the new engine regression and probe after configuring the normal Qt
project with `-DBUILD_TESTING=ON -DMADMODEM_BUILD_CW_TESTS=ON`:

```sh
cmake --build build --target madmodem_cw_engine_regression madmodem_cw_neural_probe
ctest --test-dir build -R '^madmodem_cw_engine_regression$' --output-on-failure
python tools/cw_neural/evaluate.py --probe "$PWD/build/madmodem_cw_neural_probe" \
  --model modems/cw/neural/model --out neural-evaluation --records 60
```

The evaluation command needs the training-only Python dependencies. The normal
MadModem executable does not.
