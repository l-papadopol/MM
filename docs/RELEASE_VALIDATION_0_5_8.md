# MadModem 0.5.8 — correction and validation report

Date: 2026-08-18  
Source: `MadModem_0_5_8_FULL_SOURCE`  
Starting point: known-good FT8/FT4 source carried by the 0.5.78/0.5.79 recovery

## Release decision

0.5.8 is a runtime-hardening release. The validated FT8/FT4 candidate finders,
LDPC gate and proven sequencer topology were not redesigned. R10 extends the
already validated active-QSO deadline orchestration from FT8 to FT4: a bounded
focused pass is attempted before wideband work and the normal wideband path is
left unchanged on a miss. Other corrections concentrate on ownership,
synchronization, input validation, bounded queues, persistence, CAT/PTT safety,
localization, documentation and feature gating.

The source guards prove that the three sensitivity-critical blocks still match
the validated checkpoint:

| Protected FT block | SHA-256 | Result |
|---|---|---:|
| FT8 live candidate finder | `2d4ca13f3ea884157d8343aaccff16a38b09351ce045bd156ef4f9c1afdc083f` | PASS |
| FT4 live candidate finder | `3b160cfb8f022a74282c6907a2e470e2615f6792a81bb2af95baa6191fff2af3` | PASS |
| FT8 LDPC ghost-candidate gate | `6e7b4f9c15b21a01f3351442c08c916a7f57f49686a98dfc7acb2296adee1e36` | PASS |

The complete `Ft8RxDecoder.cpp` necessarily changed around the protected core
to fix synchronization, streaming resampling and FT4 active-QSO scheduling. Its
R10 source hash is
`4bfc4f48a0438f1ce2e724152c1fe13477264f0e77a6bc916628a6c2ae27c3ca`.

## R13 native MSK144 and Q65 completion

- Q65A/B/C/D RX/TX is mandatory in every normal build. Its in-tree radix-2
  spectral front end performs synchronization, time/frequency/drift refinement,
  65-ary soft metrics, QRA/CRC validation, unpacking and parity-separated
  averaging without an optional FFT/backend branch.
- MSK144 searches the complete UTC period, refines time and centre frequency,
  decodes full LDPC/CRC frames and MSK40 hashed short frames, and synthesizes a
  continuous-phase waveform at the selected audio centre.
- MSK144 and Q65 share one first/second-period UTC TX scheduler. RX remains
  active while armed; a missed boundary advances to the next matching period
  rather than truncating a protocol frame. Coding and waveform synthesis occur
  at arm time, outside the UTC boundary path.
- Generated-audio native regressions cover Q65A/B/C/D and both MSK144 frame
  families. A source guard rejects optional, unavailable or competing runtime
  paths.
- `pack_unpack_msg77.cpp` and `nhash.cpp` are compiled once in the common
  weak-signal codec target. FT4/FT8, MSK144 and Q65 use the same process-wide
  lock for its mutable hash state.

## R12 public product README

- The GitHub homepage describes only complete, usable operator features.
- Implementation and regression details remain in developer documentation and
  do not interrupt the public presentation.
- The documentation audit rejects unfinished-feature and internal validation
  wording if it is reintroduced into `README.md`.

## R11 public README and source-root cleanup

- Removed the internal screenshot capture checklist from the public README;
  screenshots will be added only when the final image files are available.
- Removed obsolete one-off validation dumps from the package root and the old
  duplicate RTTY catalog. Current evidence is consolidated in this document,
  while reproducible checks remain in `scripts/`, `tools/` and `tests/`.
- The only first-party `.txt` files left outside bundled third-party sources are
  the version and source-revision markers required by CMake and packaging.

## R10 operator UI, documentation, localization and FT4 parity

- FT status banners and slot state now expose only the immediate action/state;
  verbose explanations remain available as tooltips and contextual help.
- Runtime operator messages and dialogs use stable translation keys. A new
  guard rejects untranslated dynamic labels and oversized FT copy.
