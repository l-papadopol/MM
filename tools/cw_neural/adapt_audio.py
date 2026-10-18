#!/usr/bin/env python3
"""Causal frontend adaptation with article-separated real-audio supervision.

SPDX-License-Identifier: GPL-3.0-or-later
Original MM experiment. No third-party model/code/predictions are training truth.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import time

import numpy as np
from scipy.signal import lfilter
import torch
from torch import nn
from train import ALPHABET, Model, batch, decode, distance, example, export, features
from refine import negative
from qso_refine import qso_record


def adaptive_features(audio, tone=1000., floor=.2):
    x = features(audio, tone)
    power = lfilter(np.ones(128), [1.], np.asarray(audio, np.float64)**2)[31::32]/128
    peak = 0.
    denominator = np.empty(len(power))
    release = math.exp(-.008/.5)
    for i, p in enumerate(power):
        peak = max(p, peak*release)
        denominator[i] = max(p, floor*floor*peak, 1e-12)
    return np.log1p(np.expm1(x)*np.sqrt(np.maximum(power, 1e-12)/denominator)[:, None]).astype(np.float32)


def refront(record, audio, tone=1000., floor=.2):
    return (adaptive_features(audio, tone, floor), *record[1:])


def prepare_real(pool, floor):
    segments = [json.loads(v) for v in (pool/'prepared/segments-private.jsonl').read_text().splitlines()]
    audio = {}
    train, dev, audit = [], [], []
    for item in segments:
        if item.get('split') == 'heldout':
            continue
        if set(item['tokens'])-set(ALPHABET):
            continue  # Omit entire waveform, never remove a keyed unsupported label.
        name = item['audio']
        if name not in audio:
            audio[name] = np.frombuffer(subprocess.check_output([
                'ffmpeg', '-v', 'error', '-i', str(pool/name), '-ac', '1', '-ar', '4000', '-f', 'f32le', '-']), '<f4').copy()
        a = audio[name][round(item['start_seconds']*4000):round(item['end_seconds']*4000)]
        a = np.pad(a, (0, 1600))
        text = ''.join(item['tokens']).strip()
        x = adaptive_features(a, 750., floor)
        # Real audio uses transcript-only CTC, not assumed frame-emission timing.
        record = (x, np.full(len(x), -100, np.int64), np.array([ALPHABET.index(c)+1 for c in text]), text)
        is_dev = item['group_id'] == 'arrl-transition-20260304'
        (dev if is_dev else train).append(record)
        audit.append({'id': item['id'], 'group_id': item['group_id'], 'split': 'development' if is_dev else 'train',
                      'sha256': item['audio_sha256'], 'wpm': item['wpm'], 'characters': len(text)})
    indices = np.linspace(0, len(dev)-1, min(24, len(dev)), dtype=int)
    return train, [dev[i] for i in indices], {'records': audit, 'development_subset_indices': indices.tolist()}


def evaluate(model, groups):
    model.eval()
    result = {}
    with torch.inference_mode():
        for name, records in groups.items():
            errors = chars = false = exact = 0
            for begin in range(0, len(records), 4):
                part = records[begin:begin+4]
                x, _, _, lengths, _ = batch(part)
                outputs = model(x).argmax(-1)
                for i, r in enumerate(part):
                    text = decode(outputs[i, :lengths[i]].tolist()).strip()
                    ref = r[3].strip()
                    errors += distance(ref, text)
                    chars += len(ref)
                    false += len(text) if not ref else 0
                    exact += text == ref
            result[name] = {'errors': errors, 'characters': chars, 'cer': errors/max(1, chars),
                            'false_characters': false, 'exact': exact, 'records': len(records)}
    model.train()
    result['selection_score'] = .45*result['arrl']['cer']+.4*result['qso']['cer']+.15*result['random']['cer']+result['negative']['false_characters']/200
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--pool', type=Path, required=True)
    ap.add_argument('--resume', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--seconds', type=float, default=900.)
    ap.add_argument('--floor', type=float, default=.2)
    ap.add_argument('--extra-pool', type=Path)
    ap.add_argument('--freeze-stack', action='store_true', help='Adapt only input convolution and output bias')
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    torch.set_num_threads(2)
    torch.manual_seed(1310172026)
    rng = np.random.default_rng(1310172026)
    real, real_dev, audit = prepare_real(args.pool, args.floor)
    if args.extra_pool:
        extra, extra_dev, extra_audit = prepare_real(args.extra_pool, args.floor)
        if extra_dev:
            raise ValueError('Additional training pool must not supply development material')
        real.extend(extra)
        audit['additional_training_records'] = extra_audit
    (args.out/'DATA_AUDIT.json').write_text(json.dumps(audit, indent=2)+'\n')
    print('real ready', len(real), len(real_dev), flush=True)
    replay = []
    for i in range(480):
        r, a = example(131000000+i, difficulty=float(rng.uniform(.05, .7)), return_audio=True)
        replay.append(refront(r, a, floor=args.floor))
    for i in range(240):
        r, a, _ = qso_record(132000000+i, 'train', True)
        replay.append(refront(r, a, floor=args.floor))
    replay.extend(refront(r, a, floor=args.floor) for r, a in (negative(133000000+i, seconds=5., return_audio=True) for i in range(100)))
    groups = {'arrl': real_dev, 'qso': [], 'random': [], 'negative': []}
    for i in range(32):
        r, a, _ = qso_record(134000000+i, 'dev', True)
        groups['qso'].append(refront(r, a, floor=args.floor))
    for i in range(40):
        r, a = example(135000000+i, difficulty=.45, return_audio=True)
        groups['random'].append(refront(r, a, floor=args.floor))
    for i in range(20):
        r, a = negative(136000000+i, seconds=6., return_audio=True)
        groups['negative'].append(refront(r, a, floor=args.floor))
    print('replay and validation ready', len(replay), flush=True)
    # Cache only features/labels locally; never redistribute third-party audio/transcripts.
    torch.save({'groups': groups}, args.out/'development-private.pt')
    model = Model()
    parent = torch.load(args.resume, weights_only=True)
    model.load_state_dict(parent)
    if args.freeze_stack:
        for name, parameter in model.named_parameters():
            parameter.requires_grad_(name.startswith('layers.0.') or name == 'head.bias')
    baseline = evaluate(model, groups)
    print('baseline', json.dumps(baseline), flush=True)
    metadata = {'id': 'mm-cw-r13-adaptive-candidate', 'experimental': True, 'license': 'GPL-3.0-or-later',
                'parent_sha256': hashlib.sha256(args.resume.read_bytes()).hexdigest(),
                'floor_amplitude_fraction': args.floor, 'peak_power_release_seconds': .5,
                'seed': 1310172026, 'requested_seconds': args.seconds, 'baseline': baseline,
                'freeze_stack': args.freeze_stack,
                'split': 'Two QST article groups train; transition bulletin development. All speed variants remain together. RF never enters training or checkpoint selection.',
                'loss': 'Real: transcript-only CTC. Synthetic replay: CTC + 0.3 frame CE. L2 reference penalty to parent weights.',
                'train_records': {'real': len(real), 'replay': len(replay)},
                'training_seed_ranges': {'random': [131000000,131000479], 'qso': [132000000,132000239], 'negative': [133000000,133000099]},
                'development_seed_ranges': {'qso': [134000000,134000031], 'random': [135000000,135000039], 'negative': [136000000,136000019]},
                'final_test_seeds_reserved': {'qso': [137000000,137000031], 'random': [138000000,138000039], 'negative': [139000000,139000019]}}
    export(model, args.out, {**metadata, 'selected_step': 0, 'validation': baseline})
    pools = []
    for records in [real, replay]:
        order = sorted(range(len(records)), key=lambda i: len(records[i][0]))
        pools.append((records, [order[i:i+24] for i in range(0, len(order), 24)]))
    ce_weights = torch.ones(42)
    ce_weights[0], ce_weights[1] = .1, 1.5
    ce = nn.CrossEntropyLoss(weight=ce_weights, ignore_index=-100)
    ctc = nn.CTCLoss(blank=0, zero_infinity=True)
    opt = torch.optim.AdamW([p for p in model.parameters() if p.requires_grad], lr=2e-5, weight_decay=1e-4)
    reference = [p.detach().clone() for p in model.parameters()]
    best, step, history = baseline['selection_score'], 0, []
    (args.out/'checkpoints').mkdir(exist_ok=True)
    started = time.monotonic()
    while time.monotonic()-started < args.seconds:
        elapsed = time.monotonic()-started
        for g in opt.param_groups:
            g['lr'] = 3e-6+17e-6*.5*(1+math.cos(math.pi*min(1., elapsed/args.seconds)))
        # Replay dominates sampling to retain callsigns, arbitrary strings and negatives.
        source = 0 if rng.random() < .35 else 1
        records, buckets = pools[source]
        chosen = buckets[int(rng.integers(len(buckets)))]
        part = [records[int(rng.choice(chosen))] for _ in range(8)]
        x, y, targets, lengths, target_lengths = batch(part)
        opt.zero_grad(set_to_none=True)
        logits = model(x)
        loss = ctc(logits.log_softmax(-1).transpose(0, 1), targets, lengths, target_lengths)
        if source == 1:
            loss = loss+.3*ce(logits.flatten(0, 1), y.flatten())
        loss = loss+.02*sum((p-r).square().mean() for p, r in zip(model.parameters(), reference))
        if not torch.isfinite(loss):
            raise RuntimeError('Nonfinite adaptation loss')
        loss.backward()
        nn.utils.clip_grad_norm_(model.parameters(), 3.)
        opt.step()
        step += 1
        if step % 100 == 0:
            print('step', step, 'seconds', round(time.monotonic()-started, 1), 'loss', round(loss.item(), 4), flush=True)
        if step % 400 == 0:
            result = evaluate(model, groups)
            history.append({'step': step, 'elapsed_seconds': time.monotonic()-started, 'development': result})
            print('development', json.dumps(history[-1]), flush=True)
            torch.save(model.state_dict(), args.out/'checkpoints'/f'step-{step:06d}.pt')
            if result['selection_score'] < best and result['negative']['false_characters'] <= baseline['negative']['false_characters']+1:
                best = result['selection_score']
                export(model, args.out, {**metadata, 'selected_step': step, 'validation': result})
            (args.out/'TRAINING_HISTORY.json').write_text(json.dumps(history, indent=2)+'\n')
    result = evaluate(model, groups)
    history.append({'step': step, 'elapsed_seconds': time.monotonic()-started, 'development': result})
    torch.save(model.state_dict(), args.out/'checkpoints'/f'step-{step:06d}.pt')
    if result['selection_score'] < best and result['negative']['false_characters'] <= baseline['negative']['false_characters']+1:
        export(model, args.out, {**metadata, 'selected_step': step, 'validation': result})
    (args.out/'TRAINING_HISTORY.json').write_text(json.dumps(history, indent=2)+'\n')
    card = json.loads((args.out/'MODEL_CARD.json').read_text())
    card.update({'completed_steps': step, 'completed_seconds': time.monotonic()-started})
    (args.out/'MODEL_CARD.json').write_text(json.dumps(card, indent=2)+'\n')
    print('completed', step, 'seconds', card['completed_seconds'], flush=True)


if __name__ == '__main__':
    main()
