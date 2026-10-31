#!/usr/bin/env python3
"""Train MM's own small streaming temporal-CNN/CTC CW model from generated audio.

SPDX-License-Identifier: GPL-3.0-or-later
Copyright (C) 2026 Papadopol Lucian-Ioan.
No upstream CW source, model weights, recordings or generated predictions are used.
Training-only dependencies: numpy, scipy, torch. Runtime: standard C++17.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import random
import time

import numpy as np
from scipy.signal import lfilter
import torch
from torch import nn

ALPHABET = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/?.,"
MORSE = dict(zip("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/?.,", [
    '.-', '-...', '-.-.', '-..', '.', '..-.', '--.', '....', '..', '.---',
    '-.-', '.-..', '--', '-.', '---', '.--.', '--.-', '.-.', '...', '-',
    '..-', '...-', '.--', '-..-', '-.--', '--..', '-----', '.----', '..---',
    '...--', '....-', '.....', '-....', '--...', '---..', '----.', '-..-.',
    '..--..', '.-.-.-', '--..--']))
RATE, WINDOW, HOP = 4000, 128, 32
OFFSETS = np.arange(-4, 5) * 62.5
CHANNELS = 48


def features(audio, tone=1000.):
    """Rectangular 32 ms coherent bank, 8 ms hop; normalized to local RMS."""
    t = np.arange(len(audio)) / RATE
    ends = np.arange(HOP - 1, len(audio), HOP)
    energy = lfilter(np.ones(WINDOW), [1.], audio * audio)[ends] / WINDOW
    values = []
    for offset in OFFSETS:
        mixed = audio * np.exp(-2j * np.pi * (tone + offset) * t)
        acc = np.concatenate(([0j], np.cumsum(mixed)))
        sums = acc[ends + 1] - acc[np.maximum(0, ends + 1 - WINDOW)]
        values.append(np.log1p(5 * np.abs(sums) / WINDOW /
                              np.sqrt(np.maximum(energy, 1e-12))))
    return np.asarray(values, dtype=np.float32).T.copy()


def example(seed, difficulty=1., text=None, wpm=None, return_audio=False):
    rng = np.random.default_rng(seed)
    wpm = float(wpm if wpm is not None else rng.uniform(12, 36))
    unit = 1.2 / wpm
    if text is None:
        chars = list(ALPHABET[1:])
        text = ''.join(rng.choice(chars, size=int(rng.integers(4, 13))))
        if rng.random() < .65:
            pos = int(rng.integers(1, len(text)))
            text = text[:pos] + ' ' + text[pos:]
    # Exact alignment labels supplement CTC; labels occur after character gaps.
    runs, stamps = [], []
    elapsed = 0.
    def add(down, duration):
        nonlocal elapsed
        duration = max(.004, duration)
        runs.append((down, duration)); elapsed += duration
    add(False, .12)
    for ch in text:
        if ch == ' ':
            start = elapsed
            add(False, 4 * unit)
            stamps.append((start + unit, start + 2.5 * unit, 1))
            continue
        for i, mark in enumerate(MORSE[ch]):
            jitter = np.clip(rng.normal(1, .035 * difficulty), .85, 1.15)
            add(True, (1 if mark == '.' else 3) * unit * jitter)
            if i + 1 < len(MORSE[ch]): add(False, unit)
        stop = elapsed
        add(False, 3 * unit)
        stamps.append((stop + 1.35 * unit, stop + 2.5 * unit,
                       ALPHABET.index(ch) + 1))
    add(False, .4)
    n = int(math.ceil(elapsed * RATE))
    env = np.zeros(n)
    pos = 0
    for down, duration in runs:
        end = min(n, pos + int(round(duration * RATE)))
        if down: env[pos:end] = 1
        pos = end
    # Smooth key edges and vary level, noise, drift and fading independently.
    edge = max(1, int(.002 * RATE))
    env = np.convolve(env, np.ones(edge) / edge, mode='same')
    t = np.arange(n) / RATE
    drift = rng.uniform(-18, 18) * difficulty
    phase = 2 * np.pi * ((1000 + drift) * t +
                        difficulty * rng.uniform(-.5, .5) * t * t)
    gain = rng.uniform(.15, .8)
    fading = 1 - difficulty * rng.uniform(0, .35) * (.5 + .5 * np.sin(
        2 * np.pi * rng.uniform(.15, .8) * t + rng.uniform(0, 6)))
    noise = rng.uniform(.005, .18) * difficulty
    audio = gain * env * fading * np.sin(phase) + rng.normal(0, noise, n)
    if difficulty > .5 and rng.random() < .3:
        freq = 1000 + rng.choice([-1, 1]) * rng.uniform(160, 400)
        audio += gain * rng.uniform(.05, .5) * difficulty * np.sin(2*np.pi*freq*t)
    x = features(audio)
    times = (np.arange(len(x)) + 1) * HOP / RATE
    labels = np.zeros(len(x), dtype=np.int64)
    for start, end, label in stamps:
        labels[(times >= start) & (times < end)] = label
    target = np.asarray([ALPHABET.index(c) + 1 for c in text], dtype=np.int64)
    record = (x, labels, target, text)
    return (record, audio) if return_audio else record


class Model(nn.Module):
    def __init__(self):
        super().__init__()
        self.layers=nn.ModuleList([nn.Conv1d(9 if i==0 else 48,48,5,dilation=d) for i,d in enumerate([1,4,16,64])])
        self.head=nn.Linear(48,len(ALPHABET)+1)
    def forward(self,x):
        x=x.transpose(1,2)
        for i,(layer,d) in enumerate(zip(self.layers,[1,4,16,64])):
            y=layer(torch.nn.functional.pad(x,(4*d,0)))
            x=torch.tanh(y)
        return self.head(x.transpose(1,2))


def batch(data):
    lengths = torch.tensor([len(v[0]) for v in data], dtype=torch.long)
    x = torch.zeros(len(data), int(lengths.max()), 9)
    y = torch.full(x.shape[:2], -100, dtype=torch.long)
    for i, (a, b, _, _) in enumerate(data):
        x[i, :len(a)] = torch.from_numpy(a); y[i, :len(b)] = torch.from_numpy(b)
    targets = torch.from_numpy(np.concatenate([v[2] for v in data]))
    target_lengths = torch.tensor([len(v[2]) for v in data], dtype=torch.long)
    return x, y, targets, lengths, target_lengths


def decode(indices):
    out, prev = [], 0
    for i in indices:
        i = int(i)
        if i and i != prev: out.append(ALPHABET[i-1])
        prev = i
    return ''.join(out)


def distance(a, b):
    row = list(range(len(b)+1))
    for i, c in enumerate(a, 1):
        nxt = [i]
        for j, d in enumerate(b, 1):
            nxt.append(min(row[j]+1, nxt[-1]+1, row[j-1]+(c != d)))
        row = nxt
    return row[-1]


def evaluate(model, data):
    model.eval()
    edits = total = exact = 0
    examples = []
    with torch.no_grad():
        for start in range(0, len(data), 16):
            part = data[start:start+16]
            x, _, _, lengths, _ = batch(part)
            predictions = model(x).argmax(-1)
            for i, item in enumerate(part):
                pred = decode(predictions[i, :lengths[i]].tolist()).strip()
                ref = item[3].strip()
                edits += distance(ref, pred); total += len(ref); exact += pred == ref
                if len(examples) < 12: examples.append({'reference':ref, 'decoded':pred})
    model.train()
    return {'cer':edits / max(1,total), 'exact':exact, 'records':len(data),
            'characters':total, 'examples':examples}


def export(model, directory, metadata):
    directory.mkdir(parents=True, exist_ok=True)
    arrays = {n: t.detach().cpu().numpy().astype('<f4') for n,t in model.state_dict().items()}
    parts = ['// Generated by tools/cw_neural/train.py from independently trained MM weights.',
             '// SPDX-License-Identifier: GPL-3.0-or-later', '#pragma once',
             'namespace madmodem::cwskimmer::neural_weights {',
             'inline constexpr int kChannels = 48;',
             f'inline constexpr char kAlphabet[] = "{ALPHABET}";']
    for name, a in arrays.items():
        key = name.replace('.', '_')
        parts.append(f'inline constexpr float {key}[] = {{')
        v = a.ravel()
        for i in range(0,len(v),8):
            parts.append(','.join(f'{float(n):.9e}f' for n in v[i:i+8])+',')
        parts.append('};')
    parts.append('}')
    header = ('\n'.join(parts)+'\n').encode()
    (directory/'CwNeuralWeights.h').write_bytes(header)
    metadata['weights_header_sha256'] = hashlib.sha256(header).hexdigest()
    metadata['parameter_count'] = sum(a.size for a in arrays.values())
    (directory/'MODEL_CARD.json').write_text(json.dumps(metadata,indent=2)+'\n')
    torch.save(model.state_dict(), directory/'checkpoint.pt')


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--steps',type=int,default=3000)
    ap.add_argument('--batch-size',type=int,default=24)
    ap.add_argument('--seed',type=int,default=17062026)
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--resume',type=Path)
    ap.add_argument('--ctc-weight',type=float,default=.1,help='CTC weight for resumed full-range training')
    ap.add_argument('--learning-rate',type=float,default=.0015)
    args=ap.parse_args()
    torch.set_num_threads(2); torch.manual_seed(args.seed)
    np.random.seed(args.seed); random.seed(args.seed)
    model=Model()
    if args.resume: model.load_state_dict(torch.load(args.resume, weights_only=True))
    opt=torch.optim.AdamW(model.parameters(), lr=args.learning_rate, weight_decay=.0001)
    weights=torch.ones(len(ALPHABET)+1); weights[0]=.04
    ce=nn.CrossEntropyLoss(weight=weights,ignore_index=-100)
    ctc=nn.CTCLoss(blank=0,zero_infinity=True)
    print('generating independent training/validation audio',flush=True)
    train=[example(args.seed+i, difficulty=0., wpm=20.) for i in range(600)]
    train += [example(args.seed+600+i, difficulty=.3, wpm=float(np.random.default_rng(args.seed+i).uniform(16,26))) for i in range(600)]
    train += [example(args.seed+1200+i, difficulty=.7) for i in range(1800)]
    for i in range(150):
        rng=np.random.default_rng(args.seed+10000+i)
        t=np.arange(14000)/RATE
        audio=rng.normal(0,rng.uniform(.01,.2),len(t))
        if i%2: audio += rng.uniform(.1,.7)*np.sin(2*np.pi*(1000+rng.uniform(-20,20))*t)
        x=features(audio)
        train.append((x,np.zeros(len(x),dtype=np.int64),np.array([],dtype=np.int64),''))
    valid=[example(900000+i, difficulty=.5) for i in range(80)]
    best=100.; history=[]; started=time.time()
    for step in range(args.steps):
        limit = 600 if step < 800 and not args.resume else 1200 if step < 1400 and not args.resume else len(train)
        indices=np.random.randint(0,limit,args.batch_size)
        x,y,target,lengths,target_lengths=batch([train[i] for i in indices])
        opt.zero_grad(); logits=model(x)
        loss_ce=ce(logits.flatten(0,1),y.flatten())
        loss_ctc=ctc(logits.log_softmax(-1).transpose(0,1),target,lengths,target_lengths)
        factor=0. if step < 1000 and not args.resume else .03 if step < 1800 and not args.resume else args.ctc_weight
        loss=loss_ce + factor*loss_ctc
        loss.backward(); nn.utils.clip_grad_norm_(model.parameters(),5.); opt.step()
        if (step+1)%50==0:
            print(f'step {step+1}/{args.steps} loss={loss.item():.4f} CE={loss_ce.item():.4f} CTC={loss_ctc.item():.4f} seconds={time.time()-started:.1f}',flush=True)
        if (step+1)%200==0 or step+1==args.steps:
            result=evaluate(model,valid); result['step']=step+1; history.append(result)
            print('validation',json.dumps(result),flush=True)
            if result['cer']<best:
                best=result['cer']
                export(model,args.out, {'id':'mm-cw-tcn-ctc-v1','license':'GPL-3.0-or-later',
                    'origin':'Random initialization; generated Morse waveforms only; no DeepCW code, weights or teacher outputs.',
                    'curriculum':'Resumed: full 12–36 WPM dataset from step 0' if args.resume else '0–799: clean 20 WPM; 800–1399: 16–26 WPM; then full 12–36 WPM and negative records',
                    'resume_checkpoint_sha256':hashlib.sha256(args.resume.read_bytes()).hexdigest() if args.resume else None,
                    'learning_rate':args.learning_rate, 'ctc_weight':args.ctc_weight,
                    'seed':args.seed,'torch_version':torch.__version__, 'training_records':len(train),
                    'validation_seed_start':900000,'validation':result,'history':history,
                    'architecture':{'features':9,'window_ms':32,'hop_ms':8,'tcn_channels':48,'dilations':[1,4,16,64],
                    'kernel_size':5,'direction':'causal','classes':len(ALPHABET)+1},
                    'training_wpm':[12,36] if args.resume else [20,20] if step<800 else [16,26] if step<1400 else [12,36], 'experimental':True,
                    'limitations':['Synthetic audio only; not validated on live radio recordings.',
                    'No claimed sensitivity improvement over native CW.']})


if __name__=='__main__': main()
