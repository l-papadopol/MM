# MadModem 0.6.5

**MadModem (MM)** è un'applicazione libera per radioamatori: modi digitali, audio,
waterfall, controllo della radio e del rotore, logbook e mappe in un unico programma.
Sviluppato da **Lucian-Ioan Papadopol, IZ6NNH** — [madexp.it](https://www.madexp.it).

**MadModem** is free amateur-radio software combining digital modes, a waterfall,
CAT/PTT, antenna rotator control and an ADIF logbook in one Qt desktop application.

## Funzioni / Features

- FT8, FT4, MSK144, Q65, CW, RTTY, BPSK/QPSK, MFSK, Feld Hell, SSTV e WEFAX.
- FT8/FT4 QSO sequencer, CQ queues, Evil Auto QSO and Doppler correction.
- CW native/neural receivers, CW/RTTY skimmers, audio CW and serial keying.
- Hamlib CAT/PTT, rotator tracking, station/DX information, QSO maps and UTC scheduler.
- ADIF logging, contest tools, macro messages, WAV recording and offline analysis.
- Help and interface in English, Italian, French, German, Norwegian and Czech.

## Download e installazione / Downloads

Scaricare il pacchetto dalla pagina [Releases](https://github.com/l-papadopol/MM/releases).

| Sistema / System | Download |
| --- | --- |
| Windows x64 | AutoCPU EXE installer; Compatible or AVX2/FMA portable ZIP |
| Ubuntu 24.04 / Mint 22.x x64 | RUN, DEB, portable TAR.GZ/ZIP |
| Fedora 43 x64 | RUN, RPM, portable TAR.GZ/ZIP |
| macOS 15+ Intel / Apple Silicon | PKG, app ZIP, DMG |

Select the package for your system and processor. Linux packages target the named
base distribution. Portable Linux: extract into a writable folder and run `AppRun`.
Installation details and user-data paths: [Installation](docs/INSTALLATION.md).

## Primo avvio / First run

1. Enter your callsign and locator in Settings → User/QTH.
2. Configure audio devices, CAT and PTT; check radio control and RX before TX.
3. Select a mode, start RX and adjust the input level without clipping.
4. For FT8/FT4, synchronize UTC. Check the selected correspondent and TX message.
5. Open the built-in Help for mode-specific instructions. Stop cancels TX and automation.

The title shows both version and build. Evil automation decisions are recorded in
Runtime log; complete compressed session logging is available in Settings → Diagnostics.
See [Troubleshooting](docs/TROUBLESHOOTING.md).

## Sorgenti / Source

- [Build instructions](docs/BUILDING.md)
- [GitHub distribution workflow](docs/DISTRIBUTION.md)
- [Documentation](docs/README.md) · [Release notes](RELEASE_NOTES.md)

Licensed under GNU GPLv3; see [COPYING](COPYING), [LICENSE.md](LICENSE.md),
[authors](AUTHORS.md) and [third-party notices](THIRD_PARTY_NOTICES.md).
