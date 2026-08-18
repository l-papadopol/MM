# MadModem 0.5.8

**One desktop for digital modes, station control and radio experiments.**

MadModem is a free, cross-platform amateur-radio application built with Qt and
C++. It brings receive and transmit tools, a wideband waterfall, CAT/PTT,
rotator control, logging and mapping into a single operating workspace for
Linux, Windows and macOS.

The interface is designed for live use: mode controls stay beside the decoder,
the waterfall remains visible while operating, and station data moves through a
single logbook instead of separate utilities.

<!-- Hero screenshot requested: docs/images/madmodem-ft8-overview.png -->

## What you can do

- Work **FT8 and FT4** with live wideband decoding, standard messages, UTC slot
  timing, QSO sequencing, caller queue and automatic logbook entry.
- Receive and transmit **RTTY, BPSK/QPSK, MFSK and Feld Hell**, with macros and
  a data-driven RTTY contest workspace.
- Follow two signals at once in **CW**, using independent RX A/RX B markers,
  AFC, adaptive speed tracking and diagnostic views.
- Receive and transmit **SSTV** and **WEFAX/MeteoFax**, including direct WAV
  analysis and image saving.
- Experiment with **MSK144 and Q65**. Q65 receive requires the optional complete
  FFTW/MSHV backend; the application reports clearly when that backend is not
  present.
- Control a radio through **Hamlib CAT/PTT**, keep the FT dial frequency aligned
  with the selected band and use saved station settings at startup.
- Control up to three rotator profiles, point by locator or DXCC, and track the
  mechanical azimuth range safely.
- Keep an **ADIF logbook**, inspect DXCC information and plot contacts on the
  integrated QSO map.
- Run receive-only **Radio Telescope** sky scans with rotator movement, timed
  integration and CSV export.

## Operating views

| Area | Highlights |
| --- | --- |
| FT4 / FT8 | Wideband decode table, QSO timeline, standard messages, slot clock, focused QSO priority |
| CW | Two independent receivers, Auto-WPM, AFC, selectable bandwidth, soft-decision timing diagnostics |
| RTTY | Live terminal, Mark/Space tuning scope, optional waterfall text, contest profiles and macros |
| Image modes | SSTV and WEFAX receive/transmit, image preview, WAV analysis and PNG export |
| Station | CAT/PTT, audio routing, rotators, scheduler, logbook, DXCC and map |
| Radio Telescope | Receive-only Alt-Az scans, beam-sized sampling cells and CSV measurements |

## Built for on-air operation

MadModem keeps audio capture away from the GUI thread, bounds display queues and
uses persistent decoder workers so a busy waterfall cannot create an unlimited
backlog. FT4/FT8 scheduling follows UTC slot boundaries, while an active QSO
receives a focused first decode pass around the correspondent's audio frequency.

The waterfall provides stable full-band levelling, OpenGL acceleration when
available and a software fallback. Five complete themes cover controls, tables,
menus, dialogs and maps rather than applying partial colour overrides.

## Station integration

- Hamlib radio control over serial CAT, `rigctld` or supported TCP endpoints
- CAT, serial RTS or serial DTR PTT routes
- separate RX and TX audio-device selection
- CAT-aware FT band changes and stored startup configuration
- independent radio and rotator connections
- ADIF import/export and atomic logbook saving
- direct recording of the normalized RX stream as 16-bit PCM WAV

Before transmitting, verify the selected radio mode, PTT route, TX audio level,
frequency and antenna. Before automatic movement, configure rotator limits,
cable-wrap behaviour and an accessible emergency stop.

## Screenshots wanted

The following images will give the project page a useful visual tour. Capture
them at 1920×1080 (or the monitor's native resolution) with the **Avionica**
theme, no open tooltip or Runtime Log window, and a clean audio level below
clipping.

1. **FT8 live QSO** — received decodes, QSO history, selected standard message,
   slot clock, waterfall and CAT frequency all visible.
2. **CW dual receiver** — two separated waterfall markers, RX A/RX B text and
   the diagnostic envelopes visible without covering one another.
3. **RTTY contest** — terminal, Mark/Space scope, compact Contest tab and the
   optional vertical waterfall decode trail.
4. **SSTV or WEFAX** — a completed decoded image beside the relevant receive
   controls.
5. **Logbook and QSO map** — a filtered set of contacts with paths or Maidenhead
   squares visible.
6. **Radio Telescope** — an active or completed sky scan with the measurement
   grid and rotator state.

Suggested filenames are `madmodem-ft8-overview.png`,
`madmodem-cw-dual-rx.png`, `madmodem-rtty-contest.png`,
`madmodem-image-mode.png`, `madmodem-logbook-map.png` and
`madmodem-radio-telescope.png`. Place them in `docs/images/`; the first can then
replace the hero comment near the top of this file.

## Languages and help

The runtime interface and embedded Qt Help are available in English, Italian,
French, German, Norwegian and Czech. Language changes apply to the active
interface without changing radio or decoder settings.

Start with **Help → MadModem Help** or open [`docs/README.md`](docs/README.md)
for the documentation index.

## Build from source

The project requires CMake, a C++17 compiler, Qt 5 or Qt 6 development packages
and the usual audio/serial development libraries. Hamlib enables radio and
rotator control.

```bash
./build_all.sh
```

The repository includes CI configurations for Linux, Windows and macOS. Native
decoder regressions and source audits can also be run through CTest after a
test-enabled build.

```bash
ctest --test-dir build --output-on-failure
```

See [`RELEASE_NOTES.md`](RELEASE_NOTES.md) for the current user-visible changes,
[`CHANGELOG.md`](CHANGELOG.md) for development history and
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) for component provenance.

## Project status

MadModem is under active development. FT4/FT8, CW and live radio control should
always be validated with the same recorded audio and on-air setup when decoder
or timing code changes. MSK144 and Q65 remain development areas; Q65 receive is
available only when its complete optional backend is compiled.

## Author and licence

MadModem is developed by **Lucian-Ioan Papadopol, IZ6NNH** and released under
the **GNU General Public License v3**. See [`LICENSE.md`](LICENSE.md) and
[`COPYING`](COPYING).
