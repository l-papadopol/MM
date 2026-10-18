#!/usr/bin/env python3
"""Frozen-model comparison with fresh seeds and receiver-filtered negatives.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import wave

import numpy as np
from scipy.signal import resample_poly
from train import example, distance
from refine import augmented, negative, clean


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--baseline',type=Path,required=True)
    ap.add_argument('--baseline-label',default='R5')
    ap.add_argument('--candidate',type=Path,required=True)
    ap.add_argument('--candidate-label',default='candidate')
    ap.add_argument('--native',type=Path)
    ap.add_argument('--previous',type=Path,help='Optional frozen R6 probe')
    ap.add_argument('--deepcw-root',type=Path,help='External official checkout; not bundled or used for training')
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--records',type=int,default=120,help='Number in EACH positive group')
    ap.add_argument('--websdr',type=Path,action='append',default=[])
    ap.add_argument('--seed',type=int,default=6900000)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    probes={args.baseline_label:args.baseline.resolve(),args.candidate_label:args.candidate.resolve()}
    if args.previous:probes['R6']=args.previous.resolve()
    if args.native:probes['Native']=args.native.resolve()
    engines=list(probes)
    if args.deepcw_root:
        import importlib.util
        import onnxruntime as ort
        root=args.deepcw_root.resolve()
        spec=importlib.util.spec_from_file_location('official_deepcw',root/'examples/python/decode_morse.py')
        official=importlib.util.module_from_spec(spec);spec.loader.exec_module(official)
        metadata=official.load_metadata(root/'model.onnx.json')
        options=ort.SessionOptions();options.intra_op_num_threads=2;options.inter_op_num_threads=1
        session=ort.InferenceSession(str(root/'model.onnx'),sess_options=options,providers=['CPUExecutionProvider'])
        engines.append('DeepCW')
    def run(audio,rate,tone):
        path=args.out/'input.f32';np.asarray(audio,dtype='<f4').tofile(path)
        result={k:subprocess.check_output([str(p),'audio',str(rate),str(tone),str(path),'997'],text=True,
                                          stderr=subprocess.DEVNULL,timeout=30).strip() for k,p in probes.items()}
        if args.deepcw_root:
            if len(audio)/rate>20.:
                result['DeepCW']=None # Outside the official example's supported window.
            else:
                x=official.resample_linear(np.asarray(audio,dtype=np.float32),rate,int(metadata['sample_rate']))
                x=np.pad(x,(0,max(0,5*int(metadata['sample_rate'])-len(x))))
                spectrum=official.audio_to_spectrogram(x,metadata)
                logits=session.run([metadata['onnx_output_name']],{metadata['onnx_input_name']:spectrum})[0]
                result['DeepCW']=official.greedy_ctc_decode(logits,list(metadata['chars']),int(metadata['blank_index'])).strip()
        return result
    details=[]
    for group in ['original','receiver_filtered','clean']:
        for i in range(args.records):
            seed=args.seed+{'original':0,'receiver_filtered':1000000,'clean':4000000}[group]+i
            extra={}
            if group=='original':record,audio=example(seed,difficulty=.5,return_audio=True)
            elif group=='receiver_filtered':record,audio,extra=augmented(seed,filtered=True,return_metadata=True)
            else:record,audio=clean(seed,return_audio=True)
            rate=[8000,16000,44100,48000][i%4]
            from math import gcd
            factor=gcd(rate,4000);audio=resample_poly(audio,rate//factor,4000//factor)
            reference=record[3].strip();predictions=run(audio,rate,1000)
            details.append({'group':group,'seed':seed,'rate':rate,'reference':reference,**extra,
                            'decoded':predictions,'edits':{k:distance(reference,v) if v is not None else None for k,v in predictions.items()}})
            if (i+1)%20==0:print(group,i+1,'/',args.records,flush=True)
    negatives=[]
    for i,width in enumerate([0,70,100,150,200,300,500,800,1200]):
        for j in range(4):
            _,audio=negative(args.seed+2000000+10*i+j,seconds=10.,width=width,return_audio=True)
            predictions=run(resample_poly(audio,2,1),8000,1000)
            negatives.append({'width_hz':width,'seed':args.seed+2000000+10*i+j,'decoded':predictions})
    # Long clean and impaired messages expose accumulation/repeated-character
    # failures that short random training records can miss.
    long_messages=[]
    text='CQ CQ CQ DE IZ6NNH IZ6NNH K TEST 599 73 CQ DE MM0ABC MM0ABC'
    for speed in [12,16,20,24,28,32,36]:
        _,audio=example(args.seed+3000000+speed,difficulty=.3,text=text,wpm=speed,return_audio=True)
        predictions=run(resample_poly(audio,12,1),48000,1000)
        long_messages.append({'wpm':speed,'reference':text,'decoded':predictions,
                              'edits':{k:distance(text,v) if v is not None else None for k,v in predictions.items()}})
    radio=[]
    for p in args.websdr:
        with wave.open(str(p)) as w:
            if w.getnchannels()!=1 or w.getsampwidth()!=2:raise ValueError('WebSDR WAV must be mono PCM16')
            rate=w.getframerate();audio=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype('float32')/32768
        radio.append({'file':p.name,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),
                      'seconds':len(audio)/rate,'rate':rate,'reference':None,
                      'note':'Unlabelled real radio audio; predictions are NOT ground truth or CER measurements.',
                      'decoded_at_750_hz':run(audio,rate,750)})
    summary={}
    for group in ['original','receiver_filtered','clean']:
        records=[r for r in details if r['group']==group]
        characters=sum(len(r['reference']) for r in records)
        summary[group]={'records':len(records),'characters':characters}
        for k in engines:
            supported=[r for r in records if r['decoded'][k] is not None]
            count=sum(len(r['reference']) for r in supported)
            summary[group][k]={'cer':sum(r['edits'][k] for r in supported)/max(1,count),
                              'errors':sum(r['edits'][k] for r in supported),
                              'characters':count,'supported_records':len(supported),
                              'exact_records':sum(r['reference']==r['decoded'][k] for r in supported)}
    summary['negatives']={'records':len(negatives),'seconds_per_record':10.,
                          **{k:{'characters':sum(len(r['decoded'][k] or '') for r in negatives),
                                'nonempty_records':sum(bool(r['decoded'][k]) for r in negatives)} for k in engines}}
    summary['long_messages']={}
    for k in engines:
        supported=[r for r in long_messages if r['decoded'][k] is not None]
        count=len(text)*len(supported)
        summary['long_messages'][k]={'characters':count,'supported_records':len(supported),
            'cer':sum(r['edits'][k] for r in supported)/count if count else None}
    result={'seed':args.seed,'scope':'Independent synthetic positive/negative tests; real radio recordings remain unlabelled.',
            'probe_sha256':{k:hashlib.sha256(p.read_bytes()).hexdigest() for k,p in probes.items()},
            'summary':summary,'details':details,'negative_details':negatives,
            'long_details':long_messages,'websdr':radio}
    if args.deepcw_root:
        result['deepcw']={'model_sha256':hashlib.sha256((root/'model.onnx').read_bytes()).hexdigest(),
                         'frontend':'Official Python example, including linear resampling; zero-pad to 5 s; >20 s unsupported.',
                         'integration':'External benchmark only; not integrated, trained against, or redistributed.'}
    (args.out/'COMPARISON.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'summary':summary,'websdr':radio},indent=2))


if __name__=='__main__':main()
