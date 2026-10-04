# Bundled Hamlib for MadModem

MadModem v1.08 includes Hamlib source so CAT/PTT can be built all-in-one on
Linux and Windows.

## Native Linux

```bash
./third_party/hamlib_lgpl/build_hamlib.sh
```

Default install prefix:

```text
third_party/hamlib_lgpl/install-linux-x86_64
```

## Windows via MXE

```bash
export MXE_ROOT=/home/iz6nnh/mxe
export MXE_TARGET=x86_64-w64-mingw32.static
./third_party/hamlib_lgpl/build_hamlib_mxe.sh
```

Default install prefix:

```text
third_party/hamlib_lgpl/install-win64-static
```

## Linking MadModem

Pass the matching prefix to CMake:

```bash
cmake -S . -B build-linux -DHAMLIB_ROOT="$PWD/third_party/hamlib_lgpl/install-linux-x86_64" -DMADMODEM_REQUIRE_HAMLIB=ON
```

The normal top-level helpers do this automatically:

```bash
make linux
make win64-static
./build_all.sh
```

## Licensing

Hamlib library code is LGPL2.1-or-later. Some upstream utilities/examples in the
full Hamlib source tree are GPL2-or-later. MadModem as a combined package is
GPLv3 because other bundled components are GPLv3.


## MXE note

For Windows/MXE builds, pass the target pkg-config executable to the MadModem CMake configure step:

```bash
-DPKG_CONFIG_EXECUTABLE="$MXE_ROOT/usr/bin/$MXE_TARGET-pkg-config"
```

v1.08 also checks `HAMLIB_ROOT` directly for `include/hamlib/rig.h` and `lib/libhamlib.a`, which avoids MXE root-path search misses.
