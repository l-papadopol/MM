#!/usr/bin/env python3
"""Development-only selection for the QSO refinement phase.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse,hashlib,json,time
from pathlib import Path
import numpy as np
import torch
from train import Model,example,export
from refine import augmented,clean,negative
from extend import long_record,validation_cache,score_cache
from qso_refine import qso_record

def main():
    ap=argparse.ArgumentParser()
    for n in ['checkpoints','run','parent','out']:ap.add_argument('--'+n,type=Path,required=True)
    ap.add_argument('--watch-log',type=Path);ap.add_argument('--explain-rejection',action='store_true')
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    torch.set_num_threads(2)
    groups={'original':[example(3900000+i,difficulty=.5) for i in range(100)],'filtered':[augmented(4900000+i,filtered=True) for i in range(100)],
       'clean':[clean(98900000+i) for i in range(100)],'negative':[negative(5900000+i,seconds=6.) for i in range(60)],
       'long':[long_record(63900000+i) for i in range(16)],'qso':[qso_record(101117000+i,'dev') for i in range(100)]}
    weighting={'qso':.55,'filtered':.2,'original':.075,'clean':.1,'long':.075}
    model=Model();model.load_state_dict(torch.load(args.parent,weights_only=True))
    baseline=score_cache(validation_cache(model,groups),[0.],[0.],weights=weighting)
    limits=json.loads(json.dumps(baseline['metrics']))
    # Respect the user's 5% target on clean/ordinary development inputs while
    # improving QSO recognition; report all regressions on the frozen test.
    for name in ['original','clean']:limits[name]['cer']=max(limits[name]['cer'],.04)
    metadata=json.loads((args.run/'MODEL_CARD.json').read_text());best=baseline['selection_score'];results=[];seen=set();chosen=0
    spaces=np.arange(-6.,6.01,.5).tolist();blanks=np.arange(-1.,3.01,.5).tolist()
    def save(state,result,step):
        selected=Model();selected.load_state_dict(state)
        with torch.no_grad():selected.head.bias[0]+=result['blank_bias'];selected.head.bias[1]+=result['space_bias']
        export(selected,args.out,{**metadata,'selected_step':step,'validation':result,
          'selected_raw_checkpoint_sha256':hashlib.sha256(state_source.read_bytes()).hexdigest(),
          'fine_calibration':{'space_grid':spaces,'blank_grid':blanks,'ordinary_clean_development_ceiling':.05,
            'noise_ceiling':limits['negative']['emitted']+2,'uses_final_test':False,'selection_history':results}})
        torch.save(state,args.out/'raw-selected.pt')
    state_source=args.parent;save(model.state_dict(),baseline,0)
    started=time.monotonic()
    while True:
        for p in sorted(args.checkpoints.glob('step-*.pt')):
            if p.name in seen:continue
            try:state=torch.load(p,weights_only=True)
            except (EOFError,RuntimeError):continue
            seen.add(p.name);step=int(p.stem.split('-')[-1]);model.load_state_dict(state)
            cache=validation_cache(model,groups)
            result=score_cache(cache,spaces,blanks,limits=limits,weights=weighting)
            if result is None and args.explain_rejection:
                print('unconstrained diagnostic',step,json.dumps(score_cache(cache,spaces,blanks,weights=weighting)),flush=True)
            results.append({'step':step,'validation':result});print('checkpoint',step,json.dumps(result),flush=True)
            if result is not None and result['selection_score']<best and result['metrics']['qso']['cer']<baseline['metrics']['qso']['cer']:
                best=result['selection_score'];chosen=step;state_source=p;save(state,result,step);print('selected',chosen,flush=True)
        if args.watch_log is None:break
        if 'completed ' in args.watch_log.read_text():
            time.sleep(1.)
            if all(p.name in seen for p in args.checkpoints.glob('step-*.pt')):break
        if time.monotonic()-started>900:raise RuntimeError('Checkpoint watch deadline exceeded')
        time.sleep(1.)
    card=json.loads((args.out/'MODEL_CARD.json').read_text());run=json.loads((args.run/'MODEL_CARD.json').read_text())
    history=json.loads((args.run/'TRAINING_HISTORY.json').read_text())
    card['fine_calibration']['selection_history']=results;card['fine_calibration']['stored_steps']=[r['step'] for r in results]
    card['completed_steps']=run.get('completed_steps',history[-1]['step']);card['completed_elapsed_seconds']=run.get('completed_elapsed_seconds',history[-1]['elapsed'])
    card['selected_elapsed_seconds']=next((r['elapsed'] for r in history if r['step']==chosen),0.)
    (args.out/'MODEL_CARD.json').write_text(json.dumps(card,indent=2)+'\n')
    print('final selected',chosen,'score',best,flush=True)

if __name__=='__main__':main()
