// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace MadModemFt {

class FtSlotMemory
{
public:
    static constexpr int bitCount = 174;
    static constexpr int maxCandidates = 96;
    static constexpr int firstParityBit = 91;

    struct Context {
        int generation = 0;
        std::uint64_t tuningGeneration = 0;
        std::uint64_t blankerGeneration = 0;
        int periodMs = 0;
        int lowHz = 0;
        int highHz = 0;

        bool operator==(const Context &other) const
        {
            return generation == other.generation &&
                   tuningGeneration == other.tuningGeneration &&
                   blankerGeneration == other.blankerGeneration &&
                   periodMs == other.periodMs && lowHz == other.lowHz && highHz == other.highHz;
        }
    };

    struct Evidence {
        std::array<float, bitCount> llr{};
        double frequencyHz = 0.0;
        double startSec = 0.0;
        double sync = 0.0;
        int hardSync = 0;
    };

    static bool nearby(const Evidence &a, const Evidence &b, int periodMs)
    {
        const double frequencyTolerance = periodMs == 7500 ? 4.5 : 2.5;
        return std::abs(a.frequencyHz - b.frequencyHz) <= frequencyTolerance &&
               std::abs(a.startSec - b.startSec) <= .055;
    }

    const Evidence *match(const Context &context, std::int64_t slotUtcMs,
                          const Evidence &current) const
    {
        if (context.periodMs <= 0 || current.hardSync == 0) return nullptr;
        const auto &frame = m_frames[(slotUtcMs / context.periodMs) & 1];
        if (!(frame.context == context) || slotUtcMs - frame.slotUtcMs != 2LL * context.periodMs) return nullptr;

        // The parity bits decorrelate different payloads with a common CQ
        // prefix. Match at most one observation from the preceding RX period.
        double best = .12;
        const Evidence *match = nullptr;
        for (const Evidence &previous : frame.candidates) {
            if (!nearby(previous, current, context.periodMs)) continue;
            double dot = 0, oldPower = 0, newPower = 0;
            for (int i = firstParityBit; i < bitCount; ++i) {
                const double old = previous.llr[i], now = current.llr[i];
                dot += old * now;
                oldPower += old * old;
                newPower += now * now;
            }
            if (oldPower <= 0 || newPower <= 0) continue;
            const double score = dot / std::sqrt(oldPower * newPower);
            if (score > best) {
                best = score;
                match = &previous;
            }
        }
        return match;
    }

    static bool supports(const Evidence &frame, const std::array<int, bitCount> &codeword)
    {
        // Require independent support from each slot, including evidence in
        // both halves of the parity block. A valid CRC on the sum is not proof
        // that an older message was sent in the current period.
        int agreements[2]{};
        double signedEvidence[2]{}, absoluteEvidence[2]{};
        constexpr int parityMiddle = firstParityBit + (bitCount - firstParityBit) / 2;
        for (int i = firstParityBit; i < bitCount; ++i) {
            const int half = i >= parityMiddle;
            const double signedLlr = codeword[i] ? -frame.llr[i] : frame.llr[i];
            agreements[half] += signedLlr > 0;
            signedEvidence[half] += std::clamp(signedLlr, -5., 5.);
            absoluteEvidence[half] += std::min(std::abs(signedLlr), 5.);
        }
        return agreements[0] >= 23 && agreements[1] >= 23 &&
               agreements[0] + agreements[1] >= 50 &&
               signedEvidence[0] > .12 * absoluteEvidence[0] &&
               signedEvidence[1] > .12 * absoluteEvidence[1];
    }

    void remember(const Context &context, std::int64_t slotUtcMs, std::vector<Evidence> candidates)
    {
        if (context.periodMs <= 0) return;
        auto &frame = m_frames[(slotUtcMs / context.periodMs) & 1];
        if (frame.context == context && frame.slotUtcMs > slotUtcMs) return;
        frame.context = context;
        frame.slotUtcMs = slotUtcMs;
        if (candidates.size() > maxCandidates) candidates.resize(maxCandidates);
        frame.candidates = std::move(candidates);
    }

private:
    struct Frame {
        Context context;
        std::int64_t slotUtcMs = 0;
        std::vector<Evidence> candidates;
    };
    std::array<Frame, 2> m_frames;
};

} // namespace MadModemFt
