# CW Neural R13: research and measured experiments

> Historical 0.6.2 report. MadModem 0.6.3 ships only Native R12 and Neural R13; active model files are in `modems/cw/neural/model/`. See `RELEASE_0_6_3.md`.

R13 studies the neural decoder separately from the Native R12 improvements. Native remains the default. Scientific papers below inform experiments; claims from speech recognition or third-party CW demonstrations are not MadModem benchmark results.

## Research findings and implementation choices

| Primary source | Relevant finding | Decision for MM |
|---|---|---|
| Graves et al., ICML 2006, CTC [1] | A best frame-by-frame path need not give the most probable collapsed label sequence. Prefix search sums probabilities of multiple alignments. Confident blank sections can bound search. | Implement an original bounded acoustic CTC prefix search: four hypotheses, six nonblank candidates per frame, causal consensus commits and confident-blank section boundaries. No callsign list or language model. |
| Lostanlen et al., IEEE SPL, PCEN [2] | Temporal integration, adaptive gain and compression can improve robustness to varying background/channel conditions. | Study the normalization stage before adding network size. R13 uses a causal peak-referenced envelope floor and retrains against the same frontend. This is an envelope-normalization experiment, not a full implementation of PCEN. |
| Li, Grandvalet and Davoine, ICML 2018 [3] | Penalizing departure from pretrained parameters can help transfer learning preserve previous representations. | Use a per-layer mean-square penalty towards R8 weights, plus synthetic replay. Also test updating only the input convolution and output biases. The penalty scaling is our experiment, not a replication of the paper. |
| Gulati et al., Interspeech 2020, Conformer [4] | Convolutions and attention capture different temporal context. | Relevant for a later wider-context model. It does not establish a CW speed/accuracy gain. First test cheaper search and normalization changes in the existing causal TCN. |
| Yu et al., ICASSP 2021, FastEmit [5] | Emission latency and accuracy need a sequence-level objective. The proposed method is formulated for transducers. | Do not transplant the RNN-T equations directly into our CTC training. Measure committed-word delay together with correct-word coverage; CPU cost is a separate metric. |

The prefix insertion penalty is an MM development choice: multiply by exp(-1.5) for each new token, suppressing low-probability insertion accumulation without favoring any letter, word or callsign. The production search keeps at most four suffix hypotheses and 64 uncommitted tokens. It commits shared prefixes immediately, closes remaining ambiguity after four frames with blank probability at least .99, and never revises emitted text. The audio feature hop remains 8 ms and uses past samples only. There is no file-end oracle.

The TCN has 38,970 parameters, nine local coherent frequency features, 48 channels, kernel five and dilations 1/4/16/64. The convolutional receptive field is 341 frames (2.728 s of feature positions). For computation, weights are transposed once into an immutable shared layout so output channels can be vectorized, preserving each output's accumulation order. This is an inference implementation improvement, independent from training or CTC search.

## Other CW implementations inspected

**DeepCW engine [6]** publishes an ONNX model, metadata and reference audio preparation. The inspected metadata specifies 3,200 Hz, a 256-sample FFT, 48-sample hop, 65 spectral bins over 400–1,200 Hz, log1p features and 42 output classes. This suggests evaluating a richer frequency representation in a future MM model. It does not reveal the original training corpus or complete training procedure. No DeepCW code, weights or predictions were added to MM or used as training labels.

**CWformer [7]** describes a causal Conformer with streaming state, a synthetic curriculum and varied sender segments. Its model is about 19.5 million parameters, substantially larger than MM's TCN. The useful ideas are separating sender/session conditions and testing state continuity; the repository's advertised accuracy is not a comparable result on our data. It remains a separate implementation, not a peer-reviewed CW performance guarantee.

**MorseNet [8]** provides an experimental LSTM/CTC example. It supports the relevance of sequence training, but its README does not provide a controlled RF comparison sufficient to justify substituting it for our native receiver.

## New material and split

Three additional 18-WPM ARRL recordings were downloaded, totaling approximately 118 minutes: January 7, February 4 and February 18, 2026. Their published symbol sequences matched independent measured Morse pulses exactly, with no unresolved alignment regions. January and February-4 material is additional training material; February-18 is withheld from training and development selection. Entire chunks containing unsupported symbols are omitted, including their audio. No reference symbol is silently deleted from a retained waveform.

The original two QST article groups remain training material. The March-4 transition bulletin remains development. All speed variants of an article stay together. The new February-18 recording, new generated QSO and arbitrary strings are final checks. Existing VARC excerpts are development/regression; ARRL practice recordings are clean source audio, not off-air HF reception.

Two bounded runs compare full-network adaptation and frozen-stack adaptation. Real recordings use transcript-only CTC, avoiding assumed frame-emission labels. Synthetic replay uses CTC and a supplemental frame loss. Final selection uses the actual causal search decoder and normalized runtime text; raw greedy monitoring logs are retained as diagnostics, not misrepresented as final runtime CER. Calibration uses development only. A frozen untrained-R8 search ablation distinguishes search gains from learned-weight gains.

## Reproduction

`tools/cw_validation/expand_arrl.py --out /path/to/extra-pool` downloads and audits the new archive material. It leaves copyrighted recordings/transcripts local. Source hashes and audit summaries accompany this report. `tools/cw_neural/adapt_audio.py` accepts `--pool`, `--extra-pool`, `--resume`, `--out`, `--seconds`, and `--freeze-stack`. Dependencies remain training-only NumPy, SciPy, PyTorch and ffmpeg; the app has no external ML runtime.

`tools/cw_neural/ctc_prefix.py` is the independent Python reference. C++ tests verify marginal-alignment recovery that greedy decoding misses, blank-separated repetitions, resets, quiet sections, actual RX routing and chunk invariance. GUI checks cover all engine choices and persistence. Accuracy and CPU measurement scopes are kept separate.

## Primary references

1. Graves, Fernández, Gomez, Schmidhuber. **Connectionist Temporal Classification: Labelling Unsegmented Sequence Data with Recurrent Neural Networks.** ICML 2006. https://www.cs.toronto.edu/~graves/icml_2006.pdf
2. Lostanlen et al. **Per-Channel Energy Normalization: Why and How.** IEEE Signal Processing Letters, 2018/2019. Author manuscript: https://www.justinsalamon.com/uploads/4/3/9/4/4394963/lostanlen_pcen_spl2018.pdf
3. Li, Grandvalet, Davoine. **Explicit Inductive Bias for Transfer Learning with Convolutional Networks.** ICML 2018. https://proceedings.mlr.press/v80/li18a.html
4. Gulati et al. **Conformer: Convolution-augmented Transformer for Speech Recognition.** Interspeech 2020. https://www.isca-archive.org/interspeech_2020/gulati20_interspeech.pdf
5. Yu et al. **FastEmit: Low-latency Streaming ASR with Sequence-level Emission Regularization.** ICASSP 2021. https://research.google/pubs/fastemit-low-latency-streaming-asr-with-sequence-level-emission-regularization/
6. e04. **deepcw-engine** and published model metadata, inspected 2026-10-17. https://github.com/e04/deepcw-engine and https://github.com/e04/deepcw-engine/blob/main/model.onnx.json
7. parsimo2010. **CWformer**, inspected 2026-10-17. https://github.com/parsimo2010/CWformer
8. netom. **MorseNet**, inspected 2026-10-17. https://github.com/netom/MorseNet
9. ARRL. **18 WPM Code Archive** and **Code Practice Files**. https://www.arrl.org/18-wpm-code-archive and https://www.arrl.org/code-practice-files
