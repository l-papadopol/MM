#!/usr/bin/env python3
"""Build the native FT8/FT4 recovery regression against a Unix Makefiles app build.

--sanitize instruments all new native recovery translation units with ASan/UBSan.
Optional .f64 arguments replay mono 12 kHz doubles through the native path.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--sanitize', action='store_true')
p.add_argument('--leaks', action='store_true', help='Enable LeakSanitizer where /proc thread inspection is available')
p.add_argument('fixtures', nargs='*', type=Path)
a = p.parse_args()
b = a.build.resolve()
s = Path(__file__).resolve().parents[1]
flags = {}
for line in (b / 'CMakeFiles/MadModem.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
link = shlex.split((b / 'CMakeFiles/MadModem.dir/link.txt').read_text())
out = b / ('ft-reference-sanitized' if a.sanitize else 'ft-reference-regression')
out.mkdir(exist_ok=True)
extra = ['-O1', '-g', '-ftrivial-auto-var-init=zero', '-fno-omit-frame-pointer', '-fsanitize=address,undefined'] if a.sanitize else ['-O2']
def compile_file(source, target):
    if a.sanitize and target.exists() and target.stat().st_size and target.stat().st_mtime >= source.stat().st_mtime:
        return
    subprocess.run([link[0], *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'],
                    *flags['CXX_FLAGS'], *extra, '-c', str(source), '-o', str(target)], cwd=b, check=True)
main = out / 'main.o'
compile_file(s / 'tests/FtReferenceRecoveryRegression.cpp', main)
link[link.index('CMakeFiles/MadModem.dir/main.cpp.o')] = str(main)
if a.sanitize:
    objects = []
    for name in ['modems/ft8/FtReferenceRecovery.cpp', *['third_party/mshv_gpl/ft_reference/' + n for n in
                  ['decoderft8.cpp', 'decoderft8var.cpp', 'decoderft4.cpp', 'decoderpom.cpp', 'fft.cpp']]]:
        obj = out / (Path(name).stem + '.o')
        compile_file(s / name, obj)
        objects.append(str(obj))
    i = next(i for i, v in enumerate(link) if v.endswith('libmadmodem_ft_reference.a'))
    link[i:i+1] = objects
    link += ['-fsanitize=address,undefined']
exe = out / 'probe'
link[link.index('-o') + 1] = str(exe)
subprocess.run(link, cwd=b, check=True)
env = dict(os.environ)
if a.sanitize:
    env['ASAN_OPTIONS'] = 'detect_leaks=' + ('1' if a.leaks else '0') + ':halt_on_error=1'
    env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
subprocess.run([str(exe), *[str(f.resolve()) for f in a.fixtures]], env=env, check=True)
