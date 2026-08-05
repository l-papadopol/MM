# MadModem 0.5.78 source-origin audit

Audit basis:

- `CMakeLists.txt` target source list;
- direct `#include` references to `third_party/`;
- top-level `third_party/` directories;
- license headers and `THIRD_PARTY_NOTICES.md`.

## Native MadModem code

The active CW receive code under `modems/cw/skimmer/` is a single native
clean-room implementation. It does not compile, copy or adapt an external CW
decoder. The CMake target contains `CwSkimmerEngine`,
`SelectedToneCwTracker`, `CwCarrierDiscriminator`, `CwRelativeTimingTask` and
`CwRelativeTimingDecoder` plus their headers.

## Compiled or linked third-party material

- `third_party/mshv_gpl/port/`: FT/Q65/MSK protocol helpers selected by CMake;
- `third_party/hamlib_lgpl/source/`: CAT/PTT/rotator control;
- `third_party/mmsstv_lgpl/MmsstvRxCore.h/.cpp`: SSTV RX helper;
- `third_party/decodium_gpl/port/NtpClient.hpp/.cpp`: Qt NTP client;
- `cty.csv`: DXCC/country-prefix data.

## Bundled reference material not compiled into MadModem

- `third_party/qsstv_gpl/reference/`;
- `third_party/mmsstv_lgpl/reference/`;
- `third_party/mshv_gpl/reference/`;
- `third_party/mshv_gpl/upstream_2765/`;
- `third_party/decodium_gpl/reference/`.

These directories are excluded from the MadModem target and retained only where
required for attribution or protocol/source traceability.

## Direct includes

- `mainwindow.cpp/.h` include the Decodium NTP port;
- FT files include MSHV-derived FT generator/support headers;
- `modems/sstv/SstvDecoder.h` includes the MMSSTV-derived RX helper;
- `modems/cw/CwDecoder.cpp` includes only MadModem-native CW headers.

Test media and MM Flow Studio are not included in the production source package.

## Native runtime/resource additions

`utils/SystemResourceManager.*`, the persistent FT coordinator and the circular
OpenGL waterfall renderer are native MadModem C++ code. They introduce no new
third-party runtime library. The FT8 FFT backend remains the existing in-tree
radix-2 implementation; bundled FFTW headers remain part of MSHV/Q65 reference
or optional support and are not newly linked into FT8.
