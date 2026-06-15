# MadModem source-origin audit

This file records the source-origin check used for the GitHub README update.

Audit basis:

- `CMakeLists.txt` target source list;
- direct `#include` references to `third_party/`;
- top-level `third_party/` directories;
- comments/notices in the relevant source files.

## Compiled or linked third-party material

The following material is part of the current build path:

- `third_party/mshv_gpl/port/`: FT8/FT4 protocol helpers, generators and related support. These files are listed in `CMakeLists.txt` and included by FT code.
- `third_party/mshv_gpl/port/boost/`: small Boost compatibility headers included through the MSHV port.
- `third_party/hamlib_lgpl/source/`: built by the bundled Hamlib build scripts and linked by MadModem for CAT/PTT/rotator control.
- `third_party/ggmorse_mit/`: added through `add_subdirectory(third_party/ggmorse_mit)` and linked as `ggmorse`.
- `third_party/mmsstv_lgpl/MmsstvRxCore.h/.cpp`: listed in `CMakeLists.txt` and used by `modems/sstv/SstvDecoder.*`.
- `third_party/decodium_gpl/port/NtpClient.hpp/.cpp`: listed in `CMakeLists.txt` and included by `mainwindow.*`.
- `cty.csv`: copied next to the build executable and used by the DXCC/country loader.

## Bundled reference material not compiled into the MadModem target

The following directories are kept in the source tree but are not in the current `MadModem` target source list:

- `third_party/qsstv_gpl/reference/`;
- `third_party/mmsstv_lgpl/reference/`;
- `third_party/mshv_gpl/reference/`;
- `third_party/mshv_gpl/upstream_2765/`;
- `third_party/decodium_gpl/reference/`.

In particular, QSSTV material is bundled as reference/attribution material. The current `SoundCardCalibrationDialog` is a MadModem source file that follows the QSSTV-style calibration idea; the QSSTV reference tree is not compiled or linked into the MadModem executable by the current CMake target.

## Direct third-party includes outside `third_party/`

Observed direct includes:

- `mainwindow.cpp/.h` include `third_party/decodium_gpl/port/NtpClient.hpp`;
- `tx/Ft8Transmitter.cpp` includes MSHV FT generator headers;
- `modems/ft8/Ft8RxDecoder.*` includes MSHV FT support headers;
- `modems/cw/CwDecoder.cpp` includes `ggmorse/ggmorse.h`;
- `modems/sstv/SstvDecoder.h` includes `third_party/mmsstv_lgpl/MmsstvRxCore.h`.

No direct include of QSSTV source files was found in the current application sources.
