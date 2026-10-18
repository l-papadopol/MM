#!/usr/bin/env python3
"""Select calibration/checkpoint using development only, with replay guards.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import hashlib
import json
from pathlib import Path
import torch
from train import Model, export
from extend import validation_cache, score_cache


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--experiment', type=Path, required=True)
    ap.add_argument('--parent', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--steps', type=int, nargs='*')
    args = ap.parse_args()
    torch.set_num_threads(1)
    groups = torch.load(args.experiment/'development-private.pt', weights_only=False)['groups']
    weighting = {'arrl': .45, 'qso': .4, 'random': .15}
    model = Model()
    model.load_state_dict(torch.load(args.parent, weights_only=True))
    baseline = score_cache(validation_cache(model, groups), [0.], [0.], weights=weighting)
    rows = []
    checkpoints = [args.parent]+sorted((args.experiment/'checkpoints').glob('step-*.pt'))
    for checkpoint in checkpoints:
        step = 0 if checkpoint == args.parent else int(checkpoint.stem.split('-')[-1])
        if args.steps is not None and step not in args.steps:
            continue
        model.load_state_dict(torch.load(checkpoint, weights_only=True))
        cache = validation_cache(model, groups)
        trials = [score_cache(cache, [space], [blank], weights=weighting)
                  for space in [-1.,0.,1.,2.,3.,4.,5.] for blank in [-.5,0.,.5]]
        # Match runtime suppression of consecutive spaces. Reject any false
        # negative-input characters, and substantial loss on arbitrary strings.
        eligible_trials = [r for r in trials if r['metrics']['negative']['emitted'] == 0
                           and r['metrics']['random']['errors'] <= baseline['metrics']['random']['errors']+2]
        eligible = bool(eligible_trials)
        result = min(eligible_trials or trials, key=lambda r: r['selection_score'])
        row = {'step': step, 'checkpoint_sha256': hashlib.sha256(checkpoint.read_bytes()).hexdigest(), 'eligible': eligible, **result}
        rows.append(row)
        print(json.dumps(row), flush=True)
    chosen = min((r for r in rows if r['eligible']), key=lambda r: r['selection_score'])
    checkpoint = args.parent if chosen['step']==0 else args.experiment/'checkpoints'/f"step-{chosen['step']:06d}.pt"
    model.load_state_dict(torch.load(checkpoint, weights_only=True))
    with torch.no_grad():
        model.head.bias[0] += chosen['blank_bias']
        model.head.bias[1] += chosen['space_bias']
    card = json.loads((args.experiment/'MODEL_CARD.json').read_text())
    card.update({'selected_step': chosen['step'], 'validation': chosen, 'selection': 'Lowest weighted development CER after constant blank/space calibration; no false characters on development negatives; random-string errors no more than parent+2. No RF/final-test selection.',
                 'development_metric': 'Runtime CTC collapse and suppression of consecutive spaces. Initial raw greedy monitoring is not the final selection metric.', 'baseline_same_frontend_normalized': baseline})
    export(model, args.out, card)
    (args.out/'SELECTION.json').write_text(json.dumps({'baseline': baseline, 'checkpoints': rows, 'chosen': chosen}, indent=2)+'\n')


if __name__ == '__main__':
    main()
