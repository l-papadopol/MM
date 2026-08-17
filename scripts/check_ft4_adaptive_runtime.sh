#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
rx="$root/modems/ft8/Ft8RxDecoder.cpp"
rxh="$root/modems/ft8/Ft8RxDecoder.h"
rmgr="$root/utils/SystemResourceManager.cpp"
main="$root/mainwindow.cpp"

# One deterministic, topology-derived live budget.  The proven 12-thread
# configuration reserves two logical processors, uses 40% of the remaining
# live budget for the gate, and keeps runtime observations telemetry-only.
grep -q 'beginFtCapture(m_modeName)' "$rx"
grep -q 'm_reservedLogicalProcessors = qMax(2' "$rmgr"
grep -q 'm_maxLiveWorkers = qMax(1, logical - m_reservedLogicalProcessors)' "$rmgr"
grep -A7 'case WorkClass::FtGate:' "$rmgr" | grep -q 'live) \* 0.40'
grep -q 'fixed live topology budget' "$rmgr"

# The extra deadline work must live in the real FT4 decoder, before the
# boundary-only subtraction path. It is narrow, candidate-bounded and tied to
# the selected correspondent context; it is not another wideband fallback.
grep -q 'QVector<Ft8RxDecoder::Decode> Ft8RxDecoder::decodeSlotFt4' "$rx"
grep -q 'qsoDeadlinePassEnabled = gateCandidateSet' "$rx"
grep -q 'm_qsoDeadlineActive.load' "$rx"
grep -q 'setQsoDeadlineActive(bool active)' "$rx"
grep -q 'kFt4DeadlinePassLatestStartMs = 260.0' "$rx"
grep -q 'kFt4DeadlinePassBudgetMs = 520.0' "$rx"
grep -A4 'findFt4DeadlineCandidates(samples,' "$rx" | grep -q '180'
grep -A4 'findFt4DeadlineCandidates(samples,' "$rx" | grep -q '220'
grep -q 'findFt4DeadlineCandidates(const QVector<double> &samples,' "$rxh"
grep -q 'int maxCandidates)' "$rxh"

# FT4 candidate decode uses atomic work stealing inside the bounded worker set.
grep -q 'std::atomic<int> nextCandidate' "$rx"

# Temporary per-row diagnostics defeated GUI batching and must stay gone.
if grep -q 'FT4 display path:' "$main"; then
    echo 'ERROR: per-row FT4 display logging returned' >&2
    exit 1
fi
if grep -q 'FT4 display diagnostic: waiting' "$main"; then
    echo 'ERROR: stale FT4 slot-key mismatch diagnostic returned' >&2
    exit 1
fi

echo 'FT4 proven-topology deadline runtime source audit: PASS'