- The word-by-word translation fallback was removed. Six dictionaries contain
  1824 canonical keys in the same order, preserve every Qt placeholder and pass
  semantic-mixing checks.
- The project README is a concise public feature overview. Release notes, the
  documentation index and 36 high-risk localized help pages were rewritten and
  are generator/audit owned.
- FT4 now receives the same bounded active-correspondent-first scheduling as
  FT8. Both callsigns must validate; failure immediately resumes the existing
  full-band pipeline, so no second decoder or fallback topology was introduced.

## R5 complete UI theme correction

R5 removes the split ownership that let Avionica colours and borders leak into
the other appearance choices. `CockpitTheme` is now the single owner of five
complete themes: Avionica, Qt Default/Classic, Hacker Green, Classic Dark and
High Contrast. Each defines every application surface, selection, disabled
state, semantic status colour and map overlay colour.

- Avionica retains its intentional three-part aircraft-instrument outer bezel;
- every non-Avionica theme uses a single quiet palette-derived outer outline;
- Qt Default/Classic is a deterministic light theme, with dark text on light
  editors, buttons, tables, menus and dialogs;
- surviving dialogs, combo popups, map overlays and custom-painted controls are
  repolished and repainted during a live theme switch;
- fixed orange/black styles were removed from Logbook, settings popups, the QSO
  map container, FT diagnostics, CW/RTTY status text and waterfall controls;
- the RF waterfall remains an intentionally dark instrument surface in every
  theme, while its controls follow the active palette.

`madmodem_ui_theme_integrity_guard` verifies all five theme definitions, rejects
known local colour/border leaks and enforces a minimum 4.5:1 contrast ratio for
normal, editor, button, selection, semantic and map text. The lowest measured
ratio is 4.76:1 (Qt Default warning text). Theme work does not touch any FT DSP
path; the protected candidate and LDPC blocks retain the hashes shown above.

## R4 FT TX and RTTY waterfall correction

The captured live log showed that the FT waveform was not intrinsically two
seconds long. CAT asserted PTT, then `startFtPreparedSlotTransmit()` waited more
than one second for a blocking call into the busy FT decoder thread. By the time
control returned, the audio target had expired and the complete-frame guard
cancelled TX. R4 closes that failure as follows:

- CAT/PTT pre-arms 650 ms before the selected boundary, inside the quiet tail
  after the useful FT signal from the preceding RX slot;
- the audio owner stops capture at the boundary, then queues the live-input gate
  and previous-slot finalization behind already-delivered RX blocks without
  waiting for a boundary/deep decode;
- operator requests up to the mode-specific full-frame-fit limit use the current
  selected period and receive a fresh 700 ms backend-preparation target;
- a later explicit TX/double-click may emit a bounded visual reply burst: it
  starts with the genuine Costas/frame prefix, never skips the beginning, keeps
  at least 600 ms of useful tone and stops 200 ms before the period changes;
- automatic sequencer, CQ and retry plans never use the partial path and defer
  an intact frame whenever the complete frame no longer fits.

The R4 change was confined to TX orchestration and did not modify the protected
candidate and LDPC blocks whose hashes are recorded above. The later R10 FT4
active-QSO scheduling change is documented separately in the R10 section.

RTTY live text no longer covers the crossed-ellipse scope. The existing
time-locked waterfall glyph engine is now active for a single `rtty-live`
stream centered at `(Mark + Space) / 2`; each new character moves vertically
with waterfall time while multidecoder callsign callouts remain static.

## Review findings closed or contained

