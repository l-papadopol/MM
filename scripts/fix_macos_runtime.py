#!/usr/bin/env python3
"""Finish macdeployqt's dependency relocation, then validate and sign the bundle."""
import argparse
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys

MAGIC = {bytes.fromhex(x) for x in ('feedface', 'cefaedfe', 'feedfacf', 'cffaedfe', 'cafebabe', 'bebafeca', 'cafebabf', 'bfbafeca')}
LOADS = {'LC_LOAD_DYLIB', 'LC_LOAD_WEAK_DYLIB', 'LC_REEXPORT_DYLIB', 'LC_LOAD_UPWARD_DYLIB', 'LC_LAZY_LOAD_DYLIB'}


def run(*args):
    return subprocess.check_output([str(a) for a in args], text=True, stderr=subprocess.STDOUT)


def macho(path):
    if not path.is_file():
        return False
    with path.open('rb') as file:
        return file.read(4) in MAGIC


def commands(binary):
    dependencies, rpaths, identities = [], [], []
    command = ''
    for line in run('otool', '-l', binary).splitlines():
        fields = line.strip().split()
        if len(fields) == 2 and fields[0] == 'cmd':
            command = fields[1]
        match = re.match(r'\s*(?:name|path) (.+) \(offset \d+\)', line)
        if match:
            if command in LOADS:
                dependencies.append(match[1])
            elif command == 'LC_RPATH':
                rpaths.append(match[1])
            elif command == 'LC_ID_DYLIB':
                identities.append(match[1])
    return list(dict.fromkeys(dependencies)), list(dict.fromkeys(rpaths)), identities


def system(ref):
    return ref.startswith(('/System/Library/', '/usr/lib/'))


def expand(ref, binary, executable):
    return Path(ref.replace('@loader_path', str(binary.parent)).replace('@executable_path', str(executable.parent)))


def binaries(app):
    return sorted({p.resolve() for p in app.rglob('*') if macho(p)})


def repair(app, brew):
    frameworks = app / 'Contents/Frameworks'
    executable = app / 'Contents/MacOS/MadModem'
    search = [brew / 'lib'] + sorted((brew / 'opt').glob('*/lib'))
    queue = binaries(app)
    seen = set()
    while queue:
        binary = queue.pop(0)
        if binary in seen:
            continue
        if not binary.is_relative_to(app):
            raise RuntimeError(f'External binary symlink: {binary}')
        seen.add(binary)
        binary.chmod(binary.stat().st_mode | stat.S_IWUSR)
        deps, rpaths, identities = commands(binary)
        for ref in deps:
            if system(ref):
                continue
            framework = re.search(r'([^/]+\.framework/.+)$', ref)
            target = frameworks / (framework[1] if framework else Path(ref).name)
            if not macho(target):
                # The shell stage already deploys complete Qt framework bundles.
                # Never silently copy just a framework executable without resources.
                if framework:
                    raise RuntimeError(f'Missing framework required by {binary}: {ref}')
                candidates = []
                if not ref.startswith('@rpath/'):
                    candidates.append(expand(ref, binary, executable))
                else:
                    candidates.extend(expand(p, binary, executable) / ref[7:] for p in rpaths if not p.startswith('@rpath'))
                source = next((p for p in candidates if macho(p)), None)
                if source is None:
                    matches = { (p / target.name).resolve() for p in search if macho(p / target.name) }
                    if len(matches) != 1:
                        raise RuntimeError(f'Cannot resolve uniquely {ref} required by {binary}: {sorted(map(str, matches))}')
                    source = matches.pop()
                print(f'Bundle {source} -> {target}', flush=True)
                shutil.copy2(source, target)
            target = target.resolve()
            if not target.is_relative_to(app):
                raise RuntimeError(f'Dependency outside bundle: {target}')
            portable = '@loader_path/' + os.path.relpath(target, binary.parent)
            if portable != ref:
                run('install_name_tool', '-change', ref, portable, binary)
            queue.append(target)
        if identities:
            run('install_name_tool', '-id', '@rpath/' + str(binary.relative_to(frameworks))
                if binary.is_relative_to(frameworks) else '@rpath/' + binary.name, binary)
    verify(app)


def verify(app):
    executable = app / 'Contents/MacOS/MadModem'
    required_archs = set(run('lipo', '-archs', executable).split())
    for binary in binaries(app):
        if not required_archs.issubset(run('lipo', '-archs', binary).split()):
            raise RuntimeError(f'Wrong architecture in {binary}; expected {sorted(required_archs)}')
        if not binary.is_relative_to(app):
            raise RuntimeError(f'External binary symlink: {binary}')
        deps, _, _ = commands(binary)
        for ref in deps:
            if system(ref):
                continue
            if not ref.startswith(('@loader_path/', '@executable_path/')):
                raise RuntimeError(f'Unrelocated dependency in {binary}: {ref}')
            target = expand(ref, binary, executable).resolve()
            if not target.is_relative_to(app) or not macho(target):
                raise RuntimeError(f'Broken dependency in {binary}: {ref} -> {target}')
    print('PASS all Mach-O dependency paths resolve inside the bundle or macOS', flush=True)


def sign(app):
    # Sign from the inside out after every install_name_tool/resource change.
    # In particular, macdeployqt can leave copied Brotli/WebP dylibs with stale signatures.
    for binary in binaries(app):
        run('codesign', '--force', '--sign', '-', binary)
    containers = sorted((p for p in app.rglob('*') if p.is_dir() and not p.is_symlink()
                         and p.suffix in {'.framework', '.app', '.xpc', '.bundle'}),
                        key=lambda p: len(p.parts), reverse=True)
    for container in containers:
        run('codesign', '--force', '--sign', '-', container)
    run('codesign', '--force', '--sign', '-', app)
    run('codesign', '--verify', '--deep', '--strict', app)
    print('PASS final bundle signature', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('app', type=Path)
    parser.add_argument('--brew-prefix', type=Path, required=True)
    parser.add_argument('--sign', action='store_true')
    args = parser.parse_args()
    app = args.app.resolve()
    repair(app, args.brew_prefix)
    if args.sign:
        sign(app)


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(f'ERROR: {error}\n{error.output}')
    except (RuntimeError, OSError) as error:
        sys.exit(f'ERROR: {error}')
