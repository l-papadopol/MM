#!/usr/bin/env python3
from pathlib import Path
import json, sys
root=Path(__file__).resolve().parents[1]
errors=[]
def must(path, needles):
    text=(root/path).read_text(encoding='utf-8', errors='replace')
    for n in needles:
        if n not in text:
            errors.append(f'{path}: missing {n!r}')
    return text

scope=must(Path('widgets/RttyScopeWidget.cpp'), ['setReversePolarity(', 'MARK', 'SPACE'])
if 'setLiveText(' in scope or 'm_liveText' in scope or 'Live decoder tape' in scope:
    errors.append('widgets/RttyScopeWidget.cpp: decoded text still covers the tuning scope')
scope_header=(root/'widgets/RttyScopeWidget.h').read_text(encoding='utf-8', errors='replace')
for obsolete in ['polarityLine', 'm_polaritySource', 'm_catMode', ' · CAT ']:
    if obsolete in scope or obsolete in scope_header:
        errors.append(f'widgets/RttyScopeWidget: obsolete in-scope status text remains: {obsolete}')
dec=must(Path('modems/rtty/RttyDecoder.cpp'), ['setCatModeHint(', 'advancePolarityProbe(m_normalProbe', 'advancePolarityProbe(m_reverseProbe'])
must(Path('rig/HamlibController.cpp'), ['rig_get_mode(', 'rig_strrmode(', 'emit modeChanged(modeName)'])
main=must(Path('mainwindow.cpp'), ['m_tabRttyContest', 'insertTab(insertIndex, m_tabRttyContest', 'updateRttyWaterfallOverlays()', 'live.verticalTrail = true', 'live.streamId = QStringLiteral("rtty-live")', 'markHz + (shiftHz * 0.5)', 'setReversePolarity(reverse)', 'm_chkRttyWaterfallTextOverlay', 'rttyWaterfallTextOverlayEnabled &&', 'clearTextOverlayStream(QStringLiteral("rtty-live"))', 'addQsoToLogFromForm(contestQsoForm())'])
settings=must(Path('settings/AppSettings.cpp'), ['RTTY/waterfallTextOverlayEnabled'])
settings_header=must(Path('settings/AppSettings.h'), ['rttyWaterfallTextOverlayEnabled = false'])
waterfall=must(Path('widgets/WaterfallWidget.cpp'), ['appendVerticalTextTrail(overlay)', 'drawVerticalTextTrails(painter)', 'centerOnFrequency'])
if 'discardedVerticalTrail' in waterfall:
    errors.append('widgets/WaterfallWidget.cpp: live vertical trails are still discarded')
cmake=must(Path('CMakeLists.txt'), ['install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/rtty_rules" DESTINATION "${CMAKE_INSTALL_BINDIR}")', 'install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cw_rules" DESTINATION "${CMAKE_INSTALL_BINDIR}")'])
must(Path('scripts/build_linux_github.sh'), ['$INSTALL_DIR/bin/rtty_rules', '$INSTALL_DIR/bin/cw_rules'])
must(Path('scripts/package_linux_github.sh'), ['$PACKAGE_DIR/bin/rtty_rules', '$PACKAGE_DIR/bin/cw_rules'])
must(Path('scripts/package_macos.sh'), ['Contents/MacOS/rtty_rules', 'Contents/MacOS/cw_rules'])
must(Path('scripts/package_windows_msys2.sh'), ['rtty_rules', 'cw_rules'])

# Keep contest rules data-driven and structurally valid.
try:
    rules=json.loads((root/'rtty_rules').read_text(encoding='utf-8'))
    profiles=rules.get('profiles', [])
    if not profiles:
        errors.append('rtty_rules: no profiles')
    if max((len(p.get('macros',[])) for p in profiles), default=0) > 6:
        errors.append('rtty_rules: profile requires more than six contest macro buttons')
except Exception as e:
    errors.append(f'rtty_rules parse failed: {e}')