| ID | 0.5.8 resolution |
|---|---|
| MM-001 | Replaced duplicate WAV readers with one strict RIFF/chunk/frame parser. It validates arithmetic, `blockAlign`, `byteRate`, chunk bounds and finite float samples. |
| MM-002 | Rear/data CAT PTT and USB/Data mode selection are fail-closed; there is no automatic fallback to ordinary PTT. |
| MM-003 | Shutdown performs blocking, acknowledged PTT OFF before disconnect, removes `QThread::terminate()` and does not invent an OFF state after a failed command. |
| MM-004 | FT configuration is protected for the complete decode pass. Pool workers use the captured mode profile and cannot re-enter the configuration lock. |
| MM-005 | ADIF writes use `QSaveFile`; in-memory state is swapped only after commit. STX/SRX direction mapping is corrected. |
| MM-006 | MSK144 and Q65 use UTC-period assemblers with sample timelines, explicit gap handling and period-start timestamps, including midnight rollover. |
| MM-007 | FT preserves absolute streaming resample position and the previous sample; MSK144/Q65 share the existing streaming `LinearResampler`. |
| MM-008 | Capture owns a dedicated thread. UI/non-FT and DSP delivery use bounded drop-oldest dispatchers with overload telemetry; FT keeps its direct AudioEngine-to-decoder queued path. Q65 owns a decoder thread. |
| MM-009 | Blocking `timedatectl`/`ntpq` probes were removed from the GUI path; the display reads asynchronous cached NTP state. |
| MM-010 | RTTY multidecoder now accumulates a real 4096/8192-sample window and does not scan a 1024-sample fragment as though it were complete. |
| MM-011 | Text terminals are bounded to 2500 blocks, highlighting is debounced and callsign scanning uses a bounded tail. |
| MM-012 | Main/runtime logs and FT/MSK144/Q65 receive tables have explicit retention limits. |
| MM-013 | Q65A/B/C/D RX/TX is always built. One native path owns sync, demodulation, QRA/CRC/unpack and parity-separated averaging; there is no optional backend or RX-unavailable state. |
| MM-014 | MSK144 work is owned and joined, with a generation guard that rejects stale results. Candidate selection covers the whole period and the native regression includes full and MSK40 frames. |
| MM-014A | MSK144 and Q65 share one UTC first/second-period TX scheduler. It keeps RX active while armed and defers complete frames after a missed boundary. |
| MM-015 | HRD v4 waits for complete CR/LF framing and caps the reply buffer at 1 MiB. |
| MM-016 | Audio conversion computes gain once per block and removes consumed input bytes in batches instead of on every frame. |
| MM-017 | ADIF record scanning is length-aware, so an `<EOR>`-like sequence inside a length-delimited field is not treated as a record boundary. |
| MM-018 | The blast radius is reduced incrementally through the dedicated capture thread, bounded dispatcher and dedicated decoder ownership. `MainWindow` remains large; a total rewrite was deliberately excluded from this stabilization release. |
| MM-019 | Bundled Hamlib auto-build is OFF by default; configure only detects dependencies unless explicitly requested. |
| MM-020 | CTest registers the runtime, FT, CAT, RTTY, localization, documentation, CW and waterfall tests. Package jobs run CTest, and a build/test workflow runs on every push and PR. Obsolete MIND workflow options are removed. |
| MM-021 | Global native-CPU tuning, AVX2 and LTO remain opt-in; portable packages do not enable them globally. |
| MM-022 | OpenStreetMap keeps normal TLS verification and enforces a 2 MiB tile-response limit. |
| MM-023 | Classic RIFF recording fails before the 4 GiB format limit and propagates header-rewrite/flush errors instead of writing a saturated corrupt header. |
| MM-024 | Offline SSTV/WEFAX loops no longer call nested `processEvents()`. They remain synchronous and explicitly non-cancelable in this release. |
| MM-025 | A missing configured RX or TX audio device fails closed instead of silently selecting the system default. |
| MM-026 | QSO form wrapper ownership is explicit and UI/log/table retention has been standardized. |

## Cleanup decisions

- Removed the dead secondary CW decoder pointer, obsolete CW software-AGC state,
  empty DSP setup method and duplicate Qt6 OpenGLWidgets link.
- Restored `PKG_CONFIG_PATH`/`PKG_CONFIG_LIBDIR` after Hamlib discovery.
- Retained compatibility-only FT setters that force the one proven adaptive
  engine. Removing them would change public connections without improving the
  decoder and is outside this no-regression checkpoint.
