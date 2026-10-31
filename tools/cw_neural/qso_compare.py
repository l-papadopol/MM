#!/usr/bin/env python3
"""Independent full-message/QSO tests, including exact callsign and RST scores.
SPDX-License-Identifier: GPL-3.0-or-later
The fixed messages here are evaluation fixtures, not training targets.
"""
import argparse
import hashlib
import importlib.util
import json
from math import gcd
from pathlib import Path
import subprocess
import numpy as np
from scipy.signal import resample_poly
from train import example, distance
from refine import receiver_filter


def align_words(reference,prediction):
    a=reference.split();b=prediction.split()
    dp=np.zeros((len(a)+1,len(b)+1),dtype=int);dp[:,0]=np.arange(len(a)+1);dp[0]=np.arange(len(b)+1)
    for i,x in enumerate(a,1):
        for j,y in enumerate(b,1):dp[i,j]=min(dp[i-1,j]+1,dp[i,j-1]+1,dp[i-1,j-1]+(x!=y))
    pairs=[];i=len(a);j=len(b)
    while i or j:
        if i and j and dp[i,j]==dp[i-1,j-1]+(a[i-1]!=b[j-1]):pairs.append((a[i-1],b[j-1]));i-=1;j-=1
        elif i and dp[i,j]==dp[i-1,j]+1:pairs.append((a[i-1],None));i-=1
        else:j-=1
    return pairs[::-1]


