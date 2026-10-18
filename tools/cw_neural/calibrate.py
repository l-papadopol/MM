#!/usr/bin/env python3
"""Calibrate CTC space/blank biases on model-selection sets only.
SPDX-License-Identifier: GPL-3.0-or-later
No dictionary, upstream predictions, or final test labels are used.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
import torch
from train import Model, batch, decode, distance, example, export
from refine import augmented, negative, clean


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--resume',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    torch.set_num_threads(2)
    model=Model();model.load_state_dict(torch.load(args.resume,weights_only=True));model.eval()
    groups={
        'original':[example(3900000+i,difficulty=.5) for i in range(100)],
        'receiver_filtered':[augmented(4900000+i,filtered=True) for i in range(100)],
        'clean':[clean(98900000+i) for i in range(100)],
        'negative':[negative(5900000+i,seconds=6.) for i in range(60)]}
    cached={}
    with torch.no_grad():
        for name,records in groups.items():
            cache=[]
            for start in range(0,len(records),16):
                part=records[start:start+16];x,_,_,lengths,_=batch(part)
                logits=model(x).numpy()
                for i,r in enumerate(part):
                    z=logits[i,:lengths[i]];others=z[:,2:].argmax(-1)+2
                    cache.append((z[:,0],z[:,1],z[np.arange(len(z)),others],others,r[3].strip()))
            cached[name]=cache
            print('cached',name,flush=True)
    def evaluate(space,blank):
        result={}
        for name,cache in cached.items():
            errors=chars=exact=emitted=0
            for b,s,o,indices,reference in cache:
                choices=np.stack([b+blank,s+space,o],axis=1).argmax(-1)
                path=np.where(choices<2,choices,indices)
                # The C++ streaming callback suppresses repeated/leading spaces.
                prediction=' '.join(decode(path).split())
                errors+=distance(reference,prediction);chars+=len(reference)
                exact+=reference==prediction;emitted+=len(prediction)
            result[name]={'errors':errors,'characters':chars,'cer':errors/max(1,chars),
                          'exact':exact,'emitted':emitted}
        score=(.3*result['original']['cer']+.3*result['receiver_filtered']['cer']+
               .4*result['clean']['cer']+result['negative']['emitted']/1200)
        return score,result
    history=[];best=(float('inf'),None,None,None)
    for space in np.arange(-1.,6.01,.5):
        for blank in np.arange(-1.,3.01,.5):
            score,result=evaluate(float(space),float(blank))
            history.append({'space':float(space),'blank':float(blank),'score':score,'metrics':result})
            if score<best[0]:
                best=(score,float(space),float(blank),result)
                print('best',json.dumps({'space':best[1],'blank':best[2],'score':score,'metrics':result}),flush=True)
    with torch.no_grad():
        model.head.bias[0]+=best[2];model.head.bias[1]+=best[1]
    metadata=json.loads((args.resume.parent/'MODEL_CARD.json').read_text())
    metadata.update({'id':'mm-cw-tcn-ctc-v3-calibrated',
        'calibration':{'parent_checkpoint_sha256':hashlib.sha256(args.resume.read_bytes()).hexdigest(),
                       'space_bias':best[1],'blank_bias':best[2],'validation':best[3],
                       'selection_score':best[0],'grid':history,
                       'uses_final_test':False,'method':'Constant output biases, baked into own model head.'}})
    export(model,args.out,metadata)
    print('selected',best[:3],flush=True)


if __name__=='__main__':main()
