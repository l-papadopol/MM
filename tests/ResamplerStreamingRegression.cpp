#include "dsp/text/LinearResampler.h"
#include <cmath>
#include <iostream>
int main() {
    bool ok = true;
    for (const int rate : {44100, 48000, 96000}) {
        QVector<float> input(rate / 4);
        for (int i = 0; i < input.size(); ++i) input[i] = 0.5f * std::sin(6.283185307179586 * 1000 * i / rate);
        LinearResampler whole, split;
        whole.configure(12000); split.configure(12000);
        const auto expected = whole.process(input, rate);
        QVector<double> actual;
        for (int offset = 0; offset < input.size(); offset += 137)
            actual += split.process(input.mid(offset, 137), rate);
        bool same = actual.size() == expected.size();
        for (int i = 0; same && i < actual.size(); ++i) same = std::abs(actual[i] - expected[i]) < 1e-12;
        split.reset();
        const auto restarted = split.process(input, rate);
        same &= restarted == expected && whole.delayNanoseconds() > 0;
        std::cout << (same ? "PASS " : "FAIL ") << rate << " split/reset/delay\n";
        ok &= same;
    }
    return ok ? 0 : 1;
}