def main():
    ap=argparse.ArgumentParser()
    for name in ['native','baseline','candidate','out']:ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--deepcw-root',type=Path)
    ap.add_argument('--reuse-baselines',type=Path,help='Reuse Native/DeepCW only; require identical live R7 output on every regenerated fixture')
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    prior=json.loads(args.reuse_baselines.read_text()) if args.reuse_baselines else None
    probes={'Native':args.native.resolve(),'R7':args.baseline.resolve(),'R8':args.candidate.resolve()}
    if args.deepcw_root:
        import onnxruntime as ort
        root=args.deepcw_root.resolve();spec=importlib.util.spec_from_file_location('official_deepcw',root/'examples/python/decode_morse.py')
        official=importlib.util.module_from_spec(spec);spec.loader.exec_module(official)
        metadata=official.load_metadata(root/'model.onnx.json')
        options=ort.SessionOptions();options.intra_op_num_threads=2;options.inter_op_num_threads=1
        session=ort.InferenceSession(str(root/'model.onnx'),sess_options=options,providers=['CPUExecutionProvider'])
    def run(audio,rate,cached=None):
        path=args.out/'input.f32';np.asarray(audio,dtype='<f4').tofile(path)
        result={k:(cached['decoded'][k] if cached is not None and k=='Native' else
                    subprocess.check_output([str(p),'audio',str(rate),'1000',str(path),'997'],text=True,
                          stderr=subprocess.DEVNULL,timeout=60).strip()) for k,p in probes.items()}
        if cached is not None and result['R7']!=cached['decoded']['R7']:
            raise RuntimeError('R7 replay changed; baseline reuse refused')
        if args.deepcw_root:
            if cached is not None:result['DeepCW']=cached['decoded']['DeepCW']
            elif len(audio)/rate>20:result['DeepCW']=None
            else:
                x=official.resample_linear(audio.astype(np.float32),rate,int(metadata['sample_rate']))
                x=np.pad(x,(0,max(0,5*int(metadata['sample_rate'])-len(x))))
                z=official.audio_to_spectrogram(x,metadata)
                logits=session.run([metadata['onnx_output_name']],{metadata['onnx_input_name']:z})[0]
                result['DeepCW']=official.greedy_ctc_decode(logits,list(metadata['chars']),int(metadata['blank_index'])).strip()
        return result
    fixtures=[('cq','CQ CQ CQ DE IZ6NNH IZ6NNH PSE K'),
              ('rst','UR 599 599 BK'),
              ('reply','IZ6NNH DE DL1ABC DL1ABC UR 599 599 PSE K'),
              ('name_qth','R R UR 559 559 NAME LUCIAN LUCIAN QTH ITALY ITALY BK'),
              ('close','DL1ABC DE IZ6NNH TNX QSO 73 73 SK'),
              ('short_rst','MM0ABC DE IZ6NNH R R UR 5NN 5NN BK')]
    calls={'IZ6NNH','DL1ABC','MM0ABC','F4XYZ','G4XYZ','EA3XYZ','I2XYZ'};reports={'599','559','579','5NN'}
    rows=[]
    def add(kind,identifier,text,audio,rate,extra):
        cached=prior['details'][len(rows)] if prior else None
        audio_sha=hashlib.sha256(np.asarray(audio,dtype='<f4').tobytes()).hexdigest()
        if cached is not None:
            if cached['reference']!=text or cached['id']!=identifier or cached['rate']!=rate or abs(cached['seconds']-len(audio)/rate)>1e-8:
                raise RuntimeError('Fixture specification changed; baseline reuse refused')
            if cached.get('audio_sha256',audio_sha)!=audio_sha:raise RuntimeError('Audio changed; baseline reuse refused')
        decoded=run(audio,rate,cached);detail={}
        for k,value in decoded.items():
            if value is None:detail[k]=None;continue
            aligned=align_words(text,value)
            detail[k]={'cer':distance(text,value)/len(text),'errors':distance(text,value),'exact':text==value,
                'calls_correct':sum(a in calls and a==b for a,b in aligned),'calls_total':sum(a in calls for a,b in aligned),
                'reports_correct':sum(a in reports and a==b for a,b in aligned),'reports_total':sum(a in reports for a,b in aligned)}
        row={'kind':kind,'id':identifier,'reference':text,'seconds':len(audio)/rate,'rate':rate,'audio_sha256':audio_sha,**extra,'decoded':decoded,'metrics':detail}
        rows.append(row);(args.out/'QSO_PARTIAL.json').write_text(json.dumps(rows,indent=2)+'\n')
        print('completed',len(rows),identifier,flush=True)
    for condition,width,difficulty in [('clean',0,0.),('receiver_300hz',300,.35),('receiver_100hz',100,.5)]:
        for speed in [12,20,28,36]:
            for index,(name,text) in enumerate(fixtures):
                seed=82900000+100000*width+100*speed+index
                _,audio=example(seed,difficulty=difficulty,text=text,wpm=speed,return_audio=True)
                if width:audio=receiver_filter(audio,width)
                rate=[8000,16000,44100,48000][index%4];factor=gcd(rate,4000)
                audio=resample_poly(audio,rate//factor,4000//factor)
                add('message',name,text,audio,rate,{'condition':condition,'filter_width_hz':width,'wpm':speed,'seed':seed})
    # The same process receives a whole exchange without decoder resets.
    # References include the ordinary word space implied by each long pause.
    for width in [0,300,100]:
        parts=[];texts=[]
        for i,(name,text) in enumerate([fixtures[0],fixtures[2],fixtures[3],fixtures[4]]):
            _,audio=example(92900000+width*100+i,difficulty=0. if width==0 else .35,text=text,wpm=[20,24,22,20][i],return_audio=True)
            if width:audio=receiver_filter(audio,width)
            parts.extend([audio,np.zeros(4000*3)]);texts.append(text)
        audio=resample_poly(np.concatenate(parts),2,1)
        add('continuous_qso','full_exchange',' '.join(texts),audio,8000,{'condition':str(width)+'hz','filter_width_hz':width,'inter_turn_silence_seconds':3})
    engines=list(rows[0]['decoded']);summary={}
    for group in ['clean','receiver_300hz','receiver_100hz','continuous_qso']:
        selected=[r for r in rows if (r['kind']=='message' and r.get('condition')==group) or (group=='continuous_qso' and r['kind']==group)]
        summary[group]={}
        for k in engines:
            supported=[r for r in selected if r['metrics'][k] is not None];characters=sum(len(r['reference']) for r in supported)
            m=[r['metrics'][k] for r in supported]
            summary[group][k]={'records':len(supported),'characters':characters,
                'cer':sum(x['errors'] for x in m)/characters if characters else None,'exact':sum(x['exact'] for x in m),
                **{field:sum(x[field] for x in m) for field in ['calls_correct','calls_total','reports_correct','reports_total']}}
    result={'scope':'Independent synthetic full-message and continuous-QSO evaluation. No fixture used to train/select R8. Callsign/RST exact scores use occurrence-preserving word alignment; merged or misspelled words count as incorrect.',
        'deepcw_scope':'Official frontend, zero-pad to 5 s; >20 s unsupported. Compare on matching supported subsets only.',
        'probe_sha256':{k:hashlib.sha256(p.read_bytes()).hexdigest() for k,p in probes.items()},'summary':summary,'details':rows}
    if args.deepcw_root:result['deepcw_model_sha256']=hashlib.sha256((root/'model.onnx').read_bytes()).hexdigest()
    if prior:result['reused_baseline_source_sha256']=hashlib.sha256(args.reuse_baselines.read_bytes()).hexdigest()
    (args.out/'QSO_COMPARISON.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(summary,indent=2))


if __name__=='__main__':main()