try:
    cw_rules=json.loads((root/'cw_rules').read_text(encoding='utf-8'))
    cw_profiles=cw_rules.get('profiles', [])
    if not cw_profiles:
        errors.append('cw_rules: no profiles')
    if max((len(p.get('macros',[])) for p in cw_profiles), default=0) > 6:
        errors.append('cw_rules: profile requires more than six contest macro buttons')
    required={'ari_40_80_cw','cq_ww_cw','cq_wpx_cw','arrl_dx_cw','iaru_hf_cw','wae_dx_cw','marconi_144_cw'}
    missing=required-{p.get('id') for p in cw_profiles}
    if missing:
        errors.append('cw_rules: missing core profiles: '+', '.join(sorted(missing)))
except Exception as e:
    errors.append(f'cw_rules parse failed: {e}')

# All UI dictionaries must contain the new contest tab fields.
keys=['rtty_contest_qso','rtty_contest_macros','qso_callsign','qso_mode','qso_rst_sent','qso_rst_received','qso_grid','qso_utc','qso_add_to_log','rtty_waterfall_text_overlay']
for lang in ['en','it','fr','de','no','cs']:
    text=(root/f'translations/ui_{lang}.ini').read_text(encoding='utf-8', errors='replace')
    for key in keys:
        if f'{key}=' not in text:
            errors.append(f'ui_{lang}.ini missing {key}')

# Old competing RTTY auto-polarity state should not survive the new resolver.
for old in ['m_autoInvert', 'm_markRunSamples', 'm_spaceRunSamples', 'm_framingFailureStreak']:
    if old in dec:
        errors.append(f'RttyDecoder still contains obsolete polarity state {old}')

# Contest turnaround and worked-call semantics are shared by RTTY/CW.  They
# remain part of this consolidated UI/runtime audit rather than creating more
# one-bug CTest entries.
rtty_h=must(Path('modems/rtty/RttyDecoder.h'), ['resumeAfterLocalTransmit()'])
rtty_multi=must(Path('modems/rtty/RttyMultiDecoder.cpp'), ['resumeAfterLocalTransmit()', 'track.decoder->resumeAfterLocalTransmit()', 'm_scanBuffer.clear()'])
cw_h=must(Path('modems/cw/CwDecoder.h'), ['resumeAfterLocalTransmit()'])
cw_tracker=must(Path('modems/cw/skimmer/SelectedToneCwTracker.cpp'),
                ['resumeAfterLocalTransmit()', 'timingTask.reset(true)', 'discriminator.resumeAfterGap()'])
# Use direct checks for multiline constructs whose whitespace is intentionally
# not an API contract.
if 'm_fastResumeCwRttyRxPending' not in main or \
   'm_rttyDecoder->resumeAfterLocalTransmit()' not in main or \
   'm_rttyMultiDecoder->resumeAfterLocalTransmit()' not in main or \
   'm_cwDecoder->resumeAfterLocalTransmit()' not in main:
    errors.append('mainwindow.cpp: CW/RTTY fast TX-to-RX resume path is incomplete')
if 'const int rxRestartDelayMs = (ftLowLatencyReturn || fastResumeCwRtty) ? 0 : 250;' not in main:
    errors.append('mainwindow.cpp: CW/RTTY fast resume is not immediate after PTT release')
if 'contestTerminal' not in main or 'isCurrentContestDupe(call)' not in main:
    errors.append('mainwindow.cpp: CW/RTTY worked highlighting is not scoped to the active contest')
if '#if defined(Q_OS_LINUX)' not in main or 'Qt::WindowStaysOnTopHint' not in main:
    errors.append('mainwindow.cpp: fullscreen popup workaround must remain Linux-only')

if errors:
    print('RTTY live/contest/runtime audit FAILED')
    for e in errors: print(' -',e)
    sys.exit(1)
print(f'RTTY live/contest/runtime audit OK ({len(profiles)} rule profiles)')
