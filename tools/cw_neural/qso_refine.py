#!/usr/bin/env python3
"""Time-bounded acoustic QSO adaptation with withheld callsigns/fixtures.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse,hashlib,json,math,time
from pathlib import Path
import numpy as np
import torch
from torch import nn
from train import Model, ALPHABET, batch, example, features, export
from refine import augmented,clean,negative,receiver_filter
from extend import validation_cache,score_cache,long_record

RESERVED={'CQ CQ CQ DE IZ6NNH IZ6NNH PSE K','UR 599 599 BK',
 'IZ6NNH DE DL1ABC DL1ABC UR 599 599 PSE K',
 'R R UR 559 559 NAME LUCIAN LUCIAN QTH ITALY ITALY BK',
 'DL1ABC DE IZ6NNH TNX QSO 73 73 SK','MM0ABC DE IZ6NNH R R UR 5NN 5NN BK'}

def qso_record(seed,mode='train',return_audio=False,text_only=False):
    rng=np.random.default_rng(seed)
    def call():
        while True:
            value=str(rng.choice(['I','IK','IZ','DL','G','F','EA','W','K','JA','VK']))+str(rng.integers(10))+''.join(rng.choice(list('ABCDEFGHIJKLMNOPQRSTUVWXYZ'),int(rng.integers(2,4))))
            if value not in {'IZ6NNH','DL1ABC','MM0ABC'}:return value
    names={'train':['MARCO','PAOLO','LUCA','HANS','PETER','JOHN','DAVID'],
           'dev':['MARIO','ANNA','PAUL','TONY','ROBERT'],
           'test':['ANDREW','GEORGE','GIANNI','PABLO']}[mode]
    cities={'train':['ROME','BERLIN','LONDON','PARIS','MADRID'],
            'dev':['TORINO','BONN','LEEDS','LILLE','OVIEDO'],
            'test':['MILANO','OSLO','PORTO','DELHI']}[mode]
    while True:
        a,b=call(),call();name=str(rng.choice(names));city=str(rng.choice(cities));rst=str(rng.choice(['599','559','579','589','5NN']))
        text=str(rng.choice([f'CQ CQ CQ DE {a} {a} PSE K',f'{a} DE {b} UR {rst} {rst} PSE K',
            f'R R UR {rst} {rst} BK',f'NAME {name} {name} QTH {city} {city} BK',
            f'{a} DE {b} TNX QSO 73 73 SK',f'R R FB NAME {name} QTH {city} HW CPY PSE K']))
        if text not in RESERVED:break
    if text_only:return text,{'calls':[a,b]}
    wpm=float(rng.choice([12,16,20,24,28,32,36]));difficulty=float(rng.uniform(0,.5))
    record,audio=example(seed+100000000,difficulty=difficulty,text=text,wpm=wpm,return_audio=True)
    width=int(rng.choice([0,0,100,150,200,300,500,800,1200]))
    if width:audio=receiver_filter(audio,width)
    record=(features(audio),record[1],record[2],record[3])
    return (record,audio,{'calls':[a,b],'report':rst,'wpm':wpm,'width_hz':width}) if return_audio else record

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--resume',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--seconds',type=float,default=450.);args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    torch.set_num_threads(2);torch.manual_seed(95117000);np.random.seed(95117000)
    model=Model();model.load_state_dict(torch.load(args.resume,weights_only=True))
    records=[]
    for name,count,base,make in [('qso',1200,95117000,qso_record),('ordinary',400,96117000,lambda s:example(s,difficulty=.5)),
          ('filtered',500,97117000,lambda s:augmented(s,filtered=True)),('clean',400,98117000,clean),('negative',400,99117000,negative)]:
        records.extend(make(base+i) for i in range(count));print('generated',name,count,flush=True)
    groups={'original':[example(3900000+i,difficulty=.5) for i in range(100)],'filtered':[augmented(4900000+i,filtered=True) for i in range(100)],
       'clean':[clean(98900000+i) for i in range(100)],'negative':[negative(5900000+i,seconds=6.) for i in range(60)],
       'long':[long_record(63900000+i) for i in range(16)],'qso':[qso_record(101117000+i,'dev') for i in range(100)]}
    weighting={'qso':.55,'filtered':.2,'original':.075,'clean':.1,'long':.075}
    spaces=[-1.,0.,.5,1.,1.5,2.,2.5,3.,3.5,4.];blanks=[-.5,0.,.5,1.]
    def check(limits=None):
        result=score_cache(validation_cache(model,groups),spaces,blanks,limits,weighting)
        model.train();return result
    baseline=score_cache(validation_cache(model,groups),[0.],[0.],weights=weighting);limits=baseline['metrics']
    best=baseline['selection_score'];history=[]
    metadata={'id':'mm-cw-tcn-ctc-v4-qso','license':'GPL-3.0-or-later','experimental':True,
      'origin':'Original MM weights adapted to generated variable-call QSO audio. No upstream predictions or unverified radio labels.',
      'parent_checkpoint_sha256':hashlib.sha256(args.resume.read_bytes()).hexdigest(),'requested_seconds':args.seconds,
      'baseline_validation':baseline,'training_records':len(records),'qso_training_seed_range':[95117000,95118199],
      'qso_development_seed_range':[101117000,101117099],'reserved_calls':['IZ6NNH','DL1ABC','MM0ABC'],
      'reserved_fixtures':sorted(RESERVED),'loss_weights':{'ce':.65,'ctc':2.,'ce_space':3.,'ce_blank':.1},
      'selection_weighting':weighting,'torch_version':torch.__version__,
      'phase_one_training_seconds':json.loads((args.resume.parent/'MODEL_CARD.json').read_text()).get('completed_elapsed_seconds'),
      'other_training_ranges':{'ordinary':[96117000,96117399],'filtered':[97117000,97117499],
          'clean':[98117000,98117399],'negative':[99117000,99117399]}}
    def save(result,step,elapsed):
        chosen=Model();chosen.load_state_dict(model.state_dict())
        with torch.no_grad():chosen.head.bias[0]+=result['blank_bias'];chosen.head.bias[1]+=result['space_bias']
        export(chosen,args.out,{**metadata,'selected_step':step,'selected_elapsed_seconds':elapsed,'validation':result})
    save(baseline,0,0.)
    order=sorted(range(len(records)),key=lambda i:len(records[i][0]));buckets=[order[i:i+48] for i in range(0,len(order),48)]
    opt=torch.optim.AdamW(model.parameters(),lr=.00008,weight_decay=.0001)
    weights=torch.ones(len(ALPHABET)+1);weights[0]=.1;weights[1]=3.
    ce=nn.CrossEntropyLoss(weight=weights,ignore_index=-100);ctc=nn.CTCLoss(blank=0,zero_infinity=True)
    checkpoint_dir=args.out/'checkpoints';checkpoint_dir.mkdir(exist_ok=True)
    started=time.monotonic();step=0
    while time.monotonic()-started<args.seconds:
        elapsed=time.monotonic()-started;lr=.000008+.000072*.5*(1+math.cos(math.pi*min(1.,elapsed/args.seconds)))
        for g in opt.param_groups:g['lr']=lr
        part=[records[i] for i in np.random.choice(buckets[np.random.randint(len(buckets))],24)]
        x,y,target,lengths,target_lengths=batch(part);opt.zero_grad();logits=model(x)
        a=ce(logits.flatten(0,1),y.flatten());b=ctc(logits.log_softmax(-1).transpose(0,1),target,lengths,target_lengths)
        loss=.65*a+2.*b
        if not torch.isfinite(loss):raise RuntimeError('Non-finite loss')
        loss.backward();nn.utils.clip_grad_norm_(model.parameters(),5.);opt.step();step+=1
        if step%100==0:print('step',step,'elapsed',round(time.monotonic()-started,1),'CE',round(a.item(),4),'CTC',round(b.item(),4),flush=True)
        if step%250==0:
            result=check(limits);history.append({'step':step,'elapsed':time.monotonic()-started,'validation':result})
            print('validation',step,json.dumps(result),flush=True)
            if result is not None and result['selection_score']<best:
                best=result['selection_score'];save(result,step,time.monotonic()-started);print('selected',step,flush=True)
            torch.save(model.state_dict(),args.out/'latest.pt')
            torch.save(model.state_dict(),checkpoint_dir/f'step-{step:06d}.pt')
            (args.out/'TRAINING_HISTORY.json').write_text(json.dumps(history,indent=2)+'\n')
    result=check(limits);history.append({'step':step,'elapsed':time.monotonic()-started,'validation':result})
    if result is not None and result['selection_score']<best:save(result,step,time.monotonic()-started)
    torch.save(model.state_dict(),checkpoint_dir/f'step-{step:06d}.pt')
    (args.out/'TRAINING_HISTORY.json').write_text(json.dumps(history,indent=2)+'\n')
    card=json.loads((args.out/'MODEL_CARD.json').read_text());card.update({'completed_steps':step,'completed_elapsed_seconds':time.monotonic()-started})
    (args.out/'MODEL_CARD.json').write_text(json.dumps(card,indent=2)+'\n')
    print('completed',step,'elapsed',time.monotonic()-started,flush=True)

if __name__=='__main__':main()
