#!/usr/bin/env python3
"""Run inside a clean matching distribution container WITHOUT Qt or Hamlib."""
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
from bundle_linux_runtime import HOST, dependencies, elf

cache = subprocess.check_output(['ldconfig', '-p'], text=True)
assert 'libQt5' not in cache and 'libhamlib' not in cache, 'Smoke-test host already has Qt/Hamlib'
with tempfile.TemporaryDirectory(prefix='mm portable ') as temp:
    work = Path(temp)
    with tarfile.open(sys.argv[1]) as archive:
        archive.extractall(work, filter='data')
    root, = work.iterdir()
    assert not (root / 'bin/managed-install.flag').exists()
    env = dict(os.environ, LD_LIBRARY_PATH=str(root / 'lib'), QT_QPA_PLATFORM='offscreen',
               QT_QUICK_BACKEND='software', HOME=str(work / 'home'))
    Path(env['HOME']).mkdir()
    runtime = work / 'xdg-runtime'
    runtime.mkdir(mode=0o700)
    env['XDG_RUNTIME_DIR'] = str(runtime)
    assert (root / 'etc/fonts/fonts.conf').is_file()
    # A launcher must override stale font configuration from the calling shell.
    env['FONTCONFIG_PATH'] = str(work / 'missing-fontconfig')
    env['FONTCONFIG_FILE'] = str(work / 'missing-fontconfig/fonts.conf')
    for key in ('QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QML_IMPORT_PATH', 'QML2_IMPORT_PATH'):
        env.pop(key, None)
    for file in root.rglob('*'):
        if file.is_file() and elf(file):
            for name, resolved in dependencies(file, env):
                assert HOST.fullmatch(name) or resolved.resolve().is_relative_to(root), (file, name, resolved)
    # Invoke through both public entry points, outside the package working directory.
    for executable, option in [('AppRun', '--version'), ('bin/MadModem', '--portable-runtime-regression'),
                               ('AppRun', '--window-rotator-regression')]:
        result = subprocess.run([str(root / executable), option], cwd=work, env=env,
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=90)
        print(result.stdout, end='')
        print(result.stderr, end='', file=sys.stderr)
        result.check_returncode()
        assert 'Fontconfig error:' not in result.stderr, 'Broken portable font configuration'
    print('PASS: relocated portable runtime in clean host without Qt or Hamlib')
