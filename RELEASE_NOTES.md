# MadModem 0.5.9-alpha release notes

MadModem 0.5.9-alpha is a pre-release build for validating the new FT split-operation path while retaining the established live-operation baseline: weak-signal
decoding keeps its established sensitivity, time-critical FT work is completed
before the reply slot, and the interface communicates state without covering
the operating area.

### Alpha r2 Hamlib compatibility

- The first alpha referenced the old public `RIG::state` layout used by older WSJT-X/Hamlib combinations. Bundled Hamlib 4.7.2 intentionally hides that state. MadModem now uses `rig_get_vfo_list()` and the normal split APIs only; no Hamlib private data is accessed.
- This is a compile/encapsulation correction to the opt-in Rig Split path. `Split=None`, FT decoding/sequencing, CW, UDP logging and the established CAT/PTT path are unchanged.

## FT4 and FT8

- An active QSO receives a bounded, frequency-focused decode pass before the
  general passband. The reply must contain the expected callsigns before the
  wideband pass may be deferred.
- FT4 now follows the same deadline order as FT8. Previously its focused pass
  ran only after the wideband work and could therefore arrive too late.
- FT transmission preparation is separated from the UTC audio boundary. A slow
  decode or UI update can no longer cancel an otherwise valid scheduled frame.
- CAT-connected band selection tunes the standard FT dial frequency. Scheduled
  band changes wait for an active TX or QSO and never start transmission.
- Slot, sequencer and TX state use short labels; complete explanations remain in
  tooltips and the Runtime log.

## MSK144 and Q65

- MSK144 searches energetic windows across the complete UTC period instead of
  exhausting its candidate budget near the beginning. Full frames and MSK40
  hashed short messages share one RX/TX codec path.
- Q65A/B/C/D receive and transmit are included in every build. The native RX
  path performs synchronization, time/frequency/drift refinement, soft 65-ary
  demodulation, QRA decoding, CRC validation and 77-bit message unpacking.
- Q65 averaging is kept separately for the two period parities. AP candidate
  lists use the configured local and DX calls without creating another decoder.
- Both modes share one UTC first/second-period scheduler. Reception continues
  while a frame is armed, and a missed boundary is deferred rather than sent
  as a shortened, undecodable frame.
- CTest round-trips generated Q65 audio in all four submodes and both MSK144
  frame families.
- The common 77-bit message/hash codec is compiled once and serialized across
  FT4/FT8, MSK144 and Q65, including mode changes with an older decode still
  completing.

## CW and text modes

- CW keeps two independent RX lanes with exact-tone filtering, AFC, adaptive
  timing and soft MARK/SPACE evidence. The native regression covers clean,
  noisy, QSB, human-timed and adjacent-carrier cases.
- The RTTY tuning scope contains only the Mark/Space signal trace. Optional
  decoded text is drawn vertically on the waterfall and has its own on/off
  control.
- RTTY and CW share one contest workspace with mode-specific external rules,
  transactional serial numbers, macros, duplicate checks and live scoring
  without creating a second logbook path. CW also exposes the same six standard
  text-macro buttons as RTTY directly above its TX editor.

## Interface and station control

- All bundled themes define complete palettes. Light themes keep dark text on
  light surfaces; dark and high-contrast themes retain readable tables, maps,
  dialogs and status colours.
- Long operational help is no longer repeated in labels, tooltips and the
  status bar. Narrow side panels use compact, wrapping text and bounded controls.
- Radio CAT/PTT and rotator connections remain separate. CAT/PTT fails closed
  when a requested data route cannot be established.
- FT4/FT8 Split Operation is opt-in (`None` remains the default). `Rig` uses
  Hamlib's native split-frequency and split-mode APIs with the same logical
  A/B-or-MAIN/SUB selection strategy used by WSJT-X; the Hamlib backend, not
  MadModem, maps that logical selector to radios with richer VFO/bank layouts.
  `Fake It` is the WSJT-X name for temporarily moving the current VFO during TX.
- Audio capture and UI/DSP distribution use bounded queues so a slow display
  cannot create an unbounded backlog.

## Documentation and languages

- The project README is now a concise public feature overview.
- The embedded help was reviewed in English, Italian, French, German, Norwegian
  and Czech. The Scheduler page now describes the actual daily UTC QSY plan;
  stale rotor text and untranslated paragraphs were removed.
- Runtime dictionaries use complete reviewed sentences for operator-critical
  messages. Missing prose falls back to coherent English rather than mixed
  word-by-word output.

## Validation

The source package keeps the detailed subsystem checks, but CTest groups them
into stable responsibility-based suites instead of exposing one test per
historical bug. Linux/macOS run 10 CTest entries: WAV, Q65, MSK144, CW and
waterfall regressions plus architecture, UI/Contest, FT runtime, release and
waterfall-integrity audits. Windows runs the applicable seven-entry subset. CI
builds and runs the complete applicable suite before packaging.

See [CHANGELOG.md](CHANGELOG.md) for the chronological engineering history and
[docs/README.md](docs/README.md) for user and developer documentation.
