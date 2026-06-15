# FT MSHV assimilation map — v2.22

## Goal

MadModem FT must no longer be described as a set of decoder flavours.  The FT modem direction is a single MSHV-derived native pipeline integrated into MM's own audio, scheduler, UI, CAT and logbook layers.

## MSHV source analysed

From `MSHV_2765_Full_Source_Code.zip`:

- `src/HvDecoderMs/decoderft8.cpp`
- `src/HvDecoderMs/decoderft4.cpp`
- `src/HvDecoderMs/decoderms.h`
- `src/HvDecoderMs/decoderpom.*`
- `src/HvDecoderMs/ft_all_ap_def.h`
- `src/HvMsPlayer/libsound/HvGenFt8/gen_ft8.*`
- `src/HvMsPlayer/libsound/HvGenFt4/gen_ft4.*`

These are now vendored in the MM source tree under `third_party/mshv_gpl/upstream_2765/`.

## What MSHV does in FT8 RX

MSHV's normal/deep FT8 path is:

1. Accept a 12 kHz, 15-second FT slot buffer.
2. Run `sync8()`:
   - `NSPS=1920` samples/symbol.
   - `NSTEP=NSPS/4=480` samples, i.e. 40 ms time grid.
   - `NFFT1=2*NSPS=3840`, i.e. 3.125 Hz frequency bins.
   - ±2.5 s Costas search relative to nominal start.
   - Candidate cap around 600.
   - `sbase` baseline from `get_spectrum_baseline()` / `baseline()`.
3. Run `ft8b()` per candidate:
   - frequency/time refinement via `ft8_downsample()` and `sync8d()`.
   - demodulation into bmeta/bmetb/bmetc/bmetd/bmete metrics.
   - LDPC `decode174_91()` with BP/OSD depending on depth.
   - AP masks according to QSO progress and contest type.
   - unpack77 and validity checks.
   - report from `xsig/xbase` using `sbase`.
4. Run `subtractft8()` for decoded signals:
   - rebuild i4tone from the valid codeword.
   - generate the RX reference GFSK waveform.
   - filter and subtract with `K_SUB=1.9962`.
5. Repeat passes so weak signals hidden under stronger decoded signals can emerge.

## What v2.22 changes in MM

- Uses MSHV 40 ms candidate time grid rather than MM's prior 80 ms compromise.
- Keeps the MSHV 3.125 Hz frequency grid.
- Raises candidate budget toward MSHV's 600-class budget.
- Uses MSHV `K_SUB=1.9962` for cancellation.
- Keeps three-pass decode/subtraction as the only FT8 path.
- Keeps MSHV generator code for FT8 and FT4 TX.

## Why the direct upstream class is not simply compiled as-is yet

`DecoderFt8` is not a small pure function.  It depends on MSHV-specific global/static AP state, QObject signals, `PomAll`, `PomFt`, `F2a`, static decode memory across periods, multi-thread decoder IDs, MSHV display/list emission and upstream thread scheduling.  Pulling it into MM unchanged would risk freezing UI/audio or fighting MM's own UTC scheduler.

The correct long-term integration is an isolated `MshvFtDecoderAdapter` that exposes only:

```text
slot samples @ 12 kHz + mode + my/dx state + QSO progress
    -> list of {utc, snr, dt, df, message, confidence}
```

MM should continue to own audio capture, UTC slot timing, CAT/PTT, sequencer, logbook and UI.
