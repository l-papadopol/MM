#!/usr/bin/env python3
"""Optional Linux DSP probe. Needs a Release app build, NumPy and SciPy.

Independent scipy.signal.hilbert generates linear chirps from the real MSHV TX
waveform; these are synthetic fixtures and do not establish QO-100 RF sensitivity.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import numpy as np
from scipy.signal import hilbert


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('build', type=Path)
    args = p.parse_args()
    build = args.build.resolve()
    source = Path(__file__).resolve().parents[1]
    output = build / 'ft-drift-probes'
    output.mkdir(exist_ok=True)
    commands = json.loads((build / 'compile_commands.json').read_text())
    entry = next(x for x in commands if x['file'].endswith('/main.cpp'))
    command = shlex.split(entry['command'])
    command.insert(1, '-fno-access-control')
    # Distinct executables prevent concurrent invocations from relinking a
    # probe that is already running. Keep result/fixture paths stable.
    obj = output / f'probe-{os.getpid()}.o'
    exe = output / f'probe-{os.getpid()}'
    command[command.index('-o') + 1] = str(obj)
    command[command.index('-c') + 1] = str(source / 'tests/FtDriftRxProbe.cpp')
    subprocess.run(command, cwd=build, check=True)
    link = shlex.split((build / 'CMakeFiles/MadModem.dir/link.txt').read_text())
    link = [str(obj) if x == 'CMakeFiles/MadModem.dir/main.cpp.o' else x for x in link]
    link[link.index('-o') + 1] = str(exe)
    subprocess.run(link, cwd=build, check=True)
    exe.chmod(0o755)
    results = []
    for mode in ('FT8', 'FT4'):
        clean = output / (mode + '-clean.f64')
        subprocess.run([str(exe), 'generate', mode, str(clean)], check=True)
        wave = np.fromfile(clean, dtype='<f8')
        analytic = hilbert(wave)
        duration = 12.640 if mode == 'FT8' else 4.944
        start = .5 if mode == 'FT8' else .348
        t = np.arange(len(wave)) / 12000 - start - duration / 2
        # Positive/negative LO drifts, two noise levels, zero drift, no signal.
        for drift in (-20., -10., -2., 0., 2., 5., 10., 20.):
            for noise in (.01, .04):
                rng = np.random.default_rng(2931)
                fixture = .15 * (analytic * np.exp(1j * np.pi * drift * t * t)).real
                fixture += rng.normal(0, noise, len(wave))
                file = output / f'{mode}-{drift:+.1f}-{noise}.f64'
                fixture.astype('<f8').tofile(file)
                action = 'pipeline'
                run = subprocess.run([str(exe), action, mode, str(file), str(drift), 'yes', '20'], text=True, capture_output=True)
                print(run.stdout, end='', flush=True)
                results.append(dict(mode=mode, drift=drift, noise=noise, action=action,
                                    passed=run.returncode == 0, output=run.stdout, stderr=run.stderr))
        for rf in (0, 14074000, 50313000, 143999999):
            file = output / f'{mode}-{20.:+.1f}-0.01.f64'
            run = subprocess.run([str(exe), 'rfblock', mode, str(file), '20', 'no', '20', str(rf)], text=True, capture_output=True)
            print(run.stdout, end='', flush=True)
            results.append(dict(mode=mode, action='rfblock', rf_hz=rf, passed=run.returncode == 0, output=run.stdout, stderr=run.stderr))
        drift = 2. if mode == 'FT8' else 5.
        first = output / f'{mode}-{drift:+.1f}-0.01.f64'
        shift = drift * (30 if mode == 'FT8' else 15)
        second = output / (mode + '-next-slot.f64')
        shifted = .15 * (analytic * np.exp(1j * np.pi * drift * t * t + 2j * np.pi * shift * t)).real
        shifted += np.random.default_rng(623).normal(0, .01, len(wave))
        shifted.astype('<f8').tofile(second)
        run = subprocess.run([str(exe), 'track', mode, str(first), str(drift), 'yes', '5', str(second)], text=True, capture_output=True)
        print(run.stdout, end='', flush=True)
        results.append(dict(mode=mode, action='track', passed=run.returncode == 0,
                            output=run.stdout, stderr=run.stderr))
        noise_file = output / (mode + '-noise.f64')
        np.random.default_rng(893).normal(0, .04, len(wave)).astype('<f8').tofile(noise_file)
        for action in ('off', 'recover'):
            run = subprocess.run([str(exe), action, mode, str(noise_file), '0', 'no'], text=True, capture_output=True)
            print(run.stdout, end='', flush=True)
            results.append(dict(mode=mode, action=action, noise_only=True,
                                passed=run.returncode == 0, output=run.stdout, stderr=run.stderr))
    (output / 'results.json').write_text(json.dumps(results, indent=2))
    if not all(x['passed'] for x in results):
        raise SystemExit('Drift probe failed: inspect ft-drift-probes/results.json')
    print('FT8/FT4 optional drift probes PASS')


if __name__ == '__main__':
    main()
