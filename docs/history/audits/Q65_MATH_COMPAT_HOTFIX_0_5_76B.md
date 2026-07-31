# Q65 math compatibility hotfix — 0.5.76b

The MSHV-derived Q65 decoder bridge is C++ code translated from Fortran/C-style DSP routines.
After the 0.5.76a complex/FFTW fix, GCC reached the next class of issues: unqualified C math functions in `decoderpom.cpp`.

Fixed by adding explicit C++ math includes and imports in the Q65 bridge translation units:

- `<cmath>`
- `<cstdlib>`
- `using std::log10;`
- `using std::fabs;`
- `using std::atan;`
- `using std::cos;`
- `using std::sqrt;`
- `using std::pow;`
- `using std::sin;`
- `using std::fmod;`
- `using std::tanh;`

No UI behavior was changed.
