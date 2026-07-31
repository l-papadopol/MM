# MadModem 0.5.74 — Q65 MSHV assimilation

## Scope

This version adds the Q65 mode family to MadModem using the MSHV GPL source as
upstream reference and active code where it is already practical to compile in
the desktop/MadModem tree.

Implemented user modes:

- Q65A
- Q65B
- Q65C
- Q65D

MSHV represents these submodes with the tone-spacing multiplier used by its
Q65 generator and decoder:

| MadModem mode | MSHV multiplier (`mq65`) |
|---|---:|
| Q65A | 1 |
| Q65B | 2 |
| Q65C | 4 |
| Q65D | 8 |

MSHV does not expose an active Q65E user mode in the imported UI mode list, so
MadModem does not add a Q65E placeholder.

## Imported MSHV source

Active Q65 TX/generator path:

- `third_party/mshv_gpl/port/HvGenQ65/gen_q65.cpp/.h`
- `third_party/mshv_gpl/port/HvGenQ65/q65_subs.cpp/.h`
- `third_party/mshv_gpl/port/HvPackUnpackMsg/pack_unpack_msg77.cpp/.h`
- `third_party/mshv_gpl/nhash.cpp/.h`

Authoritative full MSHV Q65 RX reference staged in-tree:

- `third_party/mshv_gpl/reference/HvDecoderMs/decoderq65.cpp/.h`
- `third_party/mshv_gpl/reference/HvDecoderMs/decoderpom.cpp/.h`
- `third_party/mshv_gpl/reference/HvDecoderMs/ft_all_ap_def.h`

The reference RX decoder is intentionally not compiled by default in 0.5.74.
It depends on the original MSHV FFTW-backed `decoderpom` path and still needs a
careful MadModem bridge for threading, FFTW linkage, decode-list emission and
Linux/MXE package portability. The default Q65 RX object buffers full periods
and exposes the UI/settings/status plumbing without printing fake decodes.

## Active TX

`modems/q65/tx/Q65Transmitter.*` calls MSHV `GenQ65::genq65()` directly.
It supports periods:

- 15 s
- 30 s
- 60 s
- 120 s

Generation is performed at 48000 Hz, then resampled to the selected MadModem
audio rate. If MSHV generation fails, TX is inhibited. There is no synthetic or
non-standard fallback waveform.

## UI

Q65 is exposed as four mode entries in the main Mode selector:

- Q65A
- Q65B
- Q65C
- Q65D

The Q65 page uses one shared clean layout:

- RX table: UTC, dB, DT, DF, Message
- QSO form
- editable Tx1..Tx7 message table
- clear messages

The mode settings page exposes controls that match the MSHV Q65 operational
concepts rather than invented knobs:

- Period: 15/30/60/120 s
- Decode: Fast/Normal/Deep, default Normal
- RX frequency
- TX frequency
- DF tolerance
- Average decode
- Auto-clear average after decode
- Single decode
- AP decode
- Max drift
- EME delay
- DX callsign/grid

## RX bridge status

The Q65 RX bridge is deliberately conservative in 0.5.74. It collects and
resamples live audio into Q65 periods at 12000 Hz and reports period status. It
will not emit fake decoded text until the full `DecoderQ65::q65_decode()` path is
promoted into the active target with FFTW and verified message emission.

## Licensing

The imported Q65 sources are GPL code from MSHV/WSJT-derived work. MadModem is
GPL-compatible, so these files remain under GPL with their original notices.
