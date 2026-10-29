#!/usr/bin/env python3
"""Compile and run the real app decoder regression (Unix Makefiles build)."""
import argparse,os,pathlib,shlex,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',required=True,type=pathlib.Path);a=p.parse_args();b=a.build.resolve();s=pathlib.Path(__file__).resolve().parents[1]
f={}
for line in (b/'CMakeFiles/MadModem.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);f[k]=shlex.split(v)
out=b/'ft-coherent-regression';obj=out.with_suffix('.o');link=shlex.split((b/'CMakeFiles/MadModem.dir/link.txt').read_text())
subprocess.run([link[0],*f['CXX_DEFINES'],*f['CXX_INCLUDES'],*f['CXX_FLAGS'],'-fno-access-control','-O1','-c',str(s/'tests/FtCoherentRecoveryRegression.cpp'),'-o',str(obj)],cwd=b,check=True)
link[link.index('-o')+1]=str(out);link[link.index('CMakeFiles/MadModem.dir/main.cpp.o')]=str(obj)
subprocess.run(link,cwd=b,check=True);out.chmod(0o755);subprocess.run([str(out)],check=True)
