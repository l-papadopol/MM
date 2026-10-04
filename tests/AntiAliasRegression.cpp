#include "dsp/text/AntiAliasFilter.h"
#include <cmath>
#include <iostream>

int main() {
    constexpr double pi = 3.14159265358979323846;
    bool ok = true;
    for (int rate : {44100, 48000, 96000}) {
        for (double frequency : {1000.0, 11000.0}) {
            AntiAliasFilter filter;
            filter.configure(rate, 12000);
            double energy = 0;
            int count = 0;
            for (int n = 0; n < rate; ++n) {
                const double y = filter.process(std::sin(2 * pi * frequency * n / rate));
                if (n > rate / 2) { energy += y * y; ++count; }
            }
            const double gain = std::sqrt(2 * energy / count);
            const bool pass = frequency < 6000 ? std::abs(gain - 1) < 0.01 : gain < 0.001;
            std::cout << (pass ? "PASS " : "FAIL ") << rate << " Hz, tone " << frequency << ", gain " << gain << '\n';
            ok &= pass;
        }
    }
    return ok ? 0 : 1;
}
