#!/usr/bin/env python3
"""Local publication metadata and portable updater integration checks; no network."""
from pathlib import Path
import hashlib
import importlib.util
import os
import subprocess
import tarfile
import tempfile
import zipfile
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('stamp', ROOT/'scripts/stamp_release_notes.py')
stamp=importlib.util.module_from_spec(spec);spec.loader.exec_module(stamp)
notes=stamp.stamp('Custom operator notes\n<!-- MadModem build: 113 -->\n',115,'a'*40)
assert 'Custom operator notes' in notes and '113' not in notes and notes.count('MadModem build:')==1
notes2=stamp.stamp(notes,116,'b'*40)
assert '115' not in notes2 and '116' in notes2 and notes2.count('MadModem build:')==1
assert '<!-- MadModem commit: '+'b'*40+' -->' in notes2
print('PASS release-note identity refresh preserves operator notes')
native_spec=importlib.util.spec_from_file_location('native_package', ROOT/'scripts/package_linux_installer.py')
native=importlib.util.module_from_spec(native_spec);native_spec.loader.exec_module(native)
with tempfile.TemporaryDirectory(prefix='mm-native-id-') as t:
    build=Path(t);(build/'generated').mkdir()
    header=build/'generated/MadModemBuildInfo.h'
    header.write_text('#define MADMODEM_BUILD_NUMBER "115"\n')
    assert native.distribution_release(build,'7','115')=='115'
    header.write_text('#define MADMODEM_BUILD_NUMBER "116"\n')
    assert native.distribution_release(build,'7','116')=='116'
    try:native.distribution_release(build,'7','115')
    except SystemExit:pass
    else:raise AssertionError('mixed executable/package build accepted')
    header.write_text('#define MADMODEM_BUILD_NUMBER "0"\n')
    assert native.distribution_release(build,'7')=='7'
print('PASS native package build ordering, local fallback and mixed-build rejection')
with tempfile.TemporaryDirectory(prefix='mm-update-check-') as t:
    base=Path(t)
    for label,actual,expected in [('same-build',115,115),('wrong-build',114,115),('zip-same-build',115,115),('zip-wrong-build',114,115)]:
        case=base/label;target=case/'portable copy';source=case/'stage/package'
        (target/'bin').mkdir(parents=True);(source/'bin').mkdir(parents=True)
        old='#!/bin/bash\necho "old installation"\n'
        (target/'bin/MadModem').write_text(old);(target/'bin/MadModem').chmod(0o755)
        (target/'settings.mad').write_text('fixture user state\n')
        new=f'''#!/bin/bash
if [[ "${{1:-}}" == --version ]]; then echo 'MadModem 0.6.4'; exit; fi
if [[ "${{1:-}}" == --build-info ]]; then printf 'MadModem 0.6.4\\nBuild {actual}\\nCommit aaaa\\n'; exit; fi
exit 0
'''
        (source/'bin/MadModem').write_text(new);(source/'bin/MadModem').chmod(0o755)
        archive=case/('release.zip' if label.startswith('zip-') else 'release.tar.gz')
        if label.startswith('zip-'):
            with zipfile.ZipFile(archive,'w') as z:
                for f in source.rglob('*'):z.write(f,Path('package')/f.relative_to(source))
        else:
            with tarfile.open(archive,'w:gz') as tar:tar.add(source,arcname='package')
        sha=hashlib.sha256(archive.read_bytes()).hexdigest()
        p=subprocess.run(['bash',str(ROOT/'updates/apply_portable.sh'),'2147483647',str(target),str(archive),sha,'0.6.4','linux',str(expected)],capture_output=True,text=True,timeout=25)
        assert (target/'settings.mad').read_text()=='fixture user state\n'
        if actual==expected:
            assert p.returncode==0,(p.stdout,p.stderr)
            assert (target/'bin/MadModem').read_text()==new
        else:
            assert p.returncode!=0
            assert (target/'bin/MadModem').read_text()==old
        assert list(case.glob('portable copy.update.*/backup')) or list(case.glob('portable copy.update.*/failed-update'))
        print('PASS portable',label,'state preservation', 'and rollback' if actual!=expected else 'and install')
