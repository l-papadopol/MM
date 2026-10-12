#!/usr/bin/env python3
"""Deploy Qt5, Hamlib and their ELF dependency closure into a portable directory."""
import argparse
from collections import deque
import ctypes.util
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

# These belong to the host ABI or graphics driver stack, never to the app.
HOST = re.compile(r'^(?:ld-linux[^/]*|lib(?:c|m|mvec|pthread|dl|rt|resolv|util|anl|BrokenLocale|nss_[^/]+)\.so(?:\..*)?|lib(?:GL|EGL|GLX|GLdispatch|OpenGL|GLESv[12]|drm(?:_[^/]+)?|gbm)\.so(?:\..*)?)$')
PLUGIN_GROUPS = ('platforms', 'xcbglintegrations', 'imageformats', 'iconengines',
                 'audio', 'mediaservice', 'bearer', 'geoservices', 'position',
                 'sqldrivers', 'printsupport', 'wayland-shell-integration',
                 'wayland-decoration-client', 'wayland-graphics-integration-client')
QML_MODULES = ('QtQuick.2', 'QtQuick/Window.2', 'QtLocation', 'QtPositioning')


def run(*args, env=None, check=True):
    command_env = dict(os.environ if env is None else env, LC_ALL='C')
    return subprocess.run([str(a) for a in args], env=command_env, check=check,
                          text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def elf(path):
    with path.open('rb') as f:
        return f.read(4) == b'\x7fELF'


def dependencies(path, env=None):
    result = run('ldd', path, env=env)
    found = []
    for line in result.stdout.splitlines():
        if '=> not found' in line:
            raise RuntimeError(f'Unresolved dependency in {path}: {line.strip()}')
        match = re.match(r'\s*(\S+) => (/.*?) \(0x[0-9a-f]+\)', line)
        if match:
            found.append((match[1], Path(match[2])))
    return found


def deploy_closure(seeds, libdir):
    queue = deque(seeds)
    visited = set()
    origins = {}
    while queue:
        file = queue.popleft().resolve()
        if file in visited:
            continue
        visited.add(file)
        for name, path in dependencies(file):
            if HOST.fullmatch(name):
                continue
            destination = libdir / name
            if destination.exists():
                if destination.read_bytes() != path.read_bytes():
                    raise RuntimeError(f'Conflicting runtime libraries: {name}: {path}')
            else:
                shutil.copy2(path, destination)
            origins[destination] = path
            queue.append(path)
    return origins


def copy_licenses(origins, destination):
    """Keep distribution copyright/license texts and exact package provenance."""
    destination.mkdir(parents=True, exist_ok=True)
    packages = {}
    records = []
    for deployed, source in sorted(origins.items()):
        record = {'file': str(deployed), 'source': str(source),
                  'sha256': hashlib.sha256(deployed.read_bytes()).hexdigest()}
        owner = ''
        if shutil.which('dpkg-query'):
            for candidate in (source, source.resolve(), Path(str(source).removeprefix('/usr'))):
                result = run('dpkg-query', '-S', candidate, check=False)
                if result.returncode == 0:
                    owner = result.stdout.split(': ', 1)[0].splitlines()[0]
                    break
            if owner and owner not in packages:
                metadata = run('dpkg-query', '-W', '-f=${Package}\t${Version}\t${source:Package}\t${source:Version}\n', owner).stdout.strip()
                packages[owner] = metadata
                doc = Path('/usr/share/doc') / owner.split(':')[0] / 'copyright'
                if doc.is_file():
                    shutil.copy2(doc, destination / (owner.replace(':', '_') + '.copyright'))
        elif shutil.which('rpm'):
            result = run('rpm', '-qf', '--qf', '%{NAME}', source, check=False)
            if result.returncode == 0:
                owner = result.stdout.strip()
                if owner not in packages:
                    packages[owner] = run('rpm', '-q', '--qf', '%{NAME}\t%{VERSION}-%{RELEASE}\t%{SOURCERPM}\n', owner).stdout.strip()
                    for name in run('rpm', '-q', '--qf', '[%{FILENAMES}\t%{FILEFLAGS:fflags}\n]', owner).stdout.splitlines():
                        path, _, flags = name.partition('\t')
                        if 'l' in flags and Path(path).is_file():
                            target = destination / owner / path.lstrip('/')
                            target.parent.mkdir(parents=True, exist_ok=True)
                            shutil.copy2(path, target)
        record['package'] = owner
        records.append(record)
    # Full standard license texts referenced by Debian copyright files.
    common = Path('/usr/share/common-licenses')
    if common.is_dir():
        shutil.copytree(common, destination / 'common-licenses', dirs_exist_ok=True)
    (destination / 'packages.tsv').write_text('\n'.join(packages.values()) + '\n')
    return records


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('package', type=Path)
    parser.add_argument('--qmake', default=os.environ.get('QMAKE') or
                        shutil.which('qmake-qt5') or shutil.which('qmake') or 'qmake')
    args = parser.parse_args()
    root = args.package.resolve()
    binary = root / 'bin/MadModem'
    if not binary.is_file() or not elf(binary):
        raise RuntimeError('Expected a freshly staged ELF executable in bin/MadModem')
    version = run(args.qmake, '-query', 'QT_VERSION').stdout.strip()
    if not version.startswith('5.'):
        raise RuntimeError(f'Linux packaging expects Qt5, found {version}')
    plugins = Path(run(args.qmake, '-query', 'QT_INSTALL_PLUGINS').stdout.strip())
    qml = Path(run(args.qmake, '-query', 'QT_INSTALL_QML').stdout.strip())
    for needed in ('platforms/libqxcb.so', 'platforms/libqoffscreen.so', 'sqldrivers/libqsqlite.so',
                   'geoservices/libqtgeoservices_osm.so'):
        if not (plugins / needed).is_file():
            raise RuntimeError(f'Missing Qt runtime plugin: {plugins / needed}')
    if not (plugins / 'audio').is_dir() or not list((plugins / 'audio').glob('*.so')):
        raise RuntimeError('Missing Qt audio backend plugins; install the Qt multimedia runtime plugins')
    for name in QML_MODULES:
        if not (qml / name / 'qmldir').is_file():
            raise RuntimeError(f'Missing QML runtime module: {qml / name}')
    libdir = root / 'lib'
    libdir.mkdir(exist_ok=True)
    origins = {}
    for parent, target, groups in ((plugins, root / 'plugins', PLUGIN_GROUPS),
                                   (qml, root / 'qml', ('QtQuick.2', 'QtQuick', 'QtQml', 'QtLocation', 'QtPositioning'))):
        for name in groups:
            source = parent / name
            if not source.is_dir():
                continue
            shutil.copytree(source, target / name, dirs_exist_ok=True, symlinks=False)
            for file in source.rglob('*'):
                if file.is_file():
                    origins[target / name / file.relative_to(source)] = file
    seeds = [binary] + [p for p in origins if elf(p)]
    # QSslSocket resolves OpenSSL at runtime, absent from its ELF NEEDED entries.
    cache = run('ldconfig', '-p').stdout
    for library in ('ssl', 'crypto'):
        soname = ctypes.util.find_library(library)
        match = re.search(r'^\s*' + re.escape(soname or 'MISSING') + r'\s+\(.*\) => (.+)$', cache, re.M)
        if not match:
            raise RuntimeError(f'Missing dynamically loaded OpenSSL library: {library}')
        source = Path(match[1]); target = libdir / soname
        shutil.copy2(source, target); origins[target] = source; seeds.append(source)
    origins.update(deploy_closure(seeds, libdir))
    if not list(libdir.glob('libhamlib.so*')):
        raise RuntimeError('Expected shared Hamlib in the Linux release dependency closure')
    # Strip build-machine RPATHs when present; launcher supplies a private path.
    for file in [binary] + [p for p in origins if elf(p)]:
        dynamic = run('readelf', '-d', file).stdout
        if '(RPATH)' in dynamic or '(RUNPATH)' in dynamic:
            run('patchelf', '--remove-rpath', file)
    (root / 'bin/qt.conf').write_text('[Paths]\nPrefix=..\nLibraries=lib\nPlugins=plugins\nQml2Imports=qml\n')
    binary.rename(root / 'bin/MadModem.bin')
    binary.write_text('''#!/bin/sh
set -eu
MM_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export LD_LIBRARY_PATH="$MM_ROOT/lib"
export QT_PLUGIN_PATH="$MM_ROOT/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$MM_ROOT/plugins/platforms"
export QML2_IMPORT_PATH="$MM_ROOT/qml"
export QML_IMPORT_PATH="$MM_ROOT/qml"
exec "$MM_ROOT/bin/MadModem.bin" "$@"
''')
    binary.chmod(0o755)
    launcher = root / 'AppRun'
    launcher.write_text('#!/bin/sh\nset -eu\nMM_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec "$MM_ROOT/bin/MadModem" "$@"\n')
    launcher.chmod(0o755)
    # Verify closure using ONLY the bundled search path, including every plugin.
    env = dict(os.environ, LD_LIBRARY_PATH=str(libdir))
    for file in [root / 'bin/MadModem.bin'] + [p for p in origins if elf(p)]:
        for name, resolved in dependencies(file, env):
            if not HOST.fullmatch(name) and not resolved.resolve().is_relative_to(root):
                raise RuntimeError(f'Runtime escapes portable directory: {file}: {name} => {resolved}')
    records = copy_licenses(origins, root / 'runtime-licenses')
    for record in records:
        record['file'] = str(Path(record['file']).relative_to(root))
    (root / 'runtime-manifest.json').write_text(json.dumps({'qt': version, 'libraries_and_plugins': records}, indent=2) + '\n')
    print(f'Bundled Qt {version}, Hamlib, plugins/QML and {len(list(libdir.iterdir()))} runtime libraries')


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        sys.exit(f'ERROR: {error}')
