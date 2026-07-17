/* MadModem/MSHV C++ complex compatibility shim.
 *
 * The imported MSHV decoder uses GNU/C99 complex arithmetic.  Earlier
 * revisions exposed compatibility through global preprocessor macros named
 * `complex`, `I`, `creal`, `cimag`, `conj` and `cabs`.  In Qt 6 unity/MOC
 * translation units those macros leaked into Qt headers; the one-letter `I`
 * macro, for example, rewrote Qt template parameters in QSize and broke the
 * AppleClang build.
 *
 * Keep the GNU complex representation, but expose only MadModem-prefixed
 * types and inline helpers.  No generic macro is allowed to escape this file.
 */
#ifndef MSHV_COMPLEX_COMPAT_H
#define MSHV_COMPLEX_COMPAT_H

#if defined(__cplusplus)
using mshv_complex = __complex__ double;
#else
typedef double _Complex mshv_complex;
#endif

static inline mshv_complex mshv_make_complex(double re, double im)
{
    mshv_complex z;
    __real__ z = re;
    __imag__ z = im;
    return z;
}

static inline mshv_complex mshv_i(void)
{
    return mshv_make_complex(0.0, 1.0);
}

static inline double mshv_creal(mshv_complex z)
{
    return __real__ z;
}

static inline double mshv_cimag(mshv_complex z)
{
    return __imag__ z;
}

static inline mshv_complex mshv_conj(mshv_complex z)
{
    return mshv_make_complex(__real__ z, -__imag__ z);
}

static inline double mshv_cabs(mshv_complex z)
{
    return __builtin_hypot(__real__ z, __imag__ z);
}

#endif // MSHV_COMPLEX_COMPAT_H
