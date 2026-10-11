#!/usr/bin/env python3
"""Build native packages on the target distribution, using its own dependency resolver."""
import argparse
import hashlib
import tarfile
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def run(*args, **kwargs):
    try:
        return subprocess.run([str(x) for x in args], check=True, **kwargs)
    except subprocess.CalledProcessError as error:
        if error.stderr:
            print(error.stderr, file=sys.stderr)
        raise

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--format', choices=['deb', 'rpm'], required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'dist/packages')
    args = parser.parse_args()
    version = (ROOT / 'MADMODEM_VERSION.txt').read_text().strip()
    revision = re.search(r'-r(\d+)-', (ROOT / 'MADMODEM_SOURCE_REVISION.txt').read_text())
    if not re.fullmatch(r'\d+\.\d+\.\d+', version) or not revision:
        raise SystemExit('Native installer requires a numeric release and source revision.')
    release = revision.group(1)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='madmodem-package-') as tmp:
        work = Path(tmp)
        payload = work / 'payload'
        private = payload / 'usr/lib/madmodem'
        run('cmake', '--install', args.build_dir.resolve(), '--prefix', '/usr/lib/madmodem',
            env={**os.environ, 'DESTDIR': str(payload)})
        binary = private / 'bin/MadModem'
        if not binary.is_file():
            raise SystemExit('Installed executable missing')
        # Native packages use system libraries. Never silently ship an untracked
        # private Hamlib shared object that bypasses the package dependency resolver.
        for pattern in ('lib*/libhamlib.so*',):
            for lib in private.glob(pattern):
                lib.unlink()
        (private / 'bin/managed-install.flag').write_text('MadModem managed installation\n')
        shared = payload / 'usr/share'
        shutil.move(str(private / 'share'), str(shared))
        bindir = payload / 'usr/bin'
        bindir.mkdir(parents=True)
        launcher = bindir / 'MadModem'
        launcher.write_text('#!/bin/sh\nexec /usr/lib/madmodem/bin/MadModem "$@"\n')
        launcher.chmod(0o755)
        documentation = shared / 'doc/madmodem'
        documentation.mkdir(parents=True)
        for name in ('COPYING', 'LICENSE.md', 'THIRD_PARTY_NOTICES.md', 'RELEASE_NOTES.md'):
            shutil.copy2(ROOT / name, documentation / name)
        for name in ('rtty_rules', 'cw_rules', 'cty.csv'):
            if not (private / 'bin' / name).is_file():
                raise SystemExit(f'Missing runtime data: {name}')
        if args.format == 'deb':
            (work / 'debian').mkdir()
            (work / 'debian/control').write_text('Source: madmodem\nSection: hamradio\nPriority: optional\nMaintainer: Papadopol Lucian-Ioan <l.i.papadopol@gmail.com>\n\nPackage: madmodem\nArchitecture: any\nDescription: Amateur radio digital modem\n')
            native_env = {key: value for key, value in os.environ.items() if key != 'LD_LIBRARY_PATH'}
            result = run('dpkg-shlibdeps', '-O', '-e' + str(binary), cwd=work, capture_output=True, text=True, env=native_env)
            dependencies = next(line.split('=', 1)[1] for line in result.stdout.splitlines() if line.startswith('shlibs:Depends='))
            # Qt imports and plugins loaded dynamically are absent from ELF NEEDED.
            dependencies += ', qml-module-qtquick2, qml-module-qtquick-window2, qml-module-qtlocation, qml-module-qtpositioning, libqt5multimedia5-plugins, libqt5sql5-sqlite'
            arch = run('dpkg', '--print-architecture', capture_output=True, text=True).stdout.strip()
            control = payload / 'DEBIAN'
            control.mkdir()
            (control / 'control').write_text(f'Package: madmodem\nVersion: {version}-{release}\nArchitecture: {arch}\nSection: hamradio\nPriority: optional\nMaintainer: Papadopol Lucian-Ioan <l.i.papadopol@gmail.com>\nHomepage: https://www.madexp.it\nDepends: {dependencies}\nDescription: Amateur radio digital modem and logbook\n CW, RTTY, FT8/FT4 and other digital modes with CAT control.\n')
            package = output / f'MadModem-{version}-{release}-Ubuntu24.04-{arch}.deb'
            run('dpkg-deb', '--root-owner-group', '--build', payload, package)
            run('dpkg-deb', '--info', package)
        else:
            for name in ('BUILD', 'RPMS', 'SOURCES', 'SPECS', 'SRPMS'):
                (work / name).mkdir()
            spec = work / 'SPECS/madmodem.spec'
            # All paths come from a private mkdtemp directory, never user metadata.
            spec.write_text(f'''Name: madmodem
Version: {version}
Release: {release}%{{?dist}}
Summary: Amateur radio digital modem and logbook
License: GPL-3.0-only
URL: https://www.madexp.it
Requires: qt5-qtdeclarative, qt5-qtlocation, qt5-qtmultimedia, qt5-qtbase-gui
%global debug_package %{{nil}}
%description
CW, RTTY, FT8/FT4 and other digital modes with CAT control.
%install
mkdir -p %{{buildroot}}
cp -a {payload}/. %{{buildroot}}/
%files
/usr/bin/MadModem
/usr/lib/madmodem
/usr/share/applications/MadModem.desktop
/usr/share/icons/hicolor/*/apps/*.png
%doc /usr/share/doc/madmodem
''')
            run('rpmbuild', '-bb', '--define', f'_topdir {work}', spec)
            packages = list((work / 'RPMS').rglob('*.rpm'))
            if len(packages) != 1:
                raise SystemExit('Expected exactly one RPM')
            package = output / packages[0].name
            shutil.copy2(packages[0], package)

        license_file = work / 'INSTALLER_LICENSE.txt'
        license_file.write_bytes((ROOT / 'packaging/COPYRIGHT.txt').read_bytes() + (ROOT / 'COPYING').read_bytes())
        payload_archive = work / 'installer.tar.gz'
        with tarfile.open(payload_archive, 'w:gz') as archive:
            archive.add(package, arcname=package.name)
            archive.add(license_file, arcname=license_file.name)
        template = (ROOT / 'packaging/linux-installer.sh.in').read_text()
        template = template.replace('@VERSION@', version).replace('@PACKAGE@', package.name)
        template = template.replace('@SHA256@', hashlib.sha256(payload_archive.read_bytes()).hexdigest())
        installer = output / (package.stem + '-Setup.run')
        installer.write_bytes(template.encode() + payload_archive.read_bytes())
        installer.chmod(0o755)
        print(f'Installer: {installer}')

if __name__ == '__main__':
    main()
