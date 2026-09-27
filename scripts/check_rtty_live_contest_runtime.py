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
dec=must(Path('modems/rtty/RttyDecoder.cpp'), ['setReverse(bool reverse)', 'setNarrowFilterEnabled(bool enabled)', 'LowPassBiquad::setLowPass', 'kAtcBias', 'rawBitIsMark', 'if (m_reverse)'])
for obsolete in ['setCatModeHint(', 'advancePolarityProbe(', 'evaluateAutomaticPolarity(']:
    if obsolete in dec: errors.append(f'modems/rtty/RttyDecoder.cpp: obsolete automatic polarity code remains: {obsolete}')
must(Path('rig/HamlibController.cpp'), ['rig_get_mode(', 'rig_strrmode(', 'emit modeChanged(modeName)'])
main=must(Path('mainwindow.cpp'), ['m_tabRttyContest', 'insertTab(insertIndex, m_tabRttyContest', 'updateRttyWaterfallOverlays()', 'live.verticalTrail = true', 'live.streamId = QStringLiteral("rtty-live")', 'markHz + (shiftHz * 0.5)', 'setReversePolarity(reverse)', 'm_chkRttyWaterfallTextOverlay', 'rttyWaterfallTextOverlayEnabled &&', 'clearTextOverlayStream(QStringLiteral("rtty-live"))', 'addQsoToLogFromForm(contestQsoForm())'])
settings=must(Path('settings/AppSettings.cpp'), ['RTTY/waterfallTextOverlayEnabled'])
settings_header=must(Path('settings/AppSettings.h'), ['rttyWaterfallTextOverlayEnabled = false'])
waterfall=must(Path('widgets/WaterfallWidget.cpp'), ['appendVerticalTextTrail(overlay)', 'drawVerticalTextTrails(painter)', 'centerOnFrequency'])
if 'discardedVerticalTrail' in waterfall:
    errors.append('widgets/WaterfallWidget.cpp: live vertical trails are still discarded')
cmake=must(Path('CMakeLists.txt'), ['install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/rtty_rules" DESTINATION "${CMAKE_INSTALL_BINDIR}")', 'install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cw_rules" DESTINATION "${CMAKE_INSTALL_BINDIR}")'])
conditioner=must(Path('dsp/common/DspConditioner.cpp'), ['case Profile::Rtty:'])
conditioner_h=must(Path('dsp/common/DspConditioner.h'), ['Rtty'])
for obsolete in ['rttyMatchedFilterEnabled', 'rttyMarkSpaceEnhancerEnabled', 'm_rttyMarkBp', 'm_rttySpaceBp']:
    if obsolete in conditioner or obsolete in conditioner_h:
        errors.append(f'DspConditioner still owns obsolete parallel RTTY filter state: {obsolete}')
must(Path('tests/RttySyntheticBench.cpp'), ['mark_fade_-18_db', 'cw_plus20_db_offset300', 'noise_only'])
if 'madmodem_rtty_synthetic_bench' not in cmake:
    errors.append('CMakeLists.txt: RTTY synthetic bench is not registered with CTest')
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
keys=['rtty_contest_qso','qso_callsign','qso_mode','qso_rst_sent','qso_rst_received','qso_grid','qso_utc','qso_add_to_log','rtty_waterfall_text_overlay']
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
rtty_h=must(Path('modems/rtty/RttyDecoder.h'), ['resumeAfterLocalTransmit()', 'setVisualizationEnabled(bool enabled)'])
rtty_multi=must(Path('modems/rtty/RttyMultiDecoder.cpp'), ['resumeAfterLocalTransmit()', 'track.decoder->resumeAfterLocalTransmit()', 'm_scanBuffer.clear()', 'track.decoder->setVisualizationEnabled(false)'])
cw_h=must(Path('modems/cw/CwDecoder.h'), ['resumeAfterLocalTransmit()'])
cw_tracker=must(Path('modems/cw/skimmer/SelectedToneCwTracker.cpp'),
                ['resumeAfterLocalTransmit()', 'timingTask.reset(true)', 'discriminator.resumeAfterGap()'])
