# FT8 / FT4 RX drift recovery — source R6

Public application version: 0.6.3. Baseline: the complete R5 source archive.
CW Native R12 / Neural R13, updater, audio transport and existing standard
FT decoder algorithms are retained.

## Operator controls

Settings → FT8 / FT4: experimental RX drift compensation is OFF by default.
The absolute slope limit is 0.25–5 Hz/s, initially 2 Hz/s. The operator selects
the signal with the RX marker. Runtime-log rows show accepted Hz/s estimates,
Costas sync evidence, attempts, recovered messages and elapsed recovery time.
Sync evidence is a ranking statistic, not a calibrated probability.

Settings → QRZ.com: username and masked password replace the account button
in the main station-information panel. The panel and Open QRZ page remain in
place. Both credentials persist in the local settings.mad profile and are
loaded by the panel at startup; XML login occurs when a station is selected.
The profile stores the password without encryption. POSIX profile permissions
are restricted to the owner when a password is saved. Clearing the password
removes its stored key. Session keys are never persisted. Cancel preserves the
previous account; changing accounts cancels requests and clears the session/cache.
Existing TLS POST handling keeps credentials out of URLs and runtime logs.

## DSP implementation

1. All established standard passes run first, with the existing full-band
   discovery, sync gates, metrics, LDPC/OSD policy and cancellation.
2. Complete boundary/offline jobs can run one local recovery if the selected
   correspondent has not already been decoded. Early streaming gates do not.
3. A 321-tap low-pass forms a 500-Hz complex baseband around the selected signal.
   Known Costas tones are correlated independently in each sync block. A fixed
   timing/frequency grid tests a linear model through these block scores.
   Midpoint frequency spans ±25 Hz around the local search centre. The initial
   timing grid is 0.10–1.10 s for FT8 and 0–0.816 s for FT4; normal per-candidate
   refinement then applies. This is intentionally a bounded local acquisition.
4. The best nonzero slopes must improve sync evidence over a stationary model.
   An analytic COPY of the input, made once with FFT/Hilbert processing, is
   multiplied by exp(-j*pi*drift*(t-reference)^2). This preserves FSK spacing
   and corrects the phase continuously, including inside symbols.
5. At most three hypotheses enter the existing mode-specific candidate decoder.
   FT8's additional attempts exclude metric/OSD fallback. Normal LDPC, CRC,
   unpacking and message checks remain mandatory. Previously accepted results
   remain in the output. The stage stops after one new valid message.
6. These new messages are added AFTER all standard SIC passes. They are never
   subtracted from original audio using a stationary reference. A future
   drift-aware SIC must reconstruct the identical drifting phase trajectory.
7. A recent CRC-valid estimate for the same selected DX can predict the next
   LOCAL SEARCH centre for up to 60 seconds. Changing DX, mode, or moving the
   marker away makes that prediction inapplicable. The prediction sends no CAT
   command and directly changes neither marker nor TX frequency. Existing
   operator-selected AutoQSO/follow-RX policies continue to handle normal decodes.

The recovery uses the existing coordinator, cancellation checks and configuration
mutex. UI changes coalesce without blocking the capture-owner thread. Soft wall
budgets are 280 ms FT8 / 220 ms FT4, with a search allocation of 60%; at most
three ordinary candidate attempts bound additional FEC work. A running atomic
FFT/FEC operation or OS preemption can cross the soft time budget. On a busy or
slow PC the stage can stop early and recover fewer signals.

This is per-signal linear correction. It does not estimate nonlinear drift,
lock to the QO-100 beacon, calibrate the LNB, adjust the TX uplink, or require a
GPU. A narrow SSB audio input cannot supply a beacon outside its passband.

## Verification on Linux / Qt 5

Release application build: PASS. CTest settings UI, runtime translations and FT
sequencer: 3/3 PASS. Existing FT guard suite: 6/6 PASS. Release/localization/help
suite: 7/7 PASS.

The existing discovery, per-candidate decoding and subtraction function bodies
are byte-identical to R5 for BOTH modes. The 86-buffer cancellation SHA-256 is
unchanged: f62cae7818a7ed26c129f9fe87a79a6a02177050bf706e02919176de0bea430b.
Two serial six-station comparisons per revision recovered the same six messages
in each mode, R5 versus R6 with drift disabled. R5 DSP was compiled with the R6
header layout for linkage compatibility; unused drift controls were inert.
No speed improvement of the standard decoder is claimed. One initial concurrent
verification run returned 5/6 FT8 results; the serial comparisons returned 6/6.
Live recovery budgets and timings remain sensitive to machine load.

The optional independent SciPy Hilbert generator applies positive/negative
linear chirps to the MSHV TX waveform before adding seeded noise. It uses the
complete `IZ6NNH LZ2KSQ RR73` payload, 1033-Hz midpoint, and two noise levels.

| Synthetic inputs | Standard decoder | Optional recovery enabled |
|---|---:|---:|
| FT8, ±1 and ±2 Hz/s, 8 cases | 0/8 | 8/8 |
| FT8, zero drift, 2 cases | 2/2 | 2/2, recovery skipped |
| FT4, ±2 and ±5 Hz/s, 8 cases | 8/8 | 8/8, recovery skipped |
| FT4, zero drift, 2 cases | 2/2 | 2/2, recovery skipped |
| Noise-only, one per mode | — | 0 spurious messages |

All 26 final checks pass: 20 complete standard/enabled comparisons, two verified
next-slot predictions with DX-context isolation, two noise-only recoveries and
two disabled-option checks. Separate direct recovery probes also decoded the
FT4 chirped fixtures; these show the compensation path works, while the complete
comparison above establishes no additional FT4 messages on this strong sample.
Raw final outputs are in FT_DRIFT_R6_RESULTS.json. These are synthetic checks,
not a real QO-100 sensitivity measurement or a universal drift tolerance limit.

GUI probe: retained main station panel, absent main account button, masked fields,
profile save/reload/restart, Cancel, deferred-page preservation, account-change
session/cache reset and password removal all PASS. The previous FT last-reply,
final-73, stale-timeout and active-DX QRZ scenarios also PASS. Actual authenticated
QRZ access needs an operator's XML account and was not exercised here.

Reproduce the optional GCC/Clang Linux probes after a Release app build:

```
python3 tools/run_ft_response_probes.py /absolute/path/to/build
python3 tools/run_ft_drift_probes.py /absolute/path/to/build
```

The second probe requires NumPy/SciPy for test fixture generation only; the
application has no new Python or SciPy runtime dependency. Tests write fixtures,
settings and internal GUI captures under the build directory, outside sources.

Native Windows/macOS execution and real QO-100 recordings remain to be tested.
A client already at public version 0.6.3 needs manual installation of this source
revision; automatic updates still require a higher public version.
