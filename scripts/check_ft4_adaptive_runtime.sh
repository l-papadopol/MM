#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
rx="$root/modems/ft8/Ft8RxDecoder.cpp"
rmgr="$root/utils/SystemResourceManager.cpp"
main="$root/mainwindow.cpp"

# New capture/mode sessions must restore the topology budget instead of
# inheriting an old one-worker FT8 target.
grep -q 'beginFtCapture(m_modeName)' "$rx"
grep -q 'capture reset to topology budget' "$rmgr"

# The controller must use the current queue sample, not the slot maximum that
# can contain one harmless backend scheduling spike.
[[ "$(grep -c 'stats != nullptr ? stats->currentCaptureQueueLatencyMs : 0.0' "$rx")" -eq 2 ]]

# FT4 uses the FT8-style atomic work stealing and a strict one-pass live gate.
grep -q 'std::atomic<int> nextCandidate' "$rx"
grep -q 'FT4 adaptive atomic live engine' "$rx"
grep -A18 'const int requestedPasses = gateCandidateSet' "$rx" | grep -q '? 1'

# Temporary per-row FT4 diagnostics defeated GUI batching and must stay gone.
if grep -q 'FT4 display path:' "$main"; then
    echo 'ERROR: per-row FT4 display logging returned' >&2
    exit 1
fi
if grep -q 'FT4 display diagnostic: waiting' "$main"; then
    echo 'ERROR: stale FT4 slot-key mismatch diagnostic returned' >&2
    exit 1
fi

echo 'FT4 adaptive runtime source audit: PASS'
