#!/usr/bin/env python3
"""Execute the real publisher against a local gh double; nothing is published."""
from pathlib import Path
import json
import os
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
version=(ROOT/'MADMODEM_VERSION.txt').read_text().strip()
with tempfile.TemporaryDirectory(prefix='mm-publish-check-') as tmp:
    t=Path(tmp);assets=t/'assets';assets.mkdir();bin=t/'bin';bin.mkdir()
    files=[]
    for platform in ('Ubuntu24.04','Fedora43'):
        for ext in ('zip','tar.gz'):files.append(f'MadModem-{version}-{platform}-x86_64.{ext}')
    files += [f'MadModem-{version}-115-Ubuntu24.04-amd64-Setup.run',f'madmodem-{version}-115.fc43.x86_64-Setup.run']
    files += [f'MadModem-{version}-115-Ubuntu24.04-amd64.deb',f'madmodem-{version}-115.fc43.x86_64.rpm']
    for cpu in ('legacy','avx2'):files.append(f'MadModem-{version}-Windows-x86_64-{cpu}-standalone.zip')
    files.append(f'MadModem-{version}-Windows-x64-Setup.exe')
    for arch in ('x86_64','arm64'):
        for ext in ('zip','dmg','pkg'):files.append(f'MadModem-{version}-macOS-{arch}-unsigned.{ext}')
    for name in files:(assets/name).write_bytes(b'LOCAL FIXTURE ONLY')
    gh=bin/'gh'
    gh.write_text('''#!/usr/bin/env python3
import os,sys,json,hashlib
from pathlib import Path
p=Path(os.environ['MM_GH_STATE']);d=json.loads(p.read_text()) if p.exists() else {'body':'','exists':False,'calls':[]}
a=sys.argv[1:];d['calls'].append(a);result='';status=0
if a[:2]==['release','view']:
 key=a[a.index('--json')+1]
 if key=='id': status=0 if d['exists'] else 1
 elif key=='assets':result=''
 elif key=='body':result=d['body']
 elif key=='url':result='https://example.invalid/local-test'
elif a[:2]==['release','create']:
 d['exists']=True;d['body']=Path(a[a.index('--notes-file')+1]).read_text()
elif a[:2]==['release','edit']:
 d['body']=Path(a[a.index('--notes-file')+1]).read_text()
elif a[:2]==['release','upload']:
 paths=[Path(x) for x in a[3:] if not x.startswith('--')]
 d['uploaded']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
 d['checksums']=next(p for p in paths if p.name=='SHA256SUMS.txt').read_text()
else:raise RuntimeError(a)
p.write_text(json.dumps(d));print(result);sys.exit(status)
''');gh.chmod(0o755)
    state=t/'state.json'
    env={**os.environ,'PATH':str(bin)+os.pathsep+os.environ['PATH'],'MM_GH_STATE':str(state),'GH_TOKEN':'local-test-token','GH_REPO':'example/local-test','GITHUB_SHA':'a'*40,'GITHUB_RUN_NUMBER':'115','GITHUB_REF_TYPE':'branch','GITHUB_REF_NAME':'main'}
    for run in range(2):
        if run:
            d=json.loads(state.read_text());d['body']='Custom operator note\n'+d['body'];state.write_text(json.dumps(d))
        subprocess.run(['bash',str(ROOT/'scripts/publish_github_release.sh'),str(assets)],env=env,check=True,capture_output=True,text=True)
    d=json.loads(state.read_text())
    assert 'Custom operator note' in d['body'] and d['body'].count('MadModem build:')==1
    assert '<!-- MadModem build: 115 -->' in d['body']
    edits=[x for x in d['calls'] if x[:2]==['release','edit']]
    assert len(edits)==2 and all('--prerelease=true' in x and '--latest=false' in x for x in edits)
    creates=[x for x in d['calls'] if x[:2]==['release','create']]
    assert len(creates)==1 and creates[0][2]==f'build-{version}-115'
    print('PASS publisher creates a numbered prerelease without Latest, preserves edited notes and stamps one build identity on rerun')
    expected_native={f'MadModem-{version}-Linux-Ubuntu24.04-64bit-Package.deb',f'MadModem-{version}-Linux-Fedora43-64bit-Package.rpm'}
    assert expected_native <= d['uploaded'].keys()
    assert len(d['uploaded'])==18, d['uploaded'].keys()
    checksums={name:sha for sha,name in (line.split('  ',1) for line in d['checksums'].splitlines())}
    assert checksums=={name:sha for name,sha in d['uploaded'].items() if name!='SHA256SUMS.txt'}
    for extension in ('deb','rpm'):
        package=next(assets.glob('*.'+extension));saved=package.read_bytes();package.unlink()
        before=state.read_bytes()
        result=subprocess.run(['bash',str(ROOT/'scripts/publish_github_release.sh'),str(assets)],env=env,capture_output=True,text=True)
        assert result.returncode!=0 and state.read_bytes()==before, 'partial native release was published'
        package.write_bytes(saved)
    ambiguous=assets/f'MadModem-{version}-116-Ubuntu24.04-amd64.deb';ambiguous.write_bytes(b'ambiguous build')
    before=state.read_bytes()
    result=subprocess.run(['bash',str(ROOT/'scripts/publish_github_release.sh'),str(assets)],env=env,capture_output=True,text=True)
    assert result.returncode!=0 and state.read_bytes()==before, 'ambiguous native build was published'
    print('PASS DEB/RPM and existing 15 downloads uploaded with exact checksums; missing/ambiguous native packages prevent publication before any gh operation')
