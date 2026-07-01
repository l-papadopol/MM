/* MadModem/MSHV C++ complex compatibility shim.
 * MSHV's translated decoder code uses C99/GNU complex syntax such as
 *   double complex z, I, creal(), cimag(), conj()
 * while this port is compiled as C++.  GCC supports GNU complex values in
 * C++, but the C99 convenience macros/functions are not reliably exposed by
 * <complex.h>/<ccomplex>.  Keep this shim local to the MSHV decoder port.
 */
#ifndef MSHV_COMPLEX_COMPAT_H
#define MSHV_COMPLEX_COMPAT_H

#if defined(__cplusplus)

#ifndef complex
#define complex __complex__
#endif

static inline double complex mshv_make_complex(double re, double im)
{
    double complex z;
    __real__ z = re;
    __imag__ z = im;
    return z;
}

#ifndef I
#define I (mshv_make_complex(0.0, 1.0))
#endif

#ifndef creal
#define creal(z) (__real__(z))
#endif

#ifndef cimag
#define cimag(z) (__imag__(z))
#endif

#ifndef conj
#define conj(z) (mshv_make_complex(__real__(z), -__imag__(z)))
#endif

#else
#include <complex.h>
#endif

#endif // MSHV_COMPLEX_COMPAT_H
