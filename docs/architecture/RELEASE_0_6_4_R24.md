# R24 — retrospective SSTV image adjustments

Full source based on the delivered R23; public version0.6.4.
Source revision `0.6.4-source-r24-sstv-retrospective-image-adjustment`.

## Operator behaviour

Open **Adjust received image** in the Mode tab. Horizontal position rolls every
received row; Red alignment and Blue alignment shift their channels against
green, with edge extension rather than wrapping coloured pixels across the
picture. These controls work both during reception and after completion, with
live RX stopped and after WAV analysis. Robot and PD are included.

**Reset adjustments** returns all three values to zero and restores the original
received pixels. It preserves the image and the VIS/line-sync settings. Changes
are saved in the existing SSTV settings. The range follows the selected raster:
horizontal +/-width, red/blue +/-max(8,width/3), with one-pixel steps.

**Start RX without synchronization**, inside the expandable panel, explicitly
restarts the image, switches off both Auto mode (VIS) and Auto sync, and starts
live RX if needed. An explanatory label states these consequences before the
operator clicks. Expanding the panel alone changes no receive setting. Re-enable
the two checkboxes for automatic reception. This replaces the inaccessible,
hidden and ambiguously named Force SSTV RX button.

The editor, editable projects, shared editor/TX composition, VIS TX/RX,
header-triggered next-frame reception and cancellable WAV analysis from R23
remain included. There is still no automatic received-image disk archive.

## Data and threading

The decoder retains a canonical RGB image after the existing automatic colour
registration, before operator adjustments. Each row also retains the automatic
horizontal phase used when it was decoded. These automatic corrections remain
frozen for that row, preserving the previous zero-adjustment display behaviour.

Manual adjustments rebuild the preview from canonical rows; they never resample
a previously adjusted preview. New rows use the same current manual values.
Thus repeated positive/negative adjustments and return to zero recover exactly
the same pixels. Reset, mode changes, sample-rate epochs and a fresh VIS frame
clear/rebuild the canonical raster alongside the normal image.

Rendering and image snapshots remain on the existing RX worker. The GUI obtains
the corrected snapshot through the existing coalesced notification path, even
when capture is stopped. Changing display settings emits imageUpdated, while
imageCompleted remains a one-shot receive event. PNG saves the displayed result.
An unchanged Auto VIS preference now leaves a partially acquired header intact;
GUI trim updates must not reset the VIS detector.

Storage is bounded by two rasters plus per-row phase values (under4MiB for the
800x616 mode, before independent GUI snapshot copies). The final local run's
maximum combined phase+colour setter time was7.08ms; this is a local observation,
not a hardware-independent latency guarantee. No GPU backend was added.

## Validation

- Linux Qt5 application and regression targets build successfully.
- New public-API regression:19modes x sync/free-running =38 complete cases,
  including mid-frame corrections and12 reversible adjustment cycles per case.
- Independent integer-channel/phase oracle in free-running cases; phase oracle
  in both cases; streaming/pre-adjusted receiver matches retrospective results.
- Zero restores exact original pixels; completion is not re-emitted; PNG roundtrip
  matches corrected pixels; reset clears retained data.
- Repeated display settings interleaved with a real audio VIS header retain mode
  acquisition. Existing76independent VIS cases and19full colour-bar loopbacks pass.
- Native GUI/RX-worker probe: stopped-RX snapshot and preview update together;
  zero/reset recovers the original; panel opening/reset preserve automatic flags;
  explicit unsynchronized restart clears the image and disables both flags.
  It bypasses the hardware-start branch with a simulated running flag; no audio
  device or PTT is opened.
- Six-language control geometry/non-overlap is checked in the real main window;
  final results are recorded in SSTV_R24_RESULTS.json.
- Eleven selected CTest cases pass, including the SSTV editor, capture clock,
  TX pull, FT sequencer, window sizing and four architecture/release/UI audits.

No physical radio/audio or native Windows/macOS validation was performed.
Synthetic loopback validates display correctness, not an external application's
interoperability or weak-HF sensitivity. RX demodulation, sync/AFC and automatic
chroma estimators are retained; the manual trims move to the presentation layer.
All non-SSTV modem, audio, DSP/GPU, SSTV TX and editor sources must match R23
byte-for-byte in the package audit.