# Use direct checks for multiline constructs whose whitespace is intentionally
# not an API contract.
if 'm_fastResumeCwRttyRxPending' not in main or \
   'invokeRxDecoder(m_rttyDecoder, &RttyDecoder::resumeAfterLocalTransmit)' not in main or \
   'invokeRxDecoder(m_rttyMultiDecoder, &RttyMultiDecoder::resumeAfterLocalTransmit)' not in main or \
   'invokeRxDecoder(m_cwDecoder, &CwDecoder::resumeAfterLocalTransmit)' not in main:
    errors.append('mainwindow.cpp: CW/RTTY fast TX-to-RX resume path is incomplete')
if 'const int rxRestartDelayMs = (ftLowLatencyReturn || fastResumeCwRtty) ? 0 : 250;' not in main:
    errors.append('mainwindow.cpp: CW/RTTY fast resume is not immediate after PTT release')
index_worker = (root/'runtime/LogbookIndexWorker.cpp').read_text(encoding='utf-8')
text_worker = (root/'runtime/TextAssistWorker.cpp').read_text(encoding='utf-8')
if 'textAssistContestEnabled' not in main or 'm_contestDupes.contains(contestKey(call, band, periodId))' not in index_worker:
    errors.append('text assist: CW/RTTY worked highlighting is not scoped through the contest dupe index')

# Long RTTY/CW sessions must not make GUI work grow with the complete terminal
# history or logbook. Live highlighting is tail-bounded; parsing and dupe
# membership run in dedicated workers and the GUI only paints returned ranges.
if 'highlightCallsignsInTerminal(QPlainTextEdit *terminal, bool recentOnly)' not in main or \
   'kLiveHighlightTailCharacters = 4096' not in main or \
   'highlightCallsignsInTerminal(terminal, true)' not in main:
    errors.append('mainwindow.cpp: live terminal highlighting is not tail-bounded')
_highlight_body = main.split('void MainWindow::highlightCallsignsInTerminal', 1)[1].split('void MainWindow::scheduleTerminalHighlight', 1)[0]
if 'm_logbook.records()' in _highlight_body or 'm_logbook.containsCallsign' in _highlight_body:
    errors.append('mainwindow.cpp: terminal highlighter still accesses the logbook on the GUI thread')
if 'QMetaObject::invokeMethod(worker' not in _highlight_body or 'worker->analyze(' not in _highlight_body:
    errors.append('mainwindow.cpp: live text parsing is not delegated to TextAssistWorker')
if 'm_byCall.contains(call)' not in index_worker or 'QHash<QString, int> m_byCall' not in (root/'runtime/LogbookIndexWorker.h').read_text(encoding='utf-8'):
    errors.append('LogbookIndexWorker: worked-before lookup is not hash-indexed')
if 'processRttyContestRxLine' in main or 'scanTextForHeardStations' in main:
    errors.append('mainwindow.cpp: legacy GUI-thread text/contest parser is still present')
if 'setExtraSelections(selections)' not in main or 'QTextEdit::ExtraSelection sentSelection' not in main:
    errors.append('mainwindow.cpp: live TX progress no longer uses lightweight green ExtraSelection highlighting')
if 'm_runtimeLogBuffer = m_runtimeLogBuffer.mid(' not in main:
    errors.append('mainwindow.cpp: runtime log still prunes one line at a time after reaching its cap')
if '#if defined(Q_OS_LINUX)' not in main or 'Qt::WindowStaysOnTopHint' not in main:
    errors.append('mainwindow.cpp: fullscreen popup workaround must remain Linux-only')

if errors:
    print('RTTY live/contest/runtime audit FAILED')
    for e in errors: print(' -',e)
    sys.exit(1)
print(f'RTTY live/contest/runtime audit OK ({len(profiles)} rule profiles)')
