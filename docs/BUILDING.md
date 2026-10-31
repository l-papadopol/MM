# Building MadModem 0.6.5

Use a C++17 compiler, CMake 3.16+, Qt 5.15 or Qt 6, Hamlib, FFTW3 double precision
and zlib development libraries. Qt modules: Widgets, Network, Multimedia, SerialPort,
Qml/Quick; Location/Positioning provide maps. Qt Help tools build the help collections.
The FT reference sources require compiler support for `-ftrivial-auto-var-init=zero`.

## Ubuntu 24.04 / Mint 22.x

```bash
sudo apt install build-essential cmake ninja-build pkg-config python3 zlib1g-dev \
  libgl1-mesa-dev libhamlib-dev libfftw3-dev qtbase5-dev qttools5-dev qttools5-dev-tools \
  qtdeclarative5-dev qtlocation5-dev qtpositioning5-dev qtmultimedia5-dev \
  libqt5serialport5-dev libqt5opengl5-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON -DHAMLIB_ROOT=/usr \
  -DMADMODEM_REQUIRE_HAMLIB=ON -DMADMODEM_AUTOBUILD_HAMLIB=OFF
cmake --build build --parallel 2
```

Windows uses MSYS2 **UCRT64**, with `scripts/build_windows_msys2.sh`.
macOS uses `scripts/build_macos.sh`; Homebrew dependencies are CMake, Ninja,
pkg-config, Qt/QtLocation, Hamlib and FFTW. The distribution workflow lists the
complete native dependency sets and packaging commands for every platform.

For tests, configure with `-DBUILD_TESTING=ON` and run
`ctest --test-dir build --output-on-failure`. The full GitHub test workflow is manual;
package builds perform deployment checks. See [distribution](DISTRIBUTION.md).
Do not set a public build identity for an ordinary local build: its build number is 0.
