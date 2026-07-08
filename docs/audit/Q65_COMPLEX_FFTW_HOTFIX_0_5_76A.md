# Q65 Complex/FFTW hotfix — 0.5.76a

The GCC build reported failures in the full MSHV Q65 RX bridge:

- `decoderpom.cpp`: `double complex*` could not be passed to FFTW APIs expecting `fftw_complex*` (`double (*)[2]`).
- `decoderpom.cpp` and `decoderq65.cpp`: `I`, `creal`, `cimag`, and `conj` were not available when compiling MSHV C99/GNU complex style code as C++.

Fix:

- Added `mshv_complex_compat.h`, local to `HvDecoderMs`, defining GNU C++ compatible complex helpers.
- Replaced C99 `<complex.h>` usage in `decoderpom.h` and `decoderq65.h` with that shim.
- Added explicit `reinterpret_cast<fftw_complex*>` conversions for FFTW plan creation in `decoderpom.cpp`.

This is a compile-port fix only. It does not alter Q65 UI, MSK144, MIND, or decoder policy.
