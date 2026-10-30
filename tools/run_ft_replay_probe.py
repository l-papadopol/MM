#!/usr/bin/env python3
"""Replay WAV/12 kHz float64 inputs through the real FT receiver.

Use --native-only to disable the supplementary R30 recovery. --baseline is
retained as a compatibility alias for this same native-core comparison.
A comparison with an earlier revision needs its frozen source/binary. PROBE_OFFLINE=1 removes live UTC deadlines; PROBE_GATE=1
uses the early gate. PROBE_WORKERS chooses a fixed topology for paired runs.
PROBE_OFFSET_SEC selects the start of a WAV excerpt. JSONL contains complete
messages and timing, so comparisons can report lost rows as well as additions.
"""
import argparse
from pathlib import Path
import shlex
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--baseline', action='store_true')
p.add_argument('--native-only', action='store_true', help='Disable the existing R30 supplementary recovery for native-core comparisons')
p.add_argument('--compile-only', action='store_true')
p.add_argument('fixtures', nargs='*', type=Path)
a = p.parse_args()
b = a.build.resolve()
s = Path(__file__).resolve().parents[1]
flags = {}
for line in (b / 'CMakeFiles/MadModem.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        k, v = line.split(' = ', 1)
        flags[k] = shlex.split(v)
link = shlex.split((b / 'CMakeFiles/MadModem.dir/link.txt').read_text())
out = b / ('ft-replay-native' if a.native_only else ('ft-replay-baseline' if a.baseline else 'ft-replay-r30'))
out.mkdir(exist_ok=True)
def compile_file(source, target):
    subprocess.run([link[0], *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'],
                    *flags['CXX_FLAGS'], '-fno-access-control', '-I' + str(s / 'modems/ft8'),
                    '-c', str(source), '-o', str(target)], cwd=b, check=True)
obj = out / 'main.o'
compile_file(s / 'tests/FtReplayProbe.cpp', obj)
link[link.index('CMakeFiles/MadModem.dir/main.cpp.o')] = str(obj)
if a.baseline or a.native_only:
    code = (s / 'modems/ft8/Ft8RxDecoder.cpp').read_text()
    hook = '    appendReferenceRecovery(samples, slotStartUtc, out, stats, publish);'
    if code.count(hook) != 2:
        raise SystemExit('Expected exactly the FT8 and FT4 additive hooks')
    source = out / 'baseline.cpp'
    source.write_text(code.replace(hook, '    // R30 reference hook disabled for paired baseline.'))
    decoder = out / 'decoder.o'
    compile_file(source, decoder)
    link[link.index('CMakeFiles/MadModem.dir/modems/ft8/Ft8RxDecoder.cpp.o')] = str(decoder)
exe = out / 'probe'
link[link.index('-o') + 1] = str(exe)
subprocess.run(link, cwd=b, check=True)
if not a.compile_only:
    subprocess.run([str(exe), *[str(f.resolve()) for f in a.fixtures]], check=True)
