#!/usr/bin/env python3
"""Run the real self-extracting installer with mocked package tools; no system changes."""
from pathlib import Path
import hashlib,io,os,subprocess,tarfile,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='mm-installer-preflight-') as temp:
    base=Path(temp);mock=base/'mock';mock.mkdir();log=base/'commands'
    def command(name,body):
        p=mock/name;p.write_text('#!/bin/bash\n'+body+'\n');p.chmod(0o755)
    command('dpkg', '''[[ "$*" == --audit ]] || exit 90
case "$FIXTURE_STATE" in
 interrupted) echo 'A package was unpacked but not configured';;
 failed) exit 2;;
esac''')
    command('find', '[[ "$FIXTURE_STATE" != journal ]] || echo /var/lib/dpkg/updates/0001')
    command('apt-get','printf "%s\\n" "$*" >> "$FIXTURE_LOG"')
    command('sudo','exec "$@"')
    data=io.BytesIO()
    with tarfile.open(fileobj=data,mode='w:gz') as tar:
        for name in ('fixture.deb','INSTALLER_LICENSE.txt'):
            content=b'fixture';info=tarfile.TarInfo(name);info.size=len(content);tar.addfile(info,io.BytesIO(content))
    payload=data.getvalue()
    script=(root/'packaging/linux-installer.sh.in').read_text().replace('@VERSION@','0.6.4').replace('@PACKAGE@','fixture.deb').replace('@SHA256@',hashlib.sha256(payload).hexdigest())
    installer=base/'installer.run';installer.write_bytes(script.encode()+payload)
    env=dict(os.environ,PATH=str(mock)+':'+os.environ['PATH'],DISPLAY='',WAYLAND_DISPLAY='',FIXTURE_LOG=str(log))
    for state in ('interrupted','failed','journal','clean'):
        log.unlink(missing_ok=True)
        result=subprocess.run(['bash',str(installer),'--accept-gpl3'],env=dict(env,FIXTURE_STATE=state),text=True,capture_output=True,timeout=5)
        if state=='clean':
            assert result.returncode==0,(result.stdout,result.stderr)
            assert '--reinstall' in log.read_text()
        else:
            assert result.returncode==32,(state,result.stdout,result.stderr)
            assert 'sudo dpkg --configure -a' in result.stderr
            assert not log.exists(),'apt changed a system with incomplete dpkg state'
        print('PASS installer preflight',state)
