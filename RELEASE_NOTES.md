# MadModem 0.5.8 release notes

MadModem 0.5.8 concentrates on predictable live operation: weak-signal
decoding keeps its established sensitivity, time-critical FT work is completed
before the reply slot, and the interface communicates state without covering
the operating area.

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

## CW and text modes

- CW keeps two independent RX lanes with exact-tone filtering, AFC, adaptive
  timing and soft MARK/SPACE evidence. The native regression covers clean,
  noisy, QSB, human-timed and adjacent-carrier cases.
- The RTTY tuning scope contains only the Mark/Space signal trace. Optional
  decoded text is drawn vertically on the waterfall and has its own on/off
  control.
- The RTTY contest workspace uses external rules, transactional serial numbers,
  macros, duplicate checks and live scoring without creating a second logbook
  path.

## Interface and station control

- All bundled themes define complete palettes. Light themes keep dark text on
  light surfaces; dark and high-contrast themes retain readable tables, maps,
  dialogs and status colours.
- Long operational help is no longer repeated in labels, tooltips and the
  status bar. Narrow side panels use compact, wrapping text and bounded controls.
- Radio CAT/PTT and rotator connections remain separate. CAT/PTT fails closed
  when a requested data route cannot be established.
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

The source package includes localization and documentation audits, FT lifecycle
and sensitivity guards, CAT/band synchronization checks, RTTY layout/runtime
checks, plus native CW and waterfall regressions. CI builds and runs the
applicable suite on Linux, Windows and macOS.

See [CHANGELOG.md](CHANGELOG.md) for the chronological engineering history and
[docs/README.md](docs/README.md) for user and developer documentation.
