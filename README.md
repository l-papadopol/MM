# MadModem 0.5.8

**Digital modes, station control and logging in one desktop application.**

MadModem is a free, cross-platform amateur-radio application built with Qt and
C++. It brings digital-mode operation, a wideband waterfall, CAT/PTT, rotator
control, logging and mapping into a single workspace for Linux, Windows and
macOS.

The interface is designed around the radio operator: decoding, replies, station
controls and QSO information remain visible together, without switching among
separate utilities.

## What you can do

- Work **FT8 and FT4** with live wideband decoding, standard messages, UTC slot
  timing, QSO sequencing, caller queue and automatic logbook entry.
- Receive and transmit **RTTY, BPSK/QPSK, MFSK and Feld Hell**, with macros and
  a data-driven RTTY contest workspace.
- Follow two signals at once in **CW**, using independent RX A/RX B markers,
  AFC, adaptive speed tracking and diagnostic views.
- Receive and transmit **SSTV** and **WEFAX/MeteoFax**, including direct WAV
  analysis and image saving.
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

During a QSO, received messages, the selected reply, UTC timing, CAT frequency
and waterfall remain in the same view. FT4/FT8 sequencing follows the selected
transmit period and gives the active correspondent priority, helping the next
reply remain inside the correct slot.

The full-band waterfall keeps weak and strong signals readable across the
passband. Five complete themes provide consistent controls, tables, dialogs and
maps, from the Avionica cockpit style to light, dark and high-contrast layouts.

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

The repository includes build configurations for Linux, Windows and macOS.

See [`RELEASE_NOTES.md`](RELEASE_NOTES.md) for the current user-visible changes,
[`CHANGELOG.md`](CHANGELOG.md) for development history and
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) for component provenance.

## Author and licence

MadModem is developed by **Lucian-Ioan Papadopol, IZ6NNH** and released under
the **GNU General Public License v3**. See [`LICENSE.md`](LICENSE.md) and
[`COPYING`](COPYING).
