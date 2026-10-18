#!/usr/bin/env python3
"""Unlabelled radio-window comparison. Never produces a radio CER.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import wave
import numpy as np
import onnxruntime as ort


def main():
    ap=argparse.ArgumentParser()
    for name in ['native','neural','deepcw-root','out']:ap.add_argument('--'+name,type=Path,required=True)
    ap.add_argument('--wav',type=Path,action='append',required=True)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    root=args.deepcw_root.resolve()
    spec=importlib.util.spec_from_file_location('official_deepcw',root/'examples/python/decode_morse.py')
    official=importlib.util.module_from_spec(spec);spec.loader.exec_module(official)
    metadata=official.load_metadata(root/'model.onnx.json')
    options=ort.SessionOptions();options.intra_op_num_threads=2;options.inter_op_num_threads=1
    session=ort.InferenceSession(str(root/'model.onnx'),sess_options=options,providers=['CPUExecutionProvider'])
    result=[]
    for wav in args.wav:
        with wave.open(str(wav)) as w:
            if w.getnchannels()!=1 or w.getsampwidth()!=2:raise ValueError('mono PCM16 required')
            rate=w.getframerate();audio=np.frombuffer(w.readframes(w.getnframes()),dtype='<i2').astype(np.float32)/32768
        windows=[]
        for start in range(0,len(audio),20*rate):
            samples=audio[start:start+20*rate];path=args.out/'window.f32';samples.astype('<f4').tofile(path)
            prediction={k:subprocess.check_output([str(p.resolve()),'audio',str(rate),'750',str(path),'997'],
                text=True,stderr=subprocess.DEVNULL,timeout=30).strip() for k,p in [('Native',args.native),('R7',args.neural)]}
            x=official.resample_linear(samples,rate,int(metadata['sample_rate']))
            x=np.pad(x,(0,max(0,5*int(metadata['sample_rate'])-len(x))))
            spectrum=official.audio_to_spectrogram(x,metadata)
            logits=session.run([metadata['onnx_output_name']],{metadata['onnx_input_name']:spectrum})[0]
            prediction['DeepCW']=official.greedy_ctc_decode(logits,list(metadata['chars']),int(metadata['blank_index'])).strip()
            windows.append({'start_seconds':start/rate,'duration_seconds':len(samples)/rate,'decoded':prediction,'reference':None})
        result.append({'file':wav.name,'sha256':hashlib.sha256(wav.read_bytes()).hexdigest(),'windows':windows})
    document={'scope':'Unlabelled real-radio audio, independent resets per nonoverlapping <=20 s window. Short windows padded to 5 s for DeepCW. No verified transcript and no CER.',
              'tone_hz_native_and_mm':750,'recordings':result}
    (args.out/'RADIO_WINDOWS.json').write_text(json.dumps(document,indent=2)+'\n')
    print(json.dumps(document,indent=2))


if __name__=='__main__':main()
