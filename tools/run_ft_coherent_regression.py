#!/usr/bin/env python3
"""Compile a real-app native FT regression against a Unix Makefiles build."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', required=True, type=Path)
p.add_argument('--probe', choices=['FtCoherentRecoveryRegression.cpp', 'FtNativeFrontendRegression.cpp'],
               default='FtCoherentRecoveryRegression.cpp')
p.add_argument('--sanitize', action='store_true')
p.add_argument('--leaks', action='store_true')
a = p.parse_args()
build = a.build.resolve()
source = Path(__file__).resolve().parents[1]
flags = {}
for line in (build / 'CMakeFiles/MadModem.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
name = 'ft-native-frontend-regression' if a.probe == 'FtNativeFrontendRegression.cpp' else 'ft-coherent-regression'
if a.sanitize:
    name += '-sanitized'
output = build / name
obj = output.with_suffix('.o')
link = shlex.split((build / 'CMakeFiles/MadModem.dir/link.txt').read_text())
extra = ['-O1']
if a.sanitize:
    extra += ['-g', '-fno-omit-frame-pointer', '-fsanitize=address,undefined']
def compile_file(path, target):
    subprocess.run([link[0], *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'],
                    *flags['CXX_FLAGS'], '-fno-access-control', *extra,
                    '-c', str(path), '-o', str(target)], cwd=build, check=True)
compile_file(source / 'tests' / a.probe, obj)
link[link.index('-o') + 1] = str(output)
link[link.index('CMakeFiles/MadModem.dir/main.cpp.o')] = str(obj)
if a.sanitize:
    receiver = build / (name + '-receiver.o')
    compile_file(source / 'modems/ft8/Ft8RxDecoder.cpp', receiver)
    link[link.index('CMakeFiles/MadModem.dir/modems/ft8/Ft8RxDecoder.cpp.o')] = str(receiver)
    link += ['-fsanitize=address,undefined']
subprocess.run(link, cwd=build, check=True)
output.chmod(0o755)
env = dict(os.environ)
if a.sanitize:
    env['ASAN_OPTIONS'] = 'detect_leaks=' + ('1' if a.leaks else '0') + ':halt_on_error=1'
    env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
subprocess.run([str(output)], env=env, check=True)
