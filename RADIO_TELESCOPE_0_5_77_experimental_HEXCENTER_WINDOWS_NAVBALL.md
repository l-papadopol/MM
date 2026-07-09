# MadModem 0.5.77.experimental — Radio Telescope hex-center scan, Windows launcher, navball smoothing

This experimental patch keeps the public version string at `0.5.77.experimental` and fixes three field-test issues:

## Windows standalone launcher

The MSYS2/MinGW Windows package no longer ships a `.lnk` generated on the GitHub runner.  Such shortcuts store absolute build-machine paths such as `D:\a\...` and break after extraction.

The package root now contains `MadModem.cmd`, a portable launcher that always runs:

```text
bin\MadModem.exe
```

with the working directory set to:

```text
bin\
```

## Radio Telescope scan grid

The Alt-Az scan plan is now generated from a real projected honeycomb grid.  Each hexagon has one canonical sky-plane center; that center is converted to Az/El and is the exact target commanded to the rotator.

The invariant is now:

```text
one tile = one visible hexagon = one center Az/El target = one measurement
```

The old angular-step-first plan could visually skip or mis-associate hexagons for non-divisor beam widths such as 22 degrees.

## Navball anti-wind display smoothing

The rotator navball now applies a small deadband and EMA smoothing to the displayed current Az/El only.  Raw rotator readings remain available to the controller and safety logic; the smoothing is cosmetic and suppresses wind/backlash jitter in the instrument view.