- Retained the two byte-identical 256 px icon names because Linux/Desktop
  packaging deliberately installs both case variants.
- Retained hidden WEFAX slant compatibility accessors as inert API; the stable
  receive timing path does not apply slant correction.

## Tests executed in the review environment

| Test group | Result |
|---|---:|
| Runtime hardening source guard | PASS |
| CAT startup, auto-connect and FT band/QSY synchronization | PASS |
| RTTY narrow contest layout and live/runtime rules | PASS |
| FT atomic lifecycle, caller queue/waterfall restore and linear sequencer | PASS |
| FT4 runtime topology and FT8/FT4 wideband/sensitivity invariants | PASS |
| UI themes: five complete palettes, semantic/map contrast and border ownership | PASS |
| Localization: 1824 keys in each of six languages | PASS |
| Native Q65 A/B/C/D generated-audio round trips | REGISTERED FOR QT CTEST |
| Native MSK144 full/MSK40 generated-audio round trips | REGISTERED FOR QT CTEST |
| Operator UI copy: compact state, translated runtime text and RTTY overlay control | PASS |
| Documentation: version 0.5.8, 72 HTML pages, six Qt Help projects | PASS |
| RTTY rules: 30 profiles, 29 active | PASS |
| macOS portability and release-version guards | PASS |
| Native CW production regression | PASS |
| Native waterfall leveler regression | PASS |
| Native CW and waterfall under ASan+UBSan | PASS |
| Workflow YAML and shell syntax | PASS |

### GitHub compile follow-up

The first Linux/MinGW build exposed an argument-dependent lookup ambiguity:
`WavStreamFormat` aliases `MadModemAudio::WavFileFormat`, so two anonymous-
namespace forwarding wrappers and the real namespace functions were both
viable candidates. 0.5.8 now calls the single shared reader with explicit
`MadModemAudio::` qualification and has no forwarding wrapper. WEFAX/SSTV also
propagate conversion errors instead of silently skipping a malformed chunk.
The Qt 5.14+ waterfall wheel path uses `QWheelEvent::position()`, removing the
unrelated deprecation warning while retaining the older-Qt fallback.

The next CI pass exposed two test-harness-only portability defects after the
application itself had compiled. MSYS2/Python 3.14 selected CP1252 for an
unqualified `Path.read_text()` and failed on valid UTF-8 source; all source
guards now request UTF-8 explicitly. Apple libc++ also generated a different
`std::normal_distribution` waveform than libstdc++ for the CW human-jitter
case. That one corpus now uses a fixed portable generator and seed, keeps
10% MARK jitter, 12% SPACE jitter, a 2.45 dash ratio and additive noise, and
still requires the exact `CQ CQ DE IZ6NNH` result. No production CW, audio,
waterfall or FT8/FT4 decoder code was changed for either correction.

The native CW suite includes clean messages, wrong WPM hints, the repeated
`CQ CQ OG50YL` live case, AWGN, QSB, adjacent carrier, noise tail and noise-only
scenarios. The waterfall suite covers quiet edges, AGC steps, receiver slope,
narrow carriers and asymmetric passbands.

## Required external acceptance tests

Two checks cannot be executed in the current review container:

1. the complete Qt application build, because the local container has no CMake
   or Qt development toolchain;
2. the four-WAV FT8 count regression, because neither the built executable nor
   the four external validated WAV files are present.

The new CI workflows perform the complete Qt build and CTest suite on the target
platforms. After building, the external FT corpus must be run with:

```bash
scripts/check_ft8_validated_baseline.sh ./build-linux/MadModem /path/to/wavs
```

The script verifies each WAV hash and requires at least 26, 25, 16 and 21 FT8
decodes respectively (88 total); a surplus on one file cannot hide a regression
on another. A short radio acceptance pass should then confirm audio-device
fail-closed behavior, CAT band QSY/readback, rear/data PTT routing and confirmed
PTT OFF at application shutdown.
