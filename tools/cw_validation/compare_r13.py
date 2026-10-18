#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import json,sys,subprocess,hashlib,os,tempfile
import numpy as np
from scipy.signal import resample_poly
import argparse
ap=argparse.ArgumentParser();ap.add_argument('--baseline-probe',type=Path,required=True);ap.add_argument('--new-probe',type=Path,required=True);ap.add_argument('--search-r8-probe',type=Path);ap.add_argument('--extra-pool',type=Path,required=True);ap.add_argument('--rf-manifest',type=Path);ap.add_argument('--out',type=Path,required=True);args=ap.parse_args()
R=args.out.resolve();R.mkdir(parents=True,exist_ok=True);S=Path(__file__).resolve().parents[2];sys.path.insert(0,str(S/'tools/cw_neural'));sys.path.insert(0,str(S/'tools/cw_validation'))
from train import example
from refine import negative
from qso_refine import qso_record,RESERVED
from real_compare import normalize,edit_counts
old=args.baseline_probe.resolve();new=args.new_probe.resolve()
engines={'Native_R12':(old,'native'),'Neural_CTC_R12':(old,'neural_ctc'),'Neural_Envelope_R12':(old,'neural_envelope'),'Search_R13':(new,'neural_search')}
env=os.environ.copy()
if args.search_r8_probe:engines['Search_R8_ablation']=(args.search_r8_probe.resolve(),'neural_search')
outdir=R/'comparison';outdir.mkdir(exist_ok=True);rows=[];private=[]

def invoke(name,path,rate,tone):
 bin,mode=engines[name];return json.loads(subprocess.check_output([str(bin),mode,str(rate),str(tone),str(path),'997'],env=env,text=True,timeout=120))
def run(group,id,audio,rate,tone,reference,meta=None):
 audio=np.asarray(audio,dtype='<f4');p=R/'final-input.f32';audio.tofile(p);ref=normalize(reference)
 with ThreadPoolExecutor(max_workers=4) as pool:values=list(pool.map(lambda n:(n,invoke(n,p,rate,tone)),engines))
 row={'group':group,'id':id,'reference_sha256':hashlib.sha256(ref.encode()).hexdigest(),'input_sha256':hashlib.sha256(audio.tobytes()).hexdigest(),'audio_seconds':len(audio)/rate,'metrics':{}}
 for name,value in values:
  hyp=normalize(value['text']);m=edit_counts(ref,hyp);m['hypothesis_sha256']=hashlib.sha256(hyp.encode()).hexdigest();m['exact']=hyp==ref;m['false_characters']=len(hyp) if not ref else 0
  if meta and 'calls' in meta:
   calls=[x for x in meta['calls'] if x in ref.split()];m['callsigns']={'correct':sum(c in hyp.split() for c in calls),'total':len(calls)}
  row['metrics'][name]=m
 private.append({'group':group,'id':id,'reference':ref,'outputs':dict(values),'meta':meta});rows.append(row);print(group,id,flush=True)
 (outdir/'partial-public.json').write_text(json.dumps(rows,indent=2)+'\n')

def summarize():
 results={}
 for group in dict.fromkeys(x['group'] for x in rows):
  selected=[x for x in rows if x['group']==group];g={}
  for name in engines:
   ms=[x['metrics'][name] for x in selected];c=sum(x['characters'] for x in ms);e=sum(x['errors'] for x in ms)
   g[name]={'errors':e,'characters':c,'cer':e/c if c else None,'exact_records':sum(x['exact'] for x in ms),'records':len(ms),'false_characters':sum(x['false_characters'] for x in ms)}
   if any('callsigns' in x for x in ms):g[name]['callsigns']={k:sum(x.get('callsigns',{}).get(k,0) for x in ms) for k in ['correct','total']}
  results[group]=g
 return results

for i in range(32):
 rec,a,meta=qso_record(137000000+i,'test',True);a=np.pad(resample_poly(a,2,1),(0,24000));run('fresh_qso',str(137000000+i),a,8000,1000,rec[3],meta)
for i,text in enumerate(sorted(RESERVED)):
 rec,a=example(140000000+i,difficulty=.15,text=text,wpm=20,return_audio=True);run('fixed_qso_regression',str(i),np.pad(resample_poly(a,2,1),(0,24000)),8000,1000,text)
for i in range(40):
 rec,a=example(138000000+i,difficulty=.45,return_audio=True);run('fresh_random',str(138000000+i),np.pad(resample_poly(a,2,1),(0,24000)),8000,1000,rec[3])
for i in range(20):
 _,a=negative(139000000+i,seconds=6.,return_audio=True);run('fresh_negative',str(139000000+i),resample_poly(a,2,1),8000,1000,'')
# Independently pulse-verified audio never used in training/development selection.
allclips=[json.loads(x) for x in (args.extra_pool/'prepared/segments-private.jsonl').read_text().splitlines()];eligible=[x for x in allclips if x['split']=='heldout' and not set(x['tokens'])-set(' ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/?.,')];clips=[eligible[i] for i in np.linspace(0,len(eligible)-1,20,dtype=int)];raw=np.frombuffer(subprocess.check_output(['ffmpeg','-v','error','-i',str(args.extra_pool/'arrl/260218_18WPM.mp3'),'-ac','1','-ar','8000','-f','f32le','-']),'<f4')
for s in clips:
 a=np.pad(raw[round(s['start_seconds']*8000):round(s['end_seconds']*8000)],(0,24000));run('new_arrl_heldout',s['id'],a,8000,750.,''.join(s['tokens']),s)
# Fixed 120-second continuous passage, bounded by complete words.
ali=json.loads((args.extra_pool/'aligned/260218_18WPM.alignment-private.json').read_text())['aligned'];byword={}
for x in ali:byword.setdefault(x['word'],[]).append(x)
words=[byword[k] for k in sorted(byword)];selected=[w for w in words if w[0]['start']>=300 and w[-1]['end']<420];ref=' '.join(''.join(x['symbol'] for x in w) for w in selected);assert set(ref)<=set(' ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/?.,')
start=selected[0][0]['start']-.1;end=selected[-1][-1]['end']+.1;a=np.pad(raw[round(start*8000):round(end*8000)],(0,24000));run('new_arrl_continuous','260218:300-420',a,8000,750.,ref,{'word_end_seconds':[w[-1]['end']-start for w in selected],'reference_words':ref.split()})
# These RF excerpts remain development/regression. Labels never train the models.
# Reuse local R9 annotation manifest as-is; score separately from the new holdout.
paths=[args.rf_manifest] if args.rf_manifest else []
if paths:
 mf=paths[0];manifest=json.loads(mf.read_text())
 for item in manifest['recordings']:
  from real_compare import read_wav
  rate,a=read_wav(mf.parent/item['wav']);a=np.pad(a[round(item['start_seconds']*rate):round(item['end_seconds']*rate)],(0,rate*3));run('known_rf_development',item['id'],a,rate,item['tone_hz'],item['reference'])
summary=summarize();(outdir/'FINAL_COMPARISON.json').write_text(json.dumps({'scope':'Frozen models/settings. New synthetic seeds and previously unused February-18 practice recording are held out. Existing VARC RF is development only. CER includes spaces. Accuracy probes run concurrently; their CPU timings must NOT be compared.','summary':summary,'records':rows},indent=2)+'\n');(outdir/'predictions-private.json').write_text(json.dumps(private,indent=2)+'\n');print(json.dumps(summary,indent=2))
