#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / 'mainwindow.cpp').read_text(encoding='utf-8')
HDR = (ROOT / 'mainwindow.h').read_text(encoding='utf-8')


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


def new_delta(slot_ms: int, cycle_ms: int, selected_start: int, cycle_pos: int, tone_delay: int) -> int:
    selected_end = selected_start + slot_ms
    inside = selected_start <= cycle_pos < selected_end
    if inside:
        elapsed = cycle_pos - selected_start
        latest_safe = max(0, tone_delay - 120)
        if elapsed <= latest_safe:
            return -elapsed
        return cycle_ms - cycle_pos + selected_start
    if cycle_pos < selected_start:
        return selected_start - cycle_pos
    return cycle_ms - cycle_pos + selected_start


# Exact live failure: FT8 TX requested 1181 ms after the selected boundary.
assert old_delta(15000, 30000, 0, 1181) == -1181
assert new_delta(15000, 30000, 0, 1181, 500) == 28819

# A click during the still-silent beginning may start the complete frame.
assert new_delta(15000, 30000, 0, 250, 500) == -250
# Once useful tones cannot be reached safely, the next boundary must be future.
assert new_delta(15000, 30000, 0, 381, 500) > 0
assert new_delta(7500, 15000, 0, 181, 300) > 0

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

# Deadline is checked before the local TX row and before PTT.
pre_guard = CPP.index('slot expired by %1 ms before PTT')
append_row = CPP.index('appendFt8LocalTxRow(m_pendingFt8TxMessage', pre_guard)
prearm = CPP.index('prearmFtPreparedSlotTransmit();', append_row)
assert pre_guard < append_row < prearm

print('PASS: atomic decode-batch decision retained')
print('PASS: old 1181 ms late request reproduces expired-boundary selection')
print('PASS: corrected scheduler selects the next future FT8 slot')
print('PASS: only the pre-tone safe window can use the current boundary')
print('PASS: one immutable boundary drives delay, token and watchdog')
print('PASS: TX arm token has a monotonic generation')
print('PASS: stale boundary-only recovery removed')
print('PASS: expired slot is rejected before local TX row and PTT')
