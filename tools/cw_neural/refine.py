#!/usr/bin/env python3
"""Receiver-filter augmentation and blank/CTC refinement of MM's own model.
SPDX-License-Identifier: GPL-3.0-or-later
No unverified WebSDR predictions are used as training labels.
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import time

import numpy as np
from scipy.signal import butter, sosfiltfilt
import torch
from torch import nn
from train import ALPHABET, RATE, Model, batch, decode, evaluate, example, export, features


def receiver_filter(audio, width, center=1000., order=4):
    low=max(60., center-width/2)
    high=min(RATE/2-60., center+width/2)
    return sosfiltfilt(butter(order,[low,high],fs=RATE,btype='bandpass',output='sos'),audio).copy()


def augmented(seed, filtered=None, return_audio=False, return_metadata=False):
    rng=np.random.default_rng(seed)
    size=int(rng.integers(5,14))
    chars=list(ALPHABET[1:])
    # Many independent word boundaries, without a callsign/dictionary prior.
    text=''.join(rng.choice(chars,size=size))
    positions=sorted(rng.choice(np.arange(1,size),size=int(rng.integers(0,min(4,size-1))),replace=False),reverse=True)
    for pos in positions:text=text[:pos]+' '+text[pos:]
    difficulty=float(rng.uniform(.15,.85))
    record,audio=example(seed+100000000,difficulty=difficulty,text=text,return_audio=True)
    if filtered is None: filtered=rng.random()<.55
    width=None
    if filtered:
        width=float(rng.choice([100,150,200,300,500,800,1200]))
        audio=receiver_filter(audio,width,1000+rng.uniform(-12,12),int(rng.choice([3,4,5])))
    # Overall gain changes do not change labels or the normalized frontend.
    audio*=float(10**rng.uniform(-1,.2))
    record=(features(audio),record[1],record[2],record[3])
    if return_metadata:return record,audio,{'filter_width_hz':width}
    return (record,audio) if return_audio else record


def negative(seed, seconds=4., width=None, return_audio=False):
    rng=np.random.default_rng(seed)
    n=int(seconds*RATE);t=np.arange(n)/RATE
    audio=rng.normal(0,rng.uniform(.02,.3),n)
    if width is None:width=float(rng.choice([0,70,100,150,200,300,500,800,1200]))
    if width:audio=receiver_filter(audio,width,1000+rng.uniform(-20,20))
    if rng.random()<.3:
        # An unkeyed carrier is a negative, including a gently fading one.
        level=rng.uniform(.05,.5)
        audio+=level*(.85+.15*np.sin(2*np.pi*rng.uniform(.1,.6)*t))*np.sin(2*np.pi*(1000+rng.uniform(-20,20))*t)
    x=features(audio)
    record=(x,np.zeros(len(x),dtype=np.int64),np.array([],dtype=np.int64),'')
    return (record,audio) if return_audio else record


def clean(seed, return_audio=False):
    rng=np.random.default_rng(seed)
    record,audio=example(seed,difficulty=0. if seed%2 else .08,wpm=20 if seed%3==0 else None,return_audio=True)
    before=int(rng.integers(0,100));after=int(rng.integers(0,100))
    audio=np.pad(audio,(before*32,after*32))
    result=(features(audio),np.pad(record[1],(before,after)),record[2],record[3])
    return (result,audio) if return_audio else result


def validate(model, original, filtered, negatives, clean_records=None):
    a=evaluate(model,original);b=evaluate(model,filtered)
    model.eval();emitted=0;nonempty=0
    with torch.no_grad():
        for start in range(0,len(negatives),16):
            part=negatives[start:start+16];x,_,_,lengths,_=batch(part)
            prediction=model(x).argmax(-1)
            for i,item in enumerate(part):
                text=decode(prediction[i,:lengths[i]].tolist()).strip()
                emitted+=len(text);nonempty+=bool(text)
    model.train()
    # Equal emphasis on ordinary and receiver-filtered messages; penalize
    # false output separately rather than hiding it inside transcript CER.
    clean_result=evaluate(model,clean_records) if clean_records else None
    score=(a['cer']+b['cer'])/2 if clean_result is None else .3*a['cer']+.3*b['cer']+.4*clean_result['cer']
    return {'original':a,'receiver_filtered':b,'clean':clean_result,'negative_characters':emitted,
            'negative_nonempty_records':nonempty,'negative_records':len(negatives),
            'selection_score':score+emitted/(len(negatives)*20)}


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--resume',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--steps',type=int,default=3600)
    ap.add_argument('--positive-records',type=int,default=8000)
    ap.add_argument('--negative-records',type=int,default=1600)
    ap.add_argument('--clean-records',type=int,default=0)
    ap.add_argument('--batch-size',type=int,default=24)
    ap.add_argument('--learning-rate',type=float,default=.0004)
    ap.add_argument('--ctc-weight',type=float,default=2.)
    ap.add_argument('--seed',type=int,default=27102026)
    args=ap.parse_args()
    torch.set_num_threads(2);torch.manual_seed(args.seed)
    np.random.seed(args.seed);random.seed(args.seed)
    model=Model();model.load_state_dict(torch.load(args.resume,weights_only=True))
    opt=torch.optim.AdamW(model.parameters(),lr=args.learning_rate,weight_decay=.0001)
    weights=torch.ones(len(ALPHABET)+1);weights[0]=.1;weights[1]=1.5
    ce=nn.CrossEntropyLoss(weight=weights,ignore_index=-100)
    ctc=nn.CTCLoss(blank=0,zero_infinity=True)
    train=[];print('generating receiver-filtered training data',flush=True)
    for i in range(args.positive_records):
        train.append(augmented(args.seed+i))
        if (i+1)%1000==0:print('generated',i+1,flush=True)
    train.extend(negative(args.seed+100000+i) for i in range(args.negative_records))
    train.extend(clean(args.seed+200000+i) for i in range(args.clean_records))
    if args.clean_records:
        for _ in range(40):
            x=features(np.zeros(RATE*4));train.append((x,np.zeros(len(x),dtype=np.int64),np.array([],dtype=np.int64),''))
    # These model-selection sets are distinct from R5's final test and the
    # fresh final R6 test. Their seed ranges are recorded in the model card.
    original=[example(3900000+i,difficulty=.5) for i in range(100)]
    filtered=[augmented(4900000+i,filtered=True) for i in range(100)]
    negatives=[negative(5900000+i,seconds=6.) for i in range(60)]
    clean_validation=[clean(98900000+i) for i in range(100)] if args.clean_records else None
    baseline=validate(model,original,filtered,negatives,clean_validation)
    print('baseline',json.dumps(baseline),flush=True)
    best=baseline['selection_score'];history=[];started=time.time()
    # Always write a recoverable original checkpoint before the first update.
    metadata={'id':'mm-cw-tcn-ctc-v2','license':'GPL-3.0-or-later','experimental':True,
        'origin':'Refinement of independently trained MM R5 weights; generated Morse and controlled negative audio only.',
        'resume_checkpoint_sha256':hashlib.sha256(args.resume.read_bytes()).hexdigest(),
        'seed':args.seed,'torch_version':torch.__version__,
        'training_records':len(train),'positive_records':args.positive_records,'negative_records':args.negative_records,'clean_records':args.clean_records,
        'training_wpm':[12,36],'learning_rate':args.learning_rate,'ctc_weight':args.ctc_weight,
        'ce_blank_weight':.1,'ce_space_weight':1.5,
        'receiver_filter_widths_hz':[100,150,200,300,500,800,1200],
        'validation_seed_ranges':{'original':[3900000,3900099],'filtered':[4900000,4900099],'negative':[5900000,5900059]},
        'architecture':{'features':9,'window_ms':32,'hop_ms':8,'tcn_channels':48,'dilations':[1,4,16,64],
                        'kernel_size':5,'direction':'causal','classes':len(ALPHABET)+1},
        'baseline_validation':baseline,
        'limitations':['No verified real-radio CW transcripts used for training.','No superiority over Native or DeepCW claimed.']}
    export(model,args.out,{**metadata,'validation':baseline,'selected_step':0,'history':[]})
    for step in range(1,args.steps+1):
        indices=np.random.randint(0,len(train),args.batch_size)
        x,y,target,lengths,target_lengths=batch([train[i] for i in indices])
        opt.zero_grad();logits=model(x)
        loss_ce=ce(logits.flatten(0,1),y.flatten())
        loss_ctc=ctc(logits.log_softmax(-1).transpose(0,1),target,lengths,target_lengths)
        loss=loss_ce+args.ctc_weight*loss_ctc
        loss.backward();nn.utils.clip_grad_norm_(model.parameters(),5.);opt.step()
        if step%100==0:print(f'step {step}/{args.steps} CE={loss_ce.item():.4f} CTC={loss_ctc.item():.4f} seconds={time.time()-started:.1f}',flush=True)
        if step%400==0 or step==args.steps:
            result=validate(model,original,filtered,negatives,clean_validation);result['step']=step;history.append(result)
            print('validation',json.dumps(result),flush=True)
            if result['selection_score']<best:
                best=result['selection_score']
                export(model,args.out,{**metadata,'validation':result,'selected_step':step,'history':history})
    (args.out/'TRAINING_HISTORY.json').write_text(json.dumps(history,indent=2)+'\n')


if __name__=='__main__':main()
