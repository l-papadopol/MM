#!/usr/bin/env python3
"""Time-bounded acoustic refinement of MM's own CW model.
SPDX-License-Identifier: GPL-3.0-or-later
Fresh generated messages only; no upstream predictions or radio pseudo-labels.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import random
import time
import numpy as np
import torch
from torch import nn
from train import ALPHABET, Model, batch, decode, distance, example, export, features
from refine import augmented, clean, negative, receiver_filter


def long_record(seed):
    rng=np.random.default_rng(seed)
    text=''.join(rng.choice(list(ALPHABET[1:]),int(rng.integers(20,41))))
    text=' '.join(text[i:i+5] for i in range(0,len(text),5))
    record,audio=example(seed+100000000,difficulty=.3,text=text,return_audio=True)
    if rng.random()<.5:
        audio=receiver_filter(audio,float(rng.choice([100,150,200,300,500,800])))
    return (features(audio),record[1],record[2],record[3])


def validation_cache(model,groups):
    model.eval();cache={}
    with torch.no_grad():
        for name,records in groups.items():
            saved=[]
            for start in range(0,len(records),12):
                part=records[start:start+12];x,_,_,lengths,_=batch(part)
                z=model(x).numpy()
                for i,r in enumerate(part):
                    a=z[i,:lengths[i]];indices=a[:,2:].argmax(-1)+2
                    saved.append((a[:,0].copy(),a[:,1].copy(),a[np.arange(len(a)),indices],indices,r[3].strip()))
            cache[name]=saved
    return cache


def validate(model,groups):
    result=score_cache(validation_cache(model,groups),[-1.,0.,1.,2.,3.],[-.5,0.,.5,1.])
    model.train();return result


def score_cache(cache,spaces,blanks,limits=None,weights=None):
    best=None
    # Same causal CTC rules as the runtime. Constant calibration on development
    # data is evaluated with each checkpoint, rather than selecting by CE loss.
    for space in spaces:
        for blank in blanks:
            metrics={}
            for name,records in cache.items():
                errors=chars=exact=emitted=0
                for b,s,o,indices,reference in records:
                    choice=np.stack([b+blank,s+space,o],axis=1).argmax(-1)
                    prediction=' '.join(decode(np.where(choice<2,choice,indices)).split())
                    errors+=distance(reference,prediction);chars+=len(reference)
                    exact+=reference==prediction;emitted+=len(prediction)
                metrics[name]={'errors':errors,'characters':chars,'cer':errors/max(1,chars),
                               'exact':exact,'records':len(records),'emitted':emitted}
            weighting=weights or {'original':.25,'filtered':.4,'clean':.25,'long':.1}
            score=sum(weight*metrics[name]['cer'] for name,weight in weighting.items())+metrics['negative']['emitted']/600.
            result={'selection_score':score,'space_bias':space,'blank_bias':blank,'metrics':metrics}
            if limits is not None:
                if any(metrics[k]['cer']>limits[k]['cer']+.01 for k in ['original','clean']):continue
                if metrics['negative']['emitted']>limits['negative']['emitted']+2:continue
            if best is None or score<best['selection_score']:best=result
    return best


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--resume',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True);ap.add_argument('--seconds',type=float,default=900.)
    ap.add_argument('--seed',type=int,default=61017000);ap.add_argument('--batch-size',type=int,default=24)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    torch.set_num_threads(2);torch.manual_seed(args.seed);np.random.seed(args.seed);random.seed(args.seed)
    model=Model();model.load_state_dict(torch.load(args.resume,weights_only=True))
    records=[];ranges={}
    for name,count,base,make in [('filtered',4000,61017000,lambda s:augmented(s,filtered=True)),
                              ('unfiltered',1000,62017000,lambda s:augmented(s,filtered=False)),
                              ('clean',1500,63017000,clean),
                              ('ordinary',1000,64017000,lambda s:example(s,difficulty=.5)),
                              ('negative',1000,65017000,negative),
                              ('long',400,66017000,long_record)]:
        base+=args.seed-61017000
        records.extend(make(base+i) for i in range(count));ranges[name]=[base,base+count-1]
        print('generated',name,count,flush=True)
    groups={'original':[example(3900000+i,difficulty=.5) for i in range(100)],
            'filtered':[augmented(4900000+i,filtered=True) for i in range(100)],
            'clean':[clean(98900000+i) for i in range(100)],
            'negative':[negative(5900000+i,seconds=6.) for i in range(60)],
            'long':[long_record(63900000+i) for i in range(16)]}
    baseline=validate(model,groups);print('baseline',json.dumps(baseline),flush=True)
    metadata={'id':'mm-cw-tcn-ctc-v4-extended','license':'GPL-3.0-or-later','experimental':True,
        'origin':'Time-bounded refinement of independently trained MM R7 weights; generated labelled Morse and negative audio only.',
        'parent_checkpoint_sha256':hashlib.sha256(args.resume.read_bytes()).hexdigest(),
        'training_seed_ranges':ranges,'training_records':len(records),'torch_version':torch.__version__,
        'requested_seconds':args.seconds,'seed':args.seed,'batch_size':args.batch_size,
        'initial_learning_rate':.00012,'final_learning_rate':.000015,
        'loss_weights':{'frame_ce':.5,'ctc':2.5,'ce_blank':.1,'ce_space':1.5},
        'validation_seed_ranges':{'ordinary':[3900000,3900099],'filtered':[4900000,4900099],
            'clean':[98900000,98900099],'negative':[5900000,5900059],'long':[63900000,63900015]},
        'architecture':{'features':9,'channels':48,'kernel':5,'dilations':[1,4,16,64],'direction':'causal'},
        'baseline_validation':baseline,'limitations':['No verified real-radio training or CER.','Final test is not used for checkpoint selection.']}
    def save(result,step,elapsed):
        saved=Model();saved.load_state_dict(model.state_dict())
        with torch.no_grad():
            saved.head.bias[0]+=result['blank_bias'];saved.head.bias[1]+=result['space_bias']
        export(saved,args.out,{**metadata,'selected_step':step,'selected_elapsed_seconds':elapsed,'validation':result})
    save(baseline,0,0.);best=baseline['selection_score'];history=[]
    order=sorted(range(len(records)),key=lambda i:len(records[i][0]));buckets=[order[i:i+64] for i in range(0,len(order),64)]
    opt=torch.optim.AdamW(model.parameters(),lr=.00012,weight_decay=.0001)
    weights=torch.ones(len(ALPHABET)+1);weights[0]=.1;weights[1]=1.5
    ce=nn.CrossEntropyLoss(weight=weights,ignore_index=-100);ctc=nn.CTCLoss(blank=0,zero_infinity=True)
    checkpoint_dir=args.out/'checkpoints';checkpoint_dir.mkdir(exist_ok=True)
    started=time.monotonic();step=0
    while time.monotonic()-started<args.seconds:
        elapsed=time.monotonic()-started
        lr=.000015+(.00012-.000015)*.5*(1+math.cos(math.pi*min(1.,elapsed/args.seconds)))
        for group in opt.param_groups:group['lr']=lr
        bucket=buckets[np.random.randint(len(buckets))]
        part=[records[i] for i in np.random.choice(bucket,args.batch_size,replace=True)]
        x,y,target,lengths,target_lengths=batch(part)
        opt.zero_grad();logits=model(x)
        loss_ce=ce(logits.flatten(0,1),y.flatten())
        loss_ctc=ctc(logits.log_softmax(-1).transpose(0,1),target,lengths,target_lengths)
        loss=.5*loss_ce+2.5*loss_ctc
        if not torch.isfinite(loss):raise RuntimeError('Non-finite training loss')
        loss.backward();nn.utils.clip_grad_norm_(model.parameters(),5.);opt.step();step+=1
        if step%100==0:print('step',step,'CE',round(loss_ce.item(),4),'CTC',round(loss_ctc.item(),4),
                                'elapsed',round(time.monotonic()-started,1),'/',args.seconds,flush=True)
        if step%500==0:
            result=validate(model,groups);result.update({'step':step,'elapsed':time.monotonic()-started});history.append(result)
            # Protect the already useful clean/ordinary modes and blank input.
            eligible=all(result['metrics'][k]['cer']<=baseline['metrics'][k]['cer']+.01 for k in ['original','clean'])
            eligible=eligible and result['metrics']['negative']['emitted']<=baseline['metrics']['negative']['emitted']+2
            print('validation',json.dumps({**result,'eligible':eligible}),flush=True)
            if eligible and result['selection_score']<best:
                best=result['selection_score'];save(result,step,time.monotonic()-started)
                print('selected checkpoint',step,flush=True)
            torch.save(model.state_dict(),args.out/'latest.pt')
            torch.save(model.state_dict(),checkpoint_dir/f'step-{step:06d}.pt')
            (args.out/'TRAINING_HISTORY.json').write_text(json.dumps(history,indent=2)+'\n')
    result=validate(model,groups);result.update({'step':step,'elapsed':time.monotonic()-started});history.append(result)
    eligible=all(result['metrics'][k]['cer']<=baseline['metrics'][k]['cer']+.01 for k in ['original','clean'])
    eligible=eligible and result['metrics']['negative']['emitted']<=baseline['metrics']['negative']['emitted']+2
    if eligible and result['selection_score']<best:save(result,step,time.monotonic()-started)
    torch.save(model.state_dict(),checkpoint_dir/f'step-{step:06d}.pt')
    (args.out/'TRAINING_HISTORY.json').write_text(json.dumps(history,indent=2)+'\n')
    print('completed',json.dumps({'steps':step,'elapsed':time.monotonic()-started,'final_validation':result}),flush=True)


if __name__=='__main__':main()
