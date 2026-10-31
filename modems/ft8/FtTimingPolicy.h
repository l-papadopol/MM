// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
namespace MadModemFtTiming {
constexpr int pttPrearmLeadMs = 650;
constexpr int lateStartPreparationMs = 700;
constexpr int generatedSignalAllowanceMs = 120;
constexpr int slotEndGuardMs = 200;
// Measured dense UI + sequencing + waveform work was below 136 ms here.
// Keep a separate dispatch reserve; hardware preparation is already budgeted
// by lateStartPreparationMs and still enforced by the TX start guards.
constexpr int guiDispatchReserveMs = 150;
constexpr int recoveryFinishGuardMs = 40;
// Last first-PCM instant that leaves room for the intact generated frame.
// This is distinct from the desired first-tone time and from admission time.
constexpr int latestFullFrameStartMs(int slotMs, int signalMs) {
    return slotMs > signalMs + generatedSignalAllowanceMs + slotEndGuardMs
        ? slotMs - signalMs - generatedSignalAllowanceMs - slotEndGuardMs : 0;
}
constexpr int latestFullFrameArmMs(int slotMs, int signalMs) {
    return latestFullFrameStartMs(slotMs, signalMs) > lateStartPreparationMs
        ? latestFullFrameStartMs(slotMs, signalMs) - lateStartPreparationMs : 0;
}
constexpr std::int64_t replyArmDeadlineUtcMs(std::int64_t rxSlotStartMs, int slotMs, int signalMs) {
    return rxSlotStartMs + slotMs + latestFullFrameArmMs(slotMs, signalMs);
}
constexpr std::int64_t recoveryDeadlineUtcMs(std::int64_t rxSlotStartMs, int slotMs, int signalMs) {
    return replyArmDeadlineUtcMs(rxSlotStartMs, slotMs, signalMs)
        - guiDispatchReserveMs - recoveryFinishGuardMs;
}
}
