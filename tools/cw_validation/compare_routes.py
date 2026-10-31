#!/usr/bin/env python3
"""Reproduce R12 comparisons using actual CwDecoder route probes.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse,hashlib,json,statistics,subprocess,sys,tempfile
from pathlib import Path
import numpy as np
from scipy.signal import resample_poly
SOURCE=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(SOURCE/'tools/cw_neural'))
from qso_refine import qso_record
from refine import negative
from real_compare import normalize,edit_counts,read_wav

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--old-probe',type=Path,required=True);ap.add_argument('--new-probe',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--real-manifest',type=Path);ap.add_argument('--cpu-repeats',type=int,default=3);args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=True)
 probes={'Native_R8':(args.old_probe.resolve(),'native'),'Native_R12':(args.new_probe.resolve(),'native'),'Neural_R8':(args.old_probe.resolve(),'neural_ctc'),'Neural_R12_Envelope':(args.new_probe.resolve(),'neural_envelope')}
 def run(path,rate,tone,name):
  probe,mode=probes[name];return json.loads(subprocess.check_output([str(probe),mode,str(rate),str(tone),str(path),'997'],text=True,timeout=120))
 with tempfile.TemporaryDirectory(prefix='mm-cw-compare-') as td:
  audio_path=Path(td)/'audio.f32';rows=[]
  for i in range(16):
   rec,a,meta=qso_record(124000000+i,'test',True);a=np.pad(resample_poly(a,2,1),(0,24000)).astype('<f4');a.tofile(audio_path);ref=normalize(rec[3]);m={}
   for name in probes:m[name]=edit_counts(ref,normalize(run(audio_path,8000,1000,name)['text']))
   rows.append({'seed':124000000+i,'wpm':meta['wpm'],'width_hz':meta['width_hz'],'input_sha256':hashlib.sha256(a.tobytes()).hexdigest(),'metrics':m})
  summary={}
  for name in probes:
   metrics=[r['metrics'][name] for r in rows];e=sum(m['errors'] for m in metrics);c=sum(m['characters'] for m in metrics);summary[name]={'errors':e,'characters':c,'cer':e/c}
  neg={name:0 for name in probes}
  for i in range(4):
   _,a=negative(125000000+i,seconds=10.,return_audio=True);resample_poly(a,2,1).astype('<f4').tofile(audio_path)
   for name in probes:neg[name]+=len(run(audio_path,8000,1000,name)['text'].strip())
  result={'synthetic_summary':summary,'negative_false_characters':neg,'synthetic_records':rows,'rf':[],'cpu':{}}
  if args.real_manifest:
   manifest=json.loads(args.real_manifest.read_text());records=[]
   for item in manifest['recordings']:
    rate,a=read_wav(args.real_manifest.parent/item['wav']);a=np.pad(a[round(item['start_seconds']*rate):round(item['end_seconds']*rate)],(0,round(rate*manifest.get('flush_seconds',3)))).astype('<f4');p=Path(td)/(item['id']+'.f32');a.tofile(p);records.append((item,p,rate));ref=normalize(item['reference']);metrics={}
    for name in probes:metrics[name]=edit_counts(ref,normalize(run(p,rate,item['tone_hz'],name)['text']))
    result['rf'].append({'id':item['id'],'metrics':metrics,'input_sha256':hashlib.sha256(a.tobytes()).hexdigest()})
   timing={name:[] for name in probes}
   for repeat in range(args.cpu_repeats):
    names=list(probes);names=names[repeat%len(names):]+names[:repeat%len(names)]
    for name in names:
     cpu=0
     for item,p,rate in records:cpu+=run(p,rate,item['tone_hz'],name)['cpu_seconds']
     timing[name].append(cpu)
   result['cpu']={name:{'median_cpu_seconds':statistics.median(v),'runs':v} for name,v in timing.items() if v}
  (args.out/'COMPARISON.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(summary,indent=2))
if __name__=='__main__':main()
