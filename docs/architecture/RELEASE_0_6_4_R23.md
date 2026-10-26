# R23 — SSTV editor, shared composition and standard VIS

This full-source revision builds on the delivered R22. Public version remains
0.6.4. Source revision: `0.6.4-source-r23-sstv-editor-vis`.

## Operator workflow

Open the SSTV editor. The complete card stays on the left; the right panel has
Card, QSO, Elements and TX tabs. Fit image and 100% control the canvas view.
Open/Save project, Undo/Redo, PNG export, Use for SSTV TX and Close remain
accessible on an 800x600 desktop. The TX tab previews the selected mode's
actual raster, including aspect-preserving borders.

My callsign defaults to the station profile. Correspondent, name, QTH and report
come from the quick fields. Empty fields produce empty text; there are no baked-in
sample callsigns, reports, country labels or fictitious date/exchange tables.
Elements can be selected in the list or on the card and dragged. Data edits
preserve each existing element's position, font and colour. Deleted QSO fields
stay deleted when other data changes; selecting the QSL template restores them.

`.mmsstv` saves the editable background, text, logos, frame, positions, fonts,
colours, hidden automatic fields and QSO metadata. The outer project format is
versioned by magic; canvas version2 reads version1 as well. Parsing is validated
before applying a document. Disk saves use QSaveFile. Undo/redo history is
bounded to 25 documents and 64 MiB (one document is retained if larger).
Unsaved project/image changes prompt before closing or replacing the background.
PNG is a flattened export; save a project for later editing.

Use for SSTV TX prepares the image and closes the editor. Transmission starts
from the main window's Send SSTV button. Main-window quick fields update the
same editable project. Reopening the editor retains the objects; preparing TX
adds no second frame or duplicate data. A newly loaded plain image with empty
quick fields is sent without an unsolicited frame. Projects are kept in memory
for the session and saved to disk explicitly.

## VIS and receive lifecycle

TX now generates the standard 300ms1900 / 10ms1200 / 300ms1900 leader,
30ms1200 start, seven LSB-first 30ms1100/1300 data cells, even parity and
30ms1200 stop. Header duration is910ms; the existing image/tail generation is
retained. Codes are shared across all19 modes. Raster width for MartinM2/M4 and
ScottieS2/S4 is corrected from160 to320, matching the published mode tables.

Auto mode (VIS) is a separate persisted preference, enabled by default. A valid
leader/break/leader, data, even parity and stop select the mode and start a fresh
frame. Short estimator transitions have bounded tolerance; invalid/unknown codes
are ignored. The last complete image remains visible until a new valid header
or an explicit reset. Mode selection is reflected in the GUI without resetting
the decoder a second time. Disable VIS for manual mode selection; Auto sync is
an independent line-lock setting. WAV analysis has a Cancel action.

## Evidence and limits

Linux Qt5 build and10 selected CTest cases passed, including the two new SSTV
regressions, window sizing, capture clock, TX pull, FT sequencer and four audits.
The VIS probe uses independently generated noisy PCM headers across19 modes
and four sample rates11025/44100/48000/96000:76cases. Generated TX header tones
are separately measured at each cell centre. Missing leader, bad parity, bad
stop and unsupported code are rejected. Repeated headers and manual VIS disable
are checked through the real decoder.

All19 complete clean colour-bar images traverse the actual TX generator and
RX decoder at12kHz and complete once at the expected raster. Interior patch
RMSE is below2.71 levels on an8-bit channel scale. This is a synthetic loopback,
not an independent external interoperability or weak-signal sensitivity test.
An initial diagnostic for128-row modes sampled boundaries after letterboxing;
those diagnostic scores are retained privately. Correctly transformed interior
sample positions fixed the measurement; no product demodulator change followed.

Editor regression checks small-screen primary actions, project/image roundtrip,
field position/style retention, undo/redo, element removal, hidden-field metadata,
invalid/truncated input and exact image handoff. Full-application workflow checks
three TX raster choices, quick-field composition and persisted VIS preference,
without opening an audio device or asserting PTT. All six interface languages pass at800x600, including full visibility of all
four tool tabs. Results are recorded in SSTV_R23_RESULTS.json.

Audio, DSP/GPU, all other modem implementations and R22 FT QSO fixes are retained.
No physical radio/audio/GPU or native Windows/macOS validation was performed.
Manual phase/colour trims retain their existing effect on subsequent rows; this
revision does not reconstruct previously decoded rows. There is no automatic
received-image disk archive; save a received image before the next header.
VIS behaviour under severe HF fading/interference still needs off-air recordings.

Timing/code references: bundled MMSSTV and QSSTV reference sources;
https://www.sstv-handbook.com/download/sstv-handbook.pdf (OK2MNM, chapter5).
