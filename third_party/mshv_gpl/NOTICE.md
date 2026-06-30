# MSHV FT8 reference and port material

This directory contains selected FT8-related source files from MSHV for
attribution, traceability, and staged source-level porting into MadModem.

MSHV is GPLv3 software. The original copyright and license notices are preserved
in the referenced files and in `COPYING_MSHV.txt` / `LICENSE_MSHV.txt`.

MadModem v0.90 added the FT8 UI/scheduler shell only.

MadModem v0.91 adds a compiled, source-level MSHV-derived FT8 TX path under
`port/`:

- `HvPackUnpackMsg/pack_unpack_msg77.*`: 77-bit FT8 message pack/unpack logic.
- `genpom.*` plus `HvGenFt8/bpdecode_ft8_174_91.h`: CRC/LDPC helper data and
  encoder support.
- `HvGenFt8/gen_ft8.*`: FT8 GFSK tone/audio generation.

The v0.91 port is used only for transmit generation.  The receive decoder core
remains staged for the following FT8 patch.


MadModem v0.92 adds a first compiled FT8 RX stage in modems/ft8/Ft8RxDecoder.*.
This RX stage reuses MSHV FT8 protocol material already present in this tree:
the 174/91 LDPC table, CRC14 polynomial handling, and the 77-bit message
unpacker through GenFt8.  It does not compile the full original MSHV QObject
receiver tree; the original MSHV decoder files remain under reference/ for
traceability and future deeper porting.
