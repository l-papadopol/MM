# Third-party notices and credits

MadModem uses and/or derives code and algorithms from the projects below. The
original license files and selected reference sources are kept under
`third_party/` for attribution, traceability, and source-compliance.

## Original MadModem project material

Project lead, original concept, UI/product direction, integration, testing and
release packaging: **Papadopol Lucian-Ioan**.

- E-mail: **l.i.papadopol@gmail.com**
- Website: **www.madexp.it**

Original MadModem material not otherwise credited to a third-party project is
copyright Papadopol Lucian-Ioan and is released as part of this GPLv3 combined
package. Much of the implementation was produced through an AI-assisted coding
workflow under direct human drive: requirements, architecture, experiments,
testing, refinements and release decisions are human-directed, while AI is used
as a coding multiplier.

## MSHV / WSJT-X-related FT8 material

Location:

- `third_party/mshv_gpl/`

Used for:

- FT8 77-bit message packing/unpacking.
- FT8 LDPC/CRC protocol tables and helpers.
- FT8 GFSK transmit generation.
- FT8 receive/unpack integration support.

License:

- GNU GPL, version 3, as preserved in `third_party/mshv_gpl/LICENSE_MSHV.txt`.
- Original headers also credit WSJT-X-related authors for the FT8 algorithms,
  source code, look-and-feel and protocol specifications.

## QSSTV

Location:

- `third_party/qsstv_gpl/`

Used for:

- Sound-card clock calibration concept and selected reference material.
- Future deeper SSTV timing/sync assimilation reference.

License:

- GNU GPLv3-or-later, as preserved in `third_party/qsstv_gpl/`.

Credit:

- QSSTV by Johan Maes, ON4QZ.

## MMSSTV

Location:

- `third_party/mmsstv_lgpl/`

Used for:

- SSTV RX ideas, including frequency-estimation and SSTV line/rendering
  concepts, ported into Qt/CMake-friendly code.

License:

- GNU LGPLv3-or-later, as preserved in `third_party/mmsstv_lgpl/`.

Credit:

- Copyright 2000-2013 Makoto Mori, Nobuyuki Oba.


## Decodium / Raptor reference material

Location:

- `third_party/decodium_gpl/`

Used for:

- FT8 timing/NTP diagnostic ideas and a small adapted Qt NTP client.
- Raptor-style waterfall palette reference.
- Decode hygiene ideas such as deduplication, candidate prioritization and conservative multi-pass rescan concepts.

License:

- GNU GPL, as preserved in `third_party/decodium_gpl/COPYING`.

Credit:

- Decodium 3.0 "Shannon" / Raptor source package supplied by the user.


## fldigi / gmfsk reference material

External reference source supplied for this development pass:

- `fldigi-master.zip` in the project handoff/archive material.

Used for:

- Receiver architecture inspiration for PSK/QPSK and MFSK-style tone-bank workflows.
- v1.52 MFSK detector hardening follows the same general MFSK design idea used in fldigi/gmfsk-family code: 8 kHz working rate, per-symbol tone-bank analysis, AFC-capable multi-tone receiver and conservative status metrics.
- No large fldigi GUI/runtime subsystem is imported into MadModem; adaptations are kept in native Qt/C++ MadModem files unless explicitly placed under `third_party/`.

License:

- fldigi is GPLv3-or-later; gmfsk-derived MFSK material is credited in fldigi source headers.

Credit:

- fldigi by Dave Freese W1HKJ and contributors.
- gmfsk MFSK material originally credited to Tomi Manninen OH2BNS in fldigi headers.

## WSJT-X reference material

External reference source supplied for this development pass:

- `wsjtx-3.0.1-src.tar.gz` in the project handoff/archive material.

Used for:

- Weak-signal receiver architecture comparison: 12 kHz FT working domain, strict slot timing, full-passband candidate search and filtered sample-rate conversion before Costas/LDPC decoding.
- v1.52 keeps MadModem's existing compact native FT decoder, but adds an explicit low-pass guard before resampling into the 12 kHz weak-signal domain.
- v1.58 uses WSJT-X palette stop colours (`Default.pal` and `Fldigi.pal`) as reference stops for MadModem's 256-colour interpolated waterfall palettes.

License:

- WSJT-X is GPL-compatible open source. Preserve upstream notices for any future direct imports.

Credit:

- WSJT-X / WSJT project authors and contributors.

## Hamlib

Location:

- `third_party/hamlib_lgpl/`

Used for:

- CAT rig-control API.
- Frequency readout.
- CAT PTT.
- Bundled all-in-one Linux/Windows builds from the included Hamlib source tree.

License:

- Hamlib library source: LGPL2.1-or-later.
- Some upstream tools/examples included in the Hamlib source tree are
  GPL2-or-later.

Credit:

- Hamlib - Ham Radio Control Libraries.
- Copyright holders and contributors are listed in the upstream `AUTHORS` file.


## ggmorse

Location:

- `third_party/ggmorse_mit/`

Used for:

- v2.66 CW receive timing/speed estimation and fixed-tone Morse decoding support.
- MadModem keeps its own CW marker/UI logic, but uses ggmorse as a
  robust primary decode engine for the selected CW signal.

License:

- MIT License, preserved in `third_party/ggmorse_mit/LICENSE`.

Credit:

- ggmorse by Georgi Gerganov.

## Qt

MadModem is a Qt application. Build and distribution must respect the license of
the Qt version used by the builder/distributor.

## Boost compatibility headers copied through MSHV

Some small Boost compatibility/header material is present under the MSHV port
subtree because it was needed by the imported FT8 code. Preserve its original
notices when modifying that subtree.
