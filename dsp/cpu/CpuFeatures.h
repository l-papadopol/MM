#ifndef CPUFEATURES_H
#define CPUFEATURES_H

#include <QString>

namespace MadModemCpu {

struct Features
{
    bool sse2 = false;
    bool avx2 = false;
    bool fma = false;
    bool avx512f = false;
    bool neon = false;
};

Features detect();
QString summary();
QString ft8ToneEngineName();

} // namespace MadModemCpu

#endif // CPUFEATURES_H
