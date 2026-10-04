from pathlib import Path
import sys
source=(Path(sys.argv[1])/'mainwindow.cpp').read_text()
a=source[source.index('struct AfcTonePeak'):source.index('int previousPowerOfTwo')]
b=source[source.index('AfcTonePeak estimateAfcTonePeak'):source.index('/**\n * @brief Minimal parsed WAV stream metadata.')]
Path(__file__).with_name('afc_extracted.h').write_text('// Exact AFC helper extraction from the specified MainWindow source.\n#pragma once\n#include <QtMath>\n#include <algorithm>\n'+a+b)
