# MMSSTV-derived SSTV RX code

This directory contains the first MadModem SSTV RX port derived from the open-source MMSSTV codebase supplied to the project.

Original MMSSTV files `sstv.cpp` and `sstv.h` are retained under `reference/` for attribution and algorithm traceability. They are not compiled directly because the original MMSSTV code is tied to Borland C++Builder/VCL/WinAPI. The compiled files `MmsstvRxCore.h/.cpp` are a Qt/CMake-friendly source-level port of the relevant RX ideas used first by MadModem v0.82.

Original copyright notice from MMSSTV:

Copyright 2000-2013 Makoto Mori, Nobuyuki Oba

License: GNU Lesser General Public License version 3 or later. See `COPYING.LESSER.txt` and `COPYING.txt`.
