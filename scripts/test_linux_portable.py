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
    for key in ('QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QML_IMPORT_PATH', 'QML2_IMPORT_PATH'):
        env.pop(key, None)
    for file in root.rglob('*'):
        if file.is_file() and elf(file):
            for name, resolved in dependencies(file, env):
                assert HOST.fullmatch(name) or resolved.resolve().is_relative_to(root), (file, name, resolved)
    # Invoke through both public entry points, outside the package working directory.
    for executable, option in [('AppRun', '--version'), ('bin/MadModem', '--portable-runtime-regression'),
                               ('AppRun', '--window-rotator-regression')]:
        subprocess.run([str(root / executable), option], cwd=work, env=env, check=True, timeout=90)
    print('PASS: relocated portable runtime in clean host without Qt or Hamlib')
