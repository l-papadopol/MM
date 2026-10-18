#!/usr/bin/env python3
"""Independent seeded waveform evaluation and Python/C++ inference parity.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import json
from pathlib import Path
import subprocess
import time

import numpy as np
from scipy.signal import resample_poly
import torch
from train import Model, example, distance


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--probe',type=Path,required=True)
    ap.add_argument('--model',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--records',type=int,default=60)
    args=ap.parse_args(); args.out.mkdir(parents=True,exist_ok=True)
    torch.set_num_threads(2)
    model=Model(); model.load_state_dict(torch.load(args.model/'checkpoint.pt',weights_only=True)); model.eval()
    rng=np.random.default_rng(52027)
    inputs=rng.uniform(0,2,(700,9)).astype('float32')
    feature_path=args.out/'features.f32'; logits_path=args.out/'logits.f32'
    inputs.tofile(feature_path)
    with torch.no_grad(): reference=model(torch.from_numpy(inputs)[None])[0].numpy()
    subprocess.run([str(args.probe),'features',str(feature_path),str(logits_path)],check=True)
    actual=np.fromfile(logits_path,dtype='float32').reshape(-1,42)
    error=float(np.abs(reference-actual).max())
    if error>.0003: raise RuntimeError(f'Inference parity failed: {error}')
    records=[]; seconds_audio=seconds_cpu=0.
    for i in range(args.records):
        # Seeds are disjoint from both training and model-selection validation.
        record,audio=example(1900000+i,difficulty=.5,return_audio=True)
        rate=[8000,16000,44100,48000][i%4]
        from math import gcd
        factor=gcd(rate,4000)
        sampled=resample_poly(audio,rate//factor,4000//factor).astype('float32')
        path=args.out/'input.f32'; sampled.tofile(path)
        begin=time.perf_counter()
        pred=subprocess.check_output([str(args.probe),'audio',str(rate),'1000',str(path),'997'],text=True).strip()
        elapsed=time.perf_counter()-begin
        seconds_cpu+=elapsed; seconds_audio+=len(sampled)/rate
        ref=record[3].strip()
        records.append({'seed':1900000+i,'rate':rate,'reference':ref,'decoded':pred,
                        'edit_distance':distance(ref,pred)})
    # Chunk-size independence includes chunks shorter than an analysis hop.
    record,audio=example(2900000,difficulty=0,text='CQ CQ DE IZ6NNH TEST 73',wpm=20,return_audio=True)
    audio=resample_poly(audio,12,1).astype('float32')
    path=args.out/'chunk-input.f32';audio.tofile(path)
    chunks={str(size):subprocess.check_output([str(args.probe),'audio','48000','1000',str(path),str(size)],text=True).strip()
            for size in [17,997,4096]}
    if len(set(chunks.values()))!=1: raise RuntimeError('Audio chunk boundaries changed decoding')
    negatives=[]
    for name in ['silence','white_noise','steady_carrier']:
        t=np.arange(48000*8)/48000
        audio=np.zeros(len(t))
        if name!='silence': audio=rng.normal(0,.1,len(t))
        if name=='steady_carrier': audio+=.5*np.sin(2*np.pi*1000*t)
        path=args.out/'negative.f32';audio.astype('float32').tofile(path)
        pred=subprocess.check_output([str(args.probe),'audio','48000','1000',str(path),'1024'],text=True).strip()
        negatives.append({'input':name,'decoded':pred})
    chars=sum(len(r['reference']) for r in records)
    result={'model':json.loads((args.model/'MODEL_CARD.json').read_text())['id'],
            'source':'Independent synthetic waveforms; no live radio recordings.',
            'test_seed_start':1900000,'records':len(records),'characters':chars,
            'character_error_rate':sum(r['edit_distance'] for r in records)/max(chars,1),
            'exact_records':sum(r['reference']==r['decoded'] for r in records),
            'python_cpp_max_logits_error':error,'chunk_results':chunks,
            'negative_records':negatives,'audio_seconds':seconds_audio,
            'process_seconds_including_startup':seconds_cpu,'details':records}
    (args.out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='details'},indent=2))


if __name__=='__main__':main()
