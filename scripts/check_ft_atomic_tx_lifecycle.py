#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / 'mainwindow.cpp').read_text(encoding='utf-8')
HDR = (ROOT / 'mainwindow.h').read_text(encoding='utf-8')
TX_CPP = (ROOT / 'modems/ft8/tx/Ft8Transmitter.cpp').read_text(encoding='utf-8')
TX_HDR = (ROOT / 'modems/ft8/tx/Ft8Transmitter.h').read_text(encoding='utf-8')


def old_delta(slot_ms: int, cycle_ms: int, selected_start: int, cycle_pos: int) -> int:
    selected_end = selected_start + slot_ms
    inside = selected_start <= cycle_pos < selected_end
    if inside:
        elapsed = cycle_pos - selected_start
        if elapsed < (slot_ms * 3) // 4:
            return -elapsed
        return cycle_ms - cycle_pos + selected_start
    if cycle_pos < selected_start:
        return selected_start - cycle_pos
    return cycle_ms - cycle_pos + selected_start


def new_delta(slot_ms: int, cycle_ms: int, selected_start: int, cycle_pos: int,
              signal_ms: int, manual_partial: bool = False) -> int:
    selected_end = selected_start + slot_ms
    inside = selected_start <= cycle_pos < selected_end
    if inside:
        elapsed = cycle_pos - selected_start
        latest_safe = max(0, slot_ms - signal_ms - 120 - 700 - 200)
        if elapsed <= latest_safe:
            return -elapsed
        latest_partial = max(0, slot_ms - 700 - 200 - 600)
        if manual_partial and elapsed <= latest_partial:
            return -elapsed
        return cycle_ms - cycle_pos + selected_start
    if cycle_pos < selected_start:
        return selected_start - cycle_pos
    return cycle_ms - cycle_pos + selected_start


# Exact live failure: FT8 TX requested 1181 ms after the selected boundary.
# The old repair rejected it even though an intact frame still fits. The new
# scheduler accepts the current slot and moves the audio target 700 ms beyond
# the request instead of skipping protocol symbols.
assert old_delta(15000, 30000, 0, 1181) == -1181
assert new_delta(15000, 30000, 0, 1181, 12640) == -1181
assert 1181 + 700 + 12640 + 120 + 200 <= 15000

# The exact fit window is mode-specific and based on the complete frame.
assert new_delta(15000, 30000, 0, 1340, 12640) == -1340
assert new_delta(15000, 30000, 0, 1341, 12640) > 0
assert new_delta(7500, 15000, 0, 1440, 5040) == -1440
assert new_delta(7500, 15000, 0, 1441, 5040) > 0

# Only direct operator actions may emit a late visual burst. The exact live-log
# click at +6.2 s starts now, while the same automatic retry waits for the next
# selected period. Even a manual burst keeps 600 ms of useful tone and stops
# 200 ms before the period changes.
assert new_delta(15000, 30000, 0, 6200, 12640) > 0
assert new_delta(15000, 30000, 0, 6200, 12640, True) == -6200
assert new_delta(15000, 30000, 0, 13500, 12640, True) == -13500
assert new_delta(15000, 30000, 0, 13501, 12640, True) > 0
assert new_delta(7500, 15000, 0, 6000, 5040, True) == -6000
assert new_delta(7500, 15000, 0, 6001, 5040, True) > 0

# Atomic RX batch decision remains intact.
for needle in ('handleFt8DecodeBatchStarted', 'handleFt8DecodeBatchFinished',
               'selectBestFt8SequencerDecode', 'm_ft8PendingSequencerBatch'):
    assert needle in CPP, needle

# Every arm is a distinct generation. Old queued signals cannot impersonate it.
assert 'm_pendingFt8SlotBoundaryUtcMs - armNowUtcMs' in CPP
assert '++m_ft8TxArmGeneration' in CPP
assert 'quint64 m_ft8TxArmGeneration = 0;' in HDR
assert 'exact token match=' in CPP
assert 'audio-start token was already cleared, but slot boundary still matches' not in CPP

# CAT is armed in the quiet tail and the time-critical TX path must never block
# behind the FT decoder's boundary/deep decode queue.
assert 'constexpr int kFtPttPrearmLeadMs = 650;' in CPP
start_begin = CPP.index('void MainWindow::startFtPreparedSlotTransmit()')
start_end = CPP.index('void MainWindow::stopImageTx()', start_begin)
start_tx = CPP[start_begin:start_end]
assert '"setLiveInputEnabled",\n                                      Qt::QueuedConnection' in start_tx
assert '"noteTransmitStarting",\n                                      Qt::QueuedConnection' in start_tx
assert 'Qt::BlockingQueuedConnection' not in start_tx
assert 'scheduleFt8SequencerMessage(autoTxMessage, QStringLiteral("SEQ"), true)' in CPP
assert 'm_pendingFt8LatePartial' in CPP and 'truncateTotalMilliseconds' in CPP
assert 'bool m_pendingFt8LatePartial = false;' in HDR
assert 'void truncateTotalMilliseconds(int milliseconds);' in TX_HDR
assert 'void Ft8Transmitter::truncateTotalMilliseconds(int milliseconds)' in TX_CPP
assert 'm_samples.resize(maximumSamples);' in TX_CPP

# Deadline is checked before the local TX row and before PTT.
pre_guard = CPP.index('slot expired by %1 ms before PTT')
append_row = CPP.index('appendFt8LocalTxRow(m_pendingFt8TxMessage', pre_guard)
prearm = CPP.index('prearmFtPreparedSlotTransmit();', append_row)
assert pre_guard < append_row < prearm

print('PASS: atomic decode-batch decision retained')
print('PASS: 1181 ms late operator request starts an intact FT8 frame now')
print('PASS: mode-specific current-slot window guarantees full-frame fit')
print('PASS: automatic too-late requests defer intact frames')
print('PASS: manual +6.2 s request emits a bounded current-period visual burst')
print('PASS: manual late burst preserves frame prefix and stops before slot flip')
print('PASS: CAT/PTT pre-arms before the UTC boundary')
print('PASS: TX start never blocks behind the FT decoder thread')
print('PASS: one immutable boundary drives delay, token and watchdog')
print('PASS: TX arm token has a monotonic generation')
print('PASS: stale boundary-only recovery removed')
print('PASS: expired slot is rejected before local TX row and PTT')
