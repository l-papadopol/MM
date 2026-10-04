#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
main = (root / 'mainwindow.cpp').read_text(encoding='utf-8')
hdr = (root / 'mainwindow.h').read_text(encoding='utf-8')
idx_h = (root / 'runtime/LogbookIndexWorker.h').read_text(encoding='utf-8')
idx_cpp = (root / 'runtime/LogbookIndexWorker.cpp').read_text(encoding='utf-8')
txt_h = (root / 'runtime/TextAssistWorker.h').read_text(encoding='utf-8')
txt_cpp = (root / 'runtime/TextAssistWorker.cpp').read_text(encoding='utf-8')
cmake = (root / 'CMakeLists.txt').read_text(encoding='utf-8')
errors = []

def require(cond, message):
    if not cond:
        errors.append(message)

require('m_logbookIndexWorker->moveToThread(m_logbookIndexThread)' in main,
        'LogbookIndexWorker is not owned by a dedicated QThread')
require('m_textAssistWorker->moveToThread(m_textAssistThread)' in main,
        'TextAssistWorker is not owned by a dedicated QThread')
require('{m_textAssistThread, QStringLiteral("Text assistance")}' in main and
        '{m_logbookIndexThread, QStringLiteral("Logbook index")}' in main,
        'assist worker threads are missing from cooperative shutdown')
require('QHash<QString, int> m_byCall' in idx_h and 'QHash<QString, int> m_contestDupes' in idx_h,
        'logbook worker does not own O(1) global/contest hash indexes')
require('m_byCall.contains(call)' in idx_cpp and
        'm_contestDupes.contains(contestKey(call, band, periodId))' in idx_cpp,
        'logbook lookup is not hash-based')
require('worker->addEntry(entry)' in main,
        'normal QSO logging does not incrementally update the background index')
require('rebuildFromFile' in idx_h + idx_cpp and 'setContestConfigFromFile' in idx_h + idx_cpp,
        'exceptional index rebuilds must parse ADIF inside LogbookIndexWorker')
rebuild_start = main.find('void MainWindow::queueLogbookIndexRebuild()')
rebuild_end = main.find('void MainWindow::queueLogbookIndexAdd', rebuild_start)
rebuild_body = main[rebuild_start:rebuild_end] if rebuild_start >= 0 and rebuild_end > rebuild_start else ''
require('const auto records = m_logbook.records()' in rebuild_body and 'rebuildFromRecords(records, config)' in rebuild_body,
        'GUI must not copy the whole logbook when rebuilding indexes')
require('worker->analyze(' in main and 'analysisReady' in txt_h and 'lookupReady' in idx_h,
        'text parse -> logbook lookup pipeline is incomplete')
require('QRegularExpression' in txt_cpp and 'AdifLogbook::normalizeCallsign' in txt_cpp,
        'TextAssistWorker is not the owner of callsign parsing/normalization')
require('QPlainTextEdit' not in txt_h + txt_cpp and 'QTextDocument' not in txt_h + txt_cpp,
        'TextAssistWorker must not depend on GUI text widgets/documents')
require('processRttyContestRxLine' not in main and 'scanTextForHeardStations' not in main,
        'legacy GUI-thread textual parsers are still compiled')
start = main.find('void MainWindow::highlightCallsignsInTerminal')
end = main.find('void MainWindow::scheduleTerminalHighlight', start)
body = main[start:end] if start >= 0 and end > start else ''
require('m_logbook.records()' not in body and 'm_logbook.containsCallsign' not in body,
        'live terminal highlighting still accesses the logbook')
require('kLiveHighlightTailCharacters = 4096' in body,
        'live terminal snapshot lost its bounded tail')
require('runtime/LogbookIndexWorker.cpp' in cmake and 'runtime/TextAssistWorker.cpp' in cmake,
        'new workers are not part of the MadModem build')
require('madmodem_text_assist_workers_regression' in cmake and
        'tests/TextAssistWorkersRegression.cpp' in cmake,
        'text-assist worker C++ regression is not registered in CTest')

# FT4/FT8 live GUI must use the same background index rather than scanning ADIF.
require('void lookupFt(' in idx_h and 'void LogbookIndexWorker::lookupFt' in idx_cpp and
        'ftLookupReady' in idx_h,
        'LogbookIndexWorker is missing the asynchronous FT lookup path')
require('m_byDxcc' in idx_h and 'm_byGrid4' in idx_h and
        'm_latestByCall' in idx_h and 'm_latestByCallBandMode' in idx_h,
        'FT worked/new-country/grid/recent metadata is not indexed in the worker')
for begin, end_marker, label in [
    ('void MainWindow::refreshFt8DecodeWorkedHighlights()', 'void MainWindow::refreshLogbookHighlights', 'FT table highlight'),
    ('Ft8FullAutoCqCandidate MainWindow::buildFt8FullAutoCqCandidate', 'bool MainWindow::queueFt8FullAutoCqCandidate', 'FT AutoQSO priority'),
    ('void MainWindow::autoLogFt8Qso', 'void MainWindow::handleFt8QsoHistoryDoubleClicked', 'FT auto-log'),
    ('void MainWindow::handleFt8DecodeReady', 'void MainWindow::addFt8WaterfallOverlayForDecode', 'FT live decode UI'),
]:
    a = main.find(begin)
    b = main.find(end_marker, a)
    part = main[a:b] if a >= 0 and b > a else ''
    require(bool(part), f'{label}: function body not found')
    require('m_logbook.records()' not in part and 'm_logbook.containsCallsign' not in part,
            f'{label}: synchronous GUI-thread logbook scan/lookup returned')
require('worker->lookupFt(' in main and 'cachedFtLogbookStatus' in main,
        'FT UI is not wired to the background logbook index/cache')

for decoder in ['modems/rtty/RttyDecoder.cpp', 'modems/bpsk31/Bpsk31Decoder.cpp',
                'modems/mfsk/MfskDecoder.cpp', 'modems/cw/CwDecoder.cpp']:
    text = (root / decoder).read_text(encoding='utf-8')
    require('AdifLogbook' not in text and 'LogbookIndexWorker' not in text and 'TextAssistWorker' not in text,
            f'{decoder}: modem decoder acquired logbook/text-assist responsibility')

if errors:
    print('Text assistance worker architecture FAILED')
    for e in errors:
        print(' -', e)
    sys.exit(1)
print('Text assistance worker architecture passed.')
