#include "CpuFeatures.h"

#include <QStringList>

#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
#define MADMODEM_HAVE_GNU_X86_CPU_DISPATCH 1
#endif

#if defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__)
#define MADMODEM_COMPILETIME_SSE2 1
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define MADMODEM_COMPILETIME_NEON 1
#endif

namespace MadModemCpu {

Features detect()
{
    Features f;

#if defined(MADMODEM_COMPILETIME_SSE2)
    f.sse2 = true;
#endif
#if defined(MADMODEM_COMPILETIME_NEON)
    f.neon = true;
#endif

#if defined(MADMODEM_HAVE_GNU_X86_CPU_DISPATCH)
    __builtin_cpu_init();
    f.sse2 = __builtin_cpu_supports("sse2") != 0;
    f.avx2 = __builtin_cpu_supports("avx2") != 0;
    f.fma = __builtin_cpu_supports("fma") != 0;
    f.avx512f = __builtin_cpu_supports("avx512f") != 0;
#endif

    return f;
}

QString ft8ToneEngineName()
{
    const Features f = detect();
    if (f.avx2 && f.fma) {
        return QStringLiteral("AVX2/FMA");
    }
    if (f.sse2) {
        return QStringLiteral("SSE2");
    }
    if (f.neon) {
        return QStringLiteral("NEON-ready scalar fallback");
    }
    return QStringLiteral("portable scalar");
}

QString summary()
{
    const Features f = detect();
    QStringList parts;
    parts << QStringLiteral("SSE2 %1").arg(f.sse2 ? QStringLiteral("yes") : QStringLiteral("no"));
    parts << QStringLiteral("AVX2 %1").arg(f.avx2 ? QStringLiteral("yes") : QStringLiteral("no"));
    parts << QStringLiteral("FMA %1").arg(f.fma ? QStringLiteral("yes") : QStringLiteral("no"));
    parts << QStringLiteral("AVX-512F %1").arg(f.avx512f ? QStringLiteral("yes") : QStringLiteral("no"));
    parts << QStringLiteral("NEON %1").arg(f.neon ? QStringLiteral("yes") : QStringLiteral("no"));
    parts << QStringLiteral("FT8 tone engine: %1").arg(ft8ToneEngineName());
    return parts.join(QStringLiteral(", "));
}

} // namespace MadModemCpu
