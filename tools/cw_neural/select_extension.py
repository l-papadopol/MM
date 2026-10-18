#!/usr/bin/env python3
"""Fine development-only calibration/selection of saved extension checkpoints.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
import torch
from train import Model, example, export
from refine import augmented, clean, negative
from extend import long_record, validation_cache, score_cache


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--checkpoints',type=Path,required=True)
    ap.add_argument('--run',type=Path,required=True);ap.add_argument('--parent',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True);args=ap.parse_args()
    args.out.mkdir(parents=True,exist_ok=True);torch.set_num_threads(2)
    groups={'original':[example(3900000+i,difficulty=.5) for i in range(100)],
            'filtered':[augmented(4900000+i,filtered=True) for i in range(100)],
            'clean':[clean(98900000+i) for i in range(100)],
            'negative':[negative(5900000+i,seconds=6.) for i in range(60)],
            'long':[long_record(63900000+i) for i in range(16)]}
    model=Model();model.load_state_dict(torch.load(args.parent,weights_only=True))
    baseline=score_cache(validation_cache(model,groups),[0.],[0.])
    metadata=json.loads((args.run/'MODEL_CARD.json').read_text())
    history=json.loads((args.run/'TRAINING_HISTORY.json').read_text())
    last=history[-1];limits=baseline['metrics'];best=baseline['selection_score'];chosen=0;results=[]
    grid_space=np.arange(-1.,6.01,.5).tolist();grid_blank=np.arange(-1.,3.01,.5).tolist()
    def save(state,result,step,source):
        selected=Model();selected.load_state_dict(state)
        with torch.no_grad():
            selected.head.bias[0]+=result['blank_bias'];selected.head.bias[1]+=result['space_bias']
        elapsed=next((r['elapsed'] for r in history if r['step']==step),0.)
        export(selected,args.out,{**metadata,'selected_step':step,'selected_elapsed_seconds':elapsed,
            'completed_steps':last['step'],'completed_elapsed_seconds':last['elapsed'],
            'selected_raw_checkpoint_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
            'validation':result,'fine_calibration':{'space_grid':grid_space,'blank_grid':grid_blank,
                'limits':limits,'uses_final_test':False,'selection_history':results}})
        torch.save(state,args.out/'raw-selected.pt')
    save(model.state_dict(),baseline,0,args.parent)
    for path in sorted(args.checkpoints.glob('step-*.pt')):
        step=int(path.stem.split('-')[-1]);state=torch.load(path,weights_only=True);model.load_state_dict(state)
        result=score_cache(validation_cache(model,groups),grid_space,grid_blank,limits=limits)
        results.append({'step':step,'validation':result})
        print('checkpoint',step,json.dumps(result),flush=True)
        if result is not None and result['selection_score']<best:
            best=result['selection_score'];chosen=step;save(state,result,step,path)
            print('selected',chosen,flush=True)
    # Refresh complete selection history even if the best checkpoint was early.
    card=json.loads((args.out/'MODEL_CARD.json').read_text())
    card['fine_calibration']['selection_history']=results
    card['fine_calibration']['stored_checkpoint_steps']=[r['step'] for r in results]
    (args.out/'MODEL_CARD.json').write_text(json.dumps(card,indent=2)+'\n')
    print('final selected',chosen,'score',best,flush=True)


if __name__=='__main__':main()
