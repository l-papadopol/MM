#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
main = (root / 'mainwindow.cpp').read_text()
hdr = (root / 'mainwindow.h').read_text()
settings_h = (root / 'settings/AppSettings.h').read_text()
settings_cpp = (root / 'settings/AppSettings.cpp').read_text()
wf_cpp = (root / 'widgets/WaterfallWidget.cpp').read_text()
wf_h = (root / 'widgets/WaterfallWidget.h').read_text()

checks = [
    ('caller queue setting exists', 'bool ft8QueueCallers = false;' in settings_h),
    ('caller queue setting loads', 'settings.value("FT8/queueCallers"' in settings_cpp),
    ('caller queue setting saves', 'settings.setValue("FT8/queueCallers"' in settings_cpp),
    ('caller queue checkbox exists', 'm_chkFt8CallerQueue = new QCheckBox' in main),
    ('session QSO counter exists', 'm_ftSessionQsoCount' in hdr and '++m_ftSessionQsoCount;' in main),
    ('FIFO uses first queued caller', 'const Ft8QueuedCaller queued = m_ft8CallerQueue.first();' in main and 'm_ft8CallerQueue.removeAt(0);' in main),
    ('queue feeds normal sequencer', 'processFt8SequencerDecode(queued.latestDecode);' in main),
    ('queue uses shared opposite-period selector', 'selectFt8OppositePeriodFromDecode(queued.latestDecode);' in main),
    ('terminal acks do not resurrect queued QSO', 'removeFt8QueuedCaller(call);\n        return;' in main),
    ('queued direct callers outrank Auto QSO CQ', 'priority over answering a new generic CQ' in main),
    ('legacy parked late-reply fallback removed', 'processParkedFt8LateReplies' not in main and 'm_ft8ParkedLateReplies' not in hdr),
    ('waterfall detects minimized top-level', 'topLevel->isMinimized()' in wf_cpp),
    ('waterfall drops hidden/minimized display rows', 'display-only FFT rows are discarded while GL history is preserved' in wf_cpp),
    ('waterfall preserves GL history on restore', 'existing GL history preserved' in wf_cpp and 'm_presentationSuspended' in wf_h),
    ('waterfall resize does not recreate GL history', 'Keep the circular texture and write position stable across QWidget' in wf_cpp),
    ('waterfall drains queued FFT rows with fixed time geometry', 'const int uploadBudget = queued;' in wf_cpp),
]

failed = False
for label, ok in checks:
    print(('PASS' if ok else 'FAIL') + ': ' + label)
    failed |= not ok

# Translation parity + new keys.
files = sorted((root / 'translations').glob('ui_*.ini'))
required = {'ft8_queue_callers','ft8_queue_callers_tooltip','ft8_session_stats','ft8_session_stats_tooltip'}
keysets = {}
for p in files:
    keys = []
    for line in p.read_text().splitlines():
        if line and not line.startswith(('#',';','[')) and '=' in line:
            keys.append(line.split('=',1)[0])
    keysets[p.name] = set(keys)
    ok = required.issubset(keysets[p.name]) and len(keys) == len(set(keys))
    print(('PASS' if ok else 'FAIL') + f': translations {p.name} include queue/session keys without duplicates')
    failed |= not ok
if 'ui_en.ini' in keysets:
    base = keysets['ui_en.ini']
    for name, keys in keysets.items():
        ok = keys == base
        print(('PASS' if ok else 'FAIL') + f': translation key parity {name}')
        failed |= not ok

if failed:
    sys.exit(1)
print('FT caller queue + stable waterfall source audit: PASS')
