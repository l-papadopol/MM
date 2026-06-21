# Third-party notices and credits

MadModem is a GPLv3 combined work. This file summarizes the upstream material currently present in the tree. The individual upstream license files under `third_party/` remain authoritative.

## Original MadModem project material

Project lead, original concept, UI/product direction, integration, testing and release packaging: **Papadopol Lucian-Ioan / IZ6NNH / MadExp**.

Original MadModem material not otherwise credited to a third-party project is part of this GPLv3 package.

The project uses an AI-assisted coding workflow under human direction. The author remains responsible for the product concept, requirements, testing and release decisions.

## Compiled or linked third-party material

### MSHV / WSJT-X-related FT material

Location: `third_party/mshv_gpl/port/`

Used for FT8/FT4 protocol support, including message packing/unpacking, generators, CRC/LDPC helpers and related FT support code.

License: GPL, as preserved in `third_party/mshv_gpl/`.

### Hamlib

Location: `third_party/hamlib_lgpl/source/`

Used for radio CAT/PTT and rotator control. The bundled build scripts build Hamlib for Linux and Windows/MXE.

License: Hamlib library LGPL-2.1-or-later; some upstream tools/examples are GPL-2.0-or-later.

### ggmorse

Location: `third_party/ggmorse_mit/`

Used by the CW decoder.

License: MIT License.

Credit: Georgi Gerganov.

### MMSSTV-derived SSTV core

Location: `third_party/mmsstv_lgpl/MmsstvRxCore.*`

Used as the compiled SSTV RX helper in MadModem. The file itself states that it is derived from concepts and constants in MMSSTV's LGPL `sstv.cpp` / `sstv.h`.

License: LGPLv3-or-later, as preserved in `third_party/mmsstv_lgpl/`.

Credit: Makoto Mori and Nobuyuki Oba.

### Decodium / Raptor adapted NTP client

Location: `third_party/decodium_gpl/port/NtpClient.*`

Used as an adapted Qt NTP client.

License: GPL, as preserved in `third_party/decodium_gpl/`.



### AD1C/K1EA country file

Location: `cty.csv`

Used for DXCC/country/prefix lookup. Preserve upstream attribution when updating the file.

### Qt

MadModem is a Qt application. Build and distribution must respect the license of the Qt version used.

## Bundled reference material not compiled into the current MadModem target

The following directories are kept for attribution, traceability, study or future comparison, but are not listed in the current `MadModem` CMake target source list:

- `third_party/qsstv_gpl/reference/`;
- `third_party/mmsstv_lgpl/reference/`;
- `third_party/mshv_gpl/reference/`;
- `third_party/mshv_gpl/upstream_2765/`;
- `third_party/decodium_gpl/reference/`.

QSSTV is important as the reference for the sound-card calibration workflow, but the current MadModem executable does not compile or link QSSTV source files.

## Reimplemented / inspired-by work

The following projects influenced MadModem design, but their source code is not directly copied into the current MadModem-native files unless separately listed above:

- WSJT-X: FT slot timing, sequencing, parser/QSO-state behaviour.
- MSHV / WSJT-X / JTDX family: FT receiver architecture ideas and live-decode budgeting.
- fldigi / gmfsk family: PSK/QPSK/MFSK receiver design ideas.
- QSSTV: sound-card calibration workflow idea.
- CatRotator-style programs: rotator workflow and UX ideas.
- Meeus/NOAA-style formulae: Moon / EME local ephemeris calculation.

### MIND Eigen

Location: `ai/DeepDspTinyNet.*`

MadModem internal code. No external neural-network runtime dependency is required for MIND shadow learning.


### Eigen

Location: `third_party/eigen/`

Eigen is bundled as header-only source for MIND matrix algebra. License files from the supplied Eigen source archive are preserved in the same directory.

## gMFSK MFSK16 reference material

MadModem's standard MFSK16 Varicode/FEC implementation was written for MadModem using the public gMFSK source as protocol reference for the IZ8BLY MFSK Varicode table, R=1/2 K=7 convolutional FEC polynomials, and the 10-stage diagonal interleaver/deinterleaver behaviour. gMFSK is GPL-2.0-or-later; MadModem is GPL-3.0, which is compatible with GPL-2.0-or-later code and protocol-table reuse.

Referenced components include gMFSK `mfsk.h`, `mfskrx.c`, `mfsktx.c`, `interleave.c`, and `varicode.c`.


## fldigi GPL reference core

MadModem 0.5.27 uses fldigi source code supplied by the user as a GPL reference for text-mode DSP behaviour. The integrated changes are Qt/C++ MadModem code, but PSK symbol sampling/Varicode handling, MFSK16 softdecode/FEC/interleaver structure, Feld Hell raster orientation and CW timing strategy were compared against fldigi's GPL sources under `src/psk`, `src/mfsk`, `src/feld` and `src/cw_rtty`.
