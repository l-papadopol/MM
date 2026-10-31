#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import sys,json,hashlib
import numpy as np,torch
sys.path.insert(0,str(Path(__file__).resolve().parent))
from train import Model,ALPHABET,decode,distance,example,export
from refine import negative
from qso_refine import qso_record
from adapt_audio import prepare_real,refront
from ctc_prefix import streaming_decode
import argparse
ap=argparse.ArgumentParser();ap.add_argument('--experiment-root',type=Path,required=True);ap.add_argument('--parent',type=Path,required=True);ap.add_argument('--pool',type=Path,required=True);args=ap.parse_args()
R=args.experiment_root;torch.set_num_threads(1);g=torch.load(R/'candidate/development-private.pt',weights_only=False)['groups'];parent=args.parent;model=Model();model.load_state_dict(torch.load(parent,weights_only=True))
# Same physical development audio through the released .1 envelope frontend.
_,arrl,_=prepare_real(args.pool,.1);baseline_groups={'arrl':arrl,'qso':[],'random':[],'negative':[]}
for i in range(32):
 r,a,_=qso_record(134000000+i,'dev',True);baseline_groups['qso'].append(refront(r,a,floor=.1))
for i in range(40):
 r,a=example(135000000+i,difficulty=.45,return_audio=True);baseline_groups['random'].append(refront(r,a,floor=.1))
for i in range(20):
 r,a=negative(136000000+i,seconds=6.,return_audio=True);baseline_groups['negative'].append(refront(r,a,floor=.1))
def cache(groups):
 with torch.inference_mode():return {name:[(model(torch.from_numpy(r[0])[None])[0].numpy(),r[3]) for r in rs] for name,rs in groups.items()}
def score(cached,search,space=0.):
 metrics={}
 for group,rs in cached.items():
  errors=chars=exact=0
  for z,ref in rs:
   z=z.copy();z[:,1]+=space
   if search:
    ids,_=streaming_decode(z,blank_threshold=.99,blank_frames=4);pred=''.join(ALPHABET[i-1] for i in ids)
   else:pred=decode(z.argmax(-1))
   pred=' '.join(pred.split());errors+=distance(ref,pred);chars+=len(ref);exact+=pred==ref
  metrics[group]={'errors':errors,'characters':chars,'exact':exact,'records':len(rs)}
 return metrics
baseline=score(cache(baseline_groups),False);print('released_baseline',json.dumps(baseline),flush=True)
paths=[('R8',parent,[0.])]
for directory,steps,biases in [('candidate',[4000,8000,12000],[2.,3.]),('frozen-stack',[3600],[0.,1.])]:
 files=sorted((R/directory/'checkpoints').glob('step-*.pt'));latest=max(int(p.stem.split('-')[-1]) for p in files)
 for step in sorted(set(steps+[latest])):
  p=R/directory/'checkpoints'/f'step-{step:06d}.pt'
  if p.exists():paths.append((directory+'-'+str(step),p,biases))
rows=[]
for name,path,biases in paths:
 model.load_state_dict(torch.load(path,weights_only=True));cached=cache(g)
 for bias in biases:
  metrics=score(cached,True,bias);eligible=metrics['negative']['errors']==0 and all(metrics[k]['errors']<=baseline[k]['errors']+2 for k in ['arrl','qso','random']);value=sum(w*metrics[k]['errors']/metrics[k]['characters'] for k,w in [('arrl',.45),('qso',.4),('random',.15)])
  row={'name':name,'checkpoint':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'space_bias':bias,'eligible':eligible,'score':value,'metrics':metrics};rows.append(row);print(json.dumps(row),flush=True)
chosen=min((r for r in rows if r['eligible']),key=lambda r:r['score']);model.load_state_dict(torch.load(chosen['checkpoint'],weights_only=True))
with torch.no_grad():model.head.bias[1]+=chosen['space_bias']
metadata={'id':'mm-cw-r13-search','license':'GPL-3.0-or-later','experimental':True,'parent':'mm-cw-tcn-ctc-v4-extended','selection':chosen,'released_envelope_baseline':baseline,'protocol_sha256':hashlib.sha256((R/'PROTOCOL.json').read_bytes()).hexdigest(),'final_test_used_for_selection':False,'frontend':{'floor':.2,'peak_release_seconds':.5},'decoding':{'beam_width':4,'token_penalty':1.5,'blank_threshold':.99,'blank_frames':4},'architecture':{'features':9,'channels':48,'kernel':5,'dilations':[1,4,16,64],'causal':True},'runtime_requires_external_ml':False}
export(model,R/'selected',metadata);(R/'selected/SEARCH_SELECTION.json').write_text(json.dumps({'released_baseline':baseline,'candidates':rows,'chosen':chosen},indent=2)+'\n');print('SELECTED',chosen['name'],chosen['space_bias'],flush=True)
