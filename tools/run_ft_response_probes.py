#!/usr/bin/env python3
"""Optional Linux GCC/Clang FT probes linked to a configured application's objects."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--baseline', action='store_true')
    parser.add_argument('--repeat', type=int, default=1)
    args = parser.parse_args()
    build = args.build.resolve()
    root = Path(__file__).resolve().parents[1]
    commands = json.loads((build / 'compile_commands.json').read_text())
    entry = next(x for x in commands if x['file'].endswith('/main.cpp'))
    link = shlex.split((build / 'CMakeFiles/MadModem.dir/link.txt').read_text())
    output = build / 'ft-response-probes'
    output.mkdir(exist_ok=True)
    probes = [('FtRuntimeLatencyProbe', ['cancel', 'async', ''])]
    if args.baseline:
        probes = [('FtRuntimeLatencyProbe', ['cancel', ''])]
    else:
        probes.append(('FtResponseUiProbe', ['']))
    for name, runs in probes:
        command = shlex.split(entry['command'])
        if Path(command[0]).name in ('cl', 'cl.exe'):
            parser.error('These optional private-state probes require a GCC/Clang Linux build.')
        command.insert(1, '-fno-access-control')
        if args.baseline:
            command.insert(1, '-DBASELINE')
        obj = output / (name + '.o')
        exe = output / name
        command[command.index('-o') + 1] = str(obj)
        command[command.index('-c') + 1] = str(root / 'tests' / (name + '.cpp'))
        subprocess.run(command, cwd=build, check=True)
        command = [str(obj) if x == 'CMakeFiles/MadModem.dir/main.cpp.o' else x for x in link]
        command[command.index('-o') + 1] = str(exe)
        subprocess.run(command, cwd=build, check=True)
        exe.chmod(0o755)
        for run in runs:
            for _ in range(max(1, args.repeat) if name == 'FtRuntimeLatencyProbe' and run == '' else 1):
                subprocess.run([str(exe)] + ([run] if run else []), cwd=output, check=True)


if __name__ == '__main__':
    main()
