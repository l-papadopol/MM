#include "modems/ft8/FtSlotMemory.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <random>

using Memory = MadModemFt::FtSlotMemory;

static void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

int main()
{
    std::mt19937 rng(531);
    std::array<int, Memory::bitCount> word{};
    Memory::Evidence candidate;
    candidate.frequencyHz = 1500.;
    candidate.startSec = .5;
    candidate.hardSync = 12;
    for (std::size_t i = 0; i < word.size(); ++i) {
        word[i] = rng() & 1;
        candidate.llr[i] = word[i] ? -3.0f : 3.0f;
    }

    auto unrelated = candidate;
    for (std::size_t i = 91; i < word.size(); ++i) unrelated.llr[i] = -candidate.llr[i];
    auto wrongTime = candidate;
    wrongTime.startSec = .59;
    auto wrongFrequency = candidate;
    wrongFrequency.frequencyHz = 1510.;
    auto silence = candidate;
    silence.llr.fill(0.);
    auto evidenceInOneHalf = candidate;
    for (std::size_t i = 132; i < word.size(); ++i) evidenceInOneHalf.llr[i] *= -1.;
    check(Memory::supports(candidate, word), "valid observation fails independent word check");
    check(!Memory::supports(unrelated, word), "unrelated parity is accepted");
    check(!Memory::supports(silence, word), "silence supports a word");
    check(!Memory::supports(evidenceInOneHalf, word), "a single parity half suffices");

    for (const int period : {7500, 15000}) {
        Memory memory;
        Memory::Context context{1, 2, 3, period, 100, 3000};
        const auto slot = 100LL * period;
        memory.remember(context, slot, {candidate});
        const auto match = [&](std::int64_t offset, const Memory::Evidence &evidence) {
            return memory.match(context, slot + offset * period, evidence);
        };
        check(match(2, candidate), "same-parity repeat is missed");
        check(!match(0, candidate), "same slot is offered twice");
        check(!match(1, candidate), "opposite parity is mixed");
        check(!match(4, candidate), "stale observation is offered");
        check(!match(-2, candidate), "future observation leaks backward");
        check(!match(2, unrelated), "different waveform matches at the same coordinates");
        check(!match(2, wrongTime), "time tolerance is ignored");
        check(!match(2, wrongFrequency), "frequency tolerance is ignored");
        check(!match(2, silence), "silence matches history");
        for (const auto &changed : {Memory::Context{2, 2, 3, period, 100, 3000},
                                    Memory::Context{1, 4, 3, period, 100, 3000},
                                    Memory::Context{1, 2, 4, period, 100, 3000},
                                    Memory::Context{1, 2, 3, 22500 - period, 100, 3000},
                                    Memory::Context{1, 2, 3, period, 130, 3000}}) {
            check(!memory.match(changed, slot + 2 * period, candidate), "RX context leaks into another capture");
        }
        memory.remember(context, slot + 2 * period, {});
        check(!match(4, candidate), "empty RX slot does not replace old evidence");
        memory.remember(context, slot, {candidate});
        check(!match(4, candidate), "out-of-order slot overrides the latest frame");
    }
    std::cout << "FT8 and FT4 slot memory: parity, age, independent evidence and RX context verified\n";
}
