#!/usr/bin/env python3
"""Linux/GCC developer probe: replay the built app's actual FT decoder objects.
Configure/build MadModem first with Unix Makefiles. No RF/audio devices are used.
"""
import argparse, os, pathlib, shlex, subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',required=True,type=pathlib.Path)
p.add_argument('--wav',required=True,type=pathlib.Path)
p.add_argument('--offset',type=float,default=0)
p.add_argument('--workers',type=int,default=8)
p.add_argument('--late-ms',type=int,default=0)
a=p.parse_args();b=a.build.resolve();source=pathlib.Path(__file__).resolve().parents[1]
f={}
for line in (b/'CMakeFiles/MadModem.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);f[k]=shlex.split(v)
out=b/'ft-nearby-workflow-probe';obj=out.with_suffix('.o')
link=shlex.split((b/'CMakeFiles/MadModem.dir/link.txt').read_text())
subprocess.run([link[0],*f['CXX_DEFINES'],*f['CXX_INCLUDES'],*f['CXX_FLAGS'],'-fno-access-control','-O1','-c',str(source/'tests/FtNearbyWorkflowProbe.cpp'),'-o',str(obj)],cwd=b,check=True)
link[link.index('-o')+1]=str(out)
link[link.index('CMakeFiles/MadModem.dir/main.cpp.o')]=str(obj)
subprocess.run(link,cwd=b,check=True);out.chmod(0o755)
env=dict(os.environ,PROBE_WORKERS=str(max(1,a.workers)),PROBE_OFFSET_SEC=str(a.offset),PROBE_LATE_MS=str(max(0,a.late_ms)))
subprocess.run([str(out),str(a.wav.resolve())],env=env,check=True)
