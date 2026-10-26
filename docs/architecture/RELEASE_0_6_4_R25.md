# R25 — automatic archive for received SSTV images

Full source based on delivered R24; public version0.6.4.
Revision: `0.6.4-source-r25-sstv-rx-auto-save`.

## Operator workflow

In Mode, enable **Automatically save received images** and choose the destination
with the folder button next to the path. Both preferences are persisted. The
option is initially off; the proposed directory is the platform Pictures folder
plus `MadModem/SSTV`, created on the first accepted save. The folder path is
read-only in the panel and changes through the native directory picker.

A completed live or WAV image is saved as PNG, with UTC date/time and mode:
`2026-10-26_183045_Robot36.png`. Collisions add `_001`, `_002`, etc. Existing
files are retained, including files that are not valid PNGs. Spaces and Unicode
characters in the selected destination are supported.

The image saved is the completed RX frame, including the phase/channel trims
active at completion. A later display adjustment does not create another archive
entry or alter the existing PNG; use Save PNG for a manual corrected export.
Partial frames do not trigger automatic saving. Repeated reception/analysis of
an image produces a separate file.

**Open received-image folder** opens the system file manager. Before the first
save, a missing default directory is explained in the status instead of created
by merely viewing settings. Success shows Saved plus the filename; the tooltip
and runtime log contain the complete path. Errors are visible without a modal
message blocking reception. The own QSL, TX source and editable project remain
separate; received images are not handed to the TX editor.

## Threading, snapshots and shutdown

A dedicated serial worker performs directory creation, PNG encoding, flush and
file publication. Audio, decoder and GUI threads perform no automatic PNG
encoding or file writes. Each queued job retains its own immutable QImage copy,
mode, UTC handoff time and destination. Changing the preference or folder affects
future jobs; accepted jobs retain their captured destination and finish.

The facade receives only imageCompleted through a queued connection, not
imageUpdated. Mode comes from the canonical completion reason, independent of
later GUI mode selection. The image's Qt copy-on-write storage survives the next
VIS reset. Eight pending jobs plus at most one active job bound memory. Queue
pressure is reported explicitly rather than silently dropping accepted jobs.

PNG is encoded into a QTemporaryFile in the destination directory. Its concrete
filename is obtained before closing (important for unnamed Linux temporary
files). A QFile then renames the complete file to an unused final name; an
existing destination is never replaced. Failed writes clean up their temporary
file. The worker remains available after a destination error.

At program close, archive-specific queued completion events are dispatched after
RX worker shutdown, then all accepted images are drained before destroying the
archive object. Disk latency can therefore extend Close while images are saved.
The close test exposed an existing queued VIS GUI callback reaching an already
stopped decoder; SSTV mode/completion UI callbacks now ignore shutdown, while the
separate archive completion receiver still saves the final frame.

While touching SSTV preferences, the load validation was brought into line with
R24's maximum raster ranges: horizontal +/-800 and colour +/-266. Previously the
older320/96 load guards discarded valid PD-wide trim values. Actual mode-specific
limits remain enforced by the existing R24 UI/decoder.

## Validation

- Linux Qt5 application build: PASS.
- Twelve selected CTest cases: PASS, including archive, VIS, editor, retained-image
  adjustments, capture clock, TX pull, FT sequencer, window and four audits.
- Archive regression: default disabled state; image/destination snapshots;
  existing-file collisions; two independent concurrent writers; UTC and sanitized
  names; Unicode/space paths; invalid destination plus recovery; bounded queue;
  all accepted jobs drained at finish; no temporary file left after success.
- Actual decoder -> GUI archive -> disk probe: completed Robot24 PNG matches the
  completion snapshot, own QSL/TX pixels remain unchanged, later display trims
  do not resave, disabling prevents the next frame from being saved, and an
  image whose completion is still queued for the GUI at Close is saved correctly.
- Settings reload preserves selected folder, enable flag and valid wide-raster
  trims. Six interface languages retain mode, window width and usable folder/
  status controls. Screenshots were inspected internally.

No radio/audio device or PTT was opened by the probes. Native Windows/macOS,
network shares and launching a real desktop file manager were not tested here.
The directory picker uses the existing Qt native-dialog path. Actual folder/PNG
operations were tested on local Linux temporary directories.

All R24 modem implementations (including SSTV RX/TX), audio, DSP/GPU and the QSL
editor must remain byte-identical in the release audit. This revision adds archive
I/O and GUI/settings integration; it does not alter SSTV demodulation or TX.
