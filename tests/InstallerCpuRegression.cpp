#define MADMODEM_CPU_PROBE_TEST
#include "../packaging/windows/cpu_probe.c"
#include <cstdio>
#include <initializer_list>

int main() {
    const unsigned required = (1u << 12) | (1u << 20) | (1u << 26) | (1u << 27) | (1u << 28);
    bool ok = supports_avx2(required, 1u << 5, 6);
    for (unsigned bit : {12u, 20u, 26u, 27u, 28u})
        ok &= !supports_avx2(required & ~(1u << bit), 1u << 5, 6);
    ok &= !supports_avx2(required, 0, 6);
    for (unsigned state : {0u, 2u, 4u}) ok &= !supports_avx2(required, 1u << 5, state);
    std::puts(ok ? "PASS CPU and OS AVX2/FMA eligibility, including partial OS state" : "FAIL CPU selection");
    return ok ? 0 : 1;
}
