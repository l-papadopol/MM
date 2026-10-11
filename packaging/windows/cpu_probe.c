/* Built for baseline x86-64. Exit 0 only if both CPU and OS support every
   instruction-set extension used by MADMODEM_AVX2_BUILD. */
#include <stdint.h>

static int supports_avx2(unsigned int ecx, unsigned int ebx7, uint64_t xcr0)
{
    const unsigned int required = (1u << 12) | (1u << 20) | (1u << 26) |
                                  (1u << 27) | (1u << 28); /* FMA SSE4.2 XSAVE OSXSAVE AVX */
    return (ecx & required) == required && (ebx7 & (1u << 5)) && (xcr0 & 6u) == 6u;
}
#ifndef MADMODEM_CPU_PROBE_TEST
#include <cpuid.h>
int main(void)
{
    unsigned int eax, ebx, ecx, edx, lo, hi, leaf7ecx, leaf7edx;
    if (__get_cpuid_max(0, 0) < 7 || !__get_cpuid(1, &eax, &ebx, &ecx, &edx)) return 1;
    if (!(ecx & (1u << 27))) return 1; /* XGETBV itself requires OSXSAVE. */
    __asm__ volatile ("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    __cpuid_count(7, 0, eax, ebx, leaf7ecx, leaf7edx);
    return supports_avx2(ecx, ebx, lo) ? 0 : 1;
}
#endif
