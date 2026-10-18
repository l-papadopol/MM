# CW neural receiver refinement — source R6

Version: 0.6.2; revision `0.6.2-source-r6-cw-neural-refinement`.
The saved UI selector remains **Native** (default) / **MM Neural CTC
(experimental)** for selected RX A/B. The wideband skimmer remains native.
CW audio and serial KEY TX are unchanged. This is an independently trained MM
model, not the DeepCW model or an assertion of equivalent performance.

## Changes and origin

The C++ architecture remains the 38,970-parameter causal TCN described in
`CW_NEURAL_R5.md`: nine coherent audio features, 32 ms windows, approximately
8 ms hop, four 48-channel layers and CTC best-path collapse. The runtime model
identifier is now `mm-cw-tcn-ctc-v2`. Inference needs no network, external ML
runtime, Python or GPU.

R5 produced false letters on tightly filtered receiver noise, despite passing
a broad white-noise test. Controlled bandpass-noise tests reproduced the issue.
The first refinement phase used 8,000 independent generated messages and 1,600
negative records, including receiver-like filters, continuous carriers and
noise bandwidths down to 70 Hz. Positive messages included receiver filters
100–1,200 Hz wide and more word boundaries. Blank/space supervision and the CTC
loss weight were increased.

An initial frozen diagnostic comparison then exposed a clean-signal regression.
A second phase balanced 800 augmented messages, 200 negatives, 1,200 clean or
lightly impaired messages with variable leading/trailing silence, and 40 silent
records. Both phases completed their requested runs, 3,600 and 1,600 steps.
Model-selection data chose step 2,800 of phase one and step 1,200 of phase two.
Those sets were separate from the final test below. No final-test-driven weight
update followed that test. The model card records seed ranges, losses, lineage,
validation and the exported header SHA-256. The R5 and selected intermediate
checkpoints are retained under `tools/cw_neural/` for reproducibility.

Training uses original generated audio and known labels. No DeepCW source,
model tensors, audio data or teacher predictions are used. No unverified
WebSDR decoder output is used as a target. Original code and model artifacts
remain GPL-3.0-or-later, as in this MadModem source distribution.

## Frozen comparison with R5

Both compiled C++ receivers processed the same previously unused messages at
8, 16, 44.1 and 48 kHz. There are 120 records in each positive group, with seed
ranges starting at 16,900,000 / 17,900,000 / 20,900,000. CER is Levenshtein
character error rate, including spaces. References, predictions, exact counts,
per-record errors and probe hashes are in `CW_NEURAL_R6_RESULTS.json`.

| Independent test | R5 CER | R6 CER | Exactly correct messages R5 / R6 |
| --- | ---: | ---: | ---: |
| Ordinary generated messages, 1,038 characters | 12.72% | 9.92% | 45 / 37 of 120 |
| Receiver-filtered messages, 1,357 characters | 74.58% | 23.95% | 4 / 16 of 120 |
| Clean/lightly impaired with variable silence, 1,061 characters | 30.82% | 9.90% | 34 / 33 of 120 |
| Seven longer messages, 413 characters | 31.72% | 26.39% | See complete JSON |

Lower aggregate CER does not mean that every message improves: the number of
completely correct ordinary messages decreased. Word spaces remain a major
weakness and long messages still accumulate errors. The legacy clean 20 WPM
chunk example decodes `CQCQDEIZ6NNHTEEST73`, with a duplicated E and missing
spaces, for reference `CQ CQ DE IZ6NNH TEST 73`. This known regression is
reported explicitly; it must not be disguised as a passing accuracy check.

Thirty-six separate 10-second negative records cover noise widths 0 (broad),
70, 100, 150, 200, 300, 500, 800 and 1,200 Hz, some with unkeyed carriers.
R5 produced 164 characters in 15 records; R6 produced no characters in this
test. This finite result is not a guarantee against false output in all noise.

## WebSDR observations

SO8OO (`https://sdr.so8oo.net/`) supplied two mono PCM16 8 kHz captures:

| Capture | Duration | R5 output | R6 output |
| --- | ---: | --- | --- |
| 14,025 kHz, narrow CW filter | 39.456 s | 42 E/I/S letters | Empty |
| 14,005 kHz, CW-wide, near a displayed station label | 75.504 s | Empty | Empty |

The narrow-filter recording was acquired in the earlier diagnostic session;
the second was acquired during this refinement. The initial recording has SHA-256
`f2b16a214f0df085be7ba3fd18ead9cfd5e584df28a416eb461b608930ee73f6`;
the second has SHA-256
`7a423e48c77679af9e414593df21adf3b2ec5a8b4365f538efb5f3af8bc097bb`.
Predictions are compared in the JSON, with `reference: null`. Neither recording
has a verified CW transcript. Empty output can indicate rejection of noise or
a missed weak signal; these captures establish neither a real-radio CER nor
improved sensitivity. They were not used to train weights. A displayed station
label alone does not certify that the local receiver hears that station.

The attempted alternative receiver at the University of Graz timed out; no
usable audio from it entered the experiment. Further radio training needs
recordings with checked transcripts, and whole recording sessions must remain
separate across training, model selection and final testing.

## Checks and scope

Release checks passed: Linux Qt/Hamlib application build; existing native DSP
and CwDecoder integration regressions; A/B engine routing and lifecycle checks;
Python/C++ parity (maximum logit error 0.00000763); chunk sizes 17/997/4096
producing identical text; and the UI selector/persistence probe. The optional
compiled Qt Help generation failed on this host; multilingual HTML help remains
available through the existing fallback. The
engine integration test verifies that selected A/B output matches the direct
neural receiver, including spaces, and that switching, clearing, TX resume and
capture discontinuities handle state correctly. It is not an accuracy claim.
The independent comparison above supplies the transcript accuracy measurements.

Native remains the everyday default. Native Windows/macOS builds, hand-keyed
Morse, severe QSB/QRM, speeds outside 12–36 WPM and real-radio sensitivity remain
unverified. No improvement over the Native receiver or DeepCW is claimed.

Training and comparison commands are in `tools/cw_neural/README.md`. The normal
radio computer uses fixed compiled weights and does not train during reception.
