# MadModem 0.6.4 source R9

## Changes

- Present already validated FT8/FT4 pass results during passive reception instead of waiting for the whole job. No candidate, LDPC/OSD, cancellation, drift or recovery budget changes.
- Keep presentation batches separate from canonical batches. Passive previews do not select Auto CQ candidates, enqueue callers, arm TX, update the TX report context, or finish response waits. Canonical results still reach the sequencer even when a matching row was already displayed. Active-QSO progress retains the R8 behavior.
- Keep the clock pinned above QRZ with analogue and digital displays side by side. Fit the FT message table to its six rows and the configured row/font sizes.
- Adapt QRZ text height to its contents within 48–100 px. Preserve the station information location and account settings/persistence.
- Give the RTTY scope a 190 px preferred size, bounded by 160–230 px; retain vertical scrolling when a small window cannot fit all controls.
- Log the actual FT8/FT4 mode when the sequencer stops.

## Evidence from the supplied logs

Two FT8 logs showed first gate results about one second before the slot boundary and final boundary results averaging 1.32 and 1.40 seconds after it (max 1.54 seconds). FT4 boundary results averaged 0.31 seconds after the boundary (max 0.41 seconds). GUI row updates took tens of milliseconds, not seconds. A signal's tones end before the full slot ends, so perceived delay after the audio can be longer.

The logged no-response stops followed the final listening period; the logs alone do not prove a missed automatic reply. Several manual selections occurred 3–6 seconds into a TX period. This revision does not change the established manual late-start policy or response-wait rules.

## Validation

See the measured comparison below. Synthetic audio does not replace replay of the user's actual captured RF audio. No real audio recording was supplied for this task. Native Windows/macOS runtime and DPI rendering still require platform testing.

### Measured passive publication latency

Same deterministic six-signal mixture, Linux Qt 5 Release, three sequential runs per version without a concurrent build. Times are milliseconds from job submission, **not** measured from the end of an RF slot. Medians:

| Mode | R8 first result | R9 first result | R8 final batch | R9 final batch | Canonical decodes |
| --- | ---: | ---: | ---: | ---: | --- |
| FT8 | 881 | 147 | 881 | 800 | 6 / 6, identical |
| FT4 | 418 | 109 | 418 | 383 | 6 / 6, identical |

Three passive pairs and one active-QSO pair for each mode yielded identical canonical message/frequency/SNR/DT sets. Active-QSO progress retained the established early publication path; the passive speedup is earlier presentation, not a new decoding algorithm. Full-job timing varies with system load and is not a claimed DSP throughput improvement.

Initial uncontrolled R9 runs recovered five of the six signals. The subsequent controlled comparisons all recovered six. Existing wall-time recovery limits are unchanged; these limited synthetic tests do not establish equivalence under every load or on real RF recordings.

### Other checks

- Linux Qt 5 Release application build with Hamlib 4.5.5.
- Release guard suite: 8 checks; FT guard suite: 6 checks.
- FT sequencer self-test and GUI reply-window regression: final RR73/73, stale timeout, real no-response, QRZ correspondent update.
- Presentation GUI regression for both modes: immediate display, no preview-triggered TX/report-context change, canonical reply, anti-QRM and Auto CQ isolation; dense 95-row preview/final deduplication.
- CW two-engine selector/settings regression and unclipped FT clocks at 1024×768, 1280×900 and 1600×1000.
- Same 426 px sidebar at 1440×1080: FT scroll overflow 66→0 px, clock height 214→168 px, TX table 192→151 px, QRZ 159→113 px. RTTY scope fully visible at 190 px; Mark/Space captions checked visually.
- Decoder code outside asynchronous publication, FT sequencer/session/slot scheduler and AudioEngine compared byte-for-byte with R8 and unchanged.

### Reproduction

`tests/FtPublicationProbe.cpp` uses the same six synthetic signals for passive reception (no argument) and active QSO (`active`). `tests/FtPresentationUiProbe.cpp`, `tests/ModePanelUiProbe.cpp`, `tests/FtResponseUiProbe.cpp` and `tests/CwEngineUiProbe.cpp` are manual Qt application probes. Like the existing probes, compile them against application objects in place of `main.cpp` with GCC `-fno-access-control`; run GUI probes with `QT_QPA_PLATFORM=offscreen` on Linux. They are not hardware radio tests. Canonical settings and build flags must be identical for A/B comparisons.
