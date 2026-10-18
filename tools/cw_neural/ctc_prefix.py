#!/usr/bin/env python3
"""Bounded CTC prefix search without a dictionary or language model.
SPDX-License-Identifier: GPL-3.0-or-later
Reference: Graves et al., ICML 2006, https://www.cs.toronto.edu/~graves/icml_2006.pdf
"""
from collections import defaultdict
import numpy as np


def streaming_decode(logits, width=4, topk=6, insertion_penalty=1.5, blank_threshold=.995, blank_frames=6):
    """Causal consensus commits plus confident-blank section boundaries.

    Events are (frame index, emitted label tuple). No file-end oracle/flush.
    """
    beams = {(): (1., 0.)}
    last = 0
    blank_run = 0
    events = []
    penalty = float(np.exp(-insertion_penalty))
    probability = np.exp(logits-logits.max(axis=1, keepdims=True))
    probability /= probability.sum(axis=1, keepdims=True)
    for frame, p in enumerate(probability):
        labels = np.argsort(p[1:])[-topk:]+1
        new = defaultdict(lambda: [0., 0.])
        for prefix, (blank, nonblank) in beams.items():
            total = blank+nonblank
            new[prefix][0] += total*float(p[0])
            previous = prefix[-1] if prefix else last
            for label in labels:
                label = int(label)
                value = float(p[label])
                if value < 1e-6:
                    continue
                if previous == label:
                    new[prefix][1] += nonblank*value
                    new[prefix+(label,)][1] += blank*value*penalty
                else:
                    new[prefix+(label,)][1] += total*value*penalty
        best = sorted(new, key=lambda s: sum(new[s]), reverse=True)[:width]
        scale = sum(new[best[0]])
        beams = {s: (new[s][0]/scale, new[s][1]/scale) for s in best}
        common = len(best[0])
        for s in best[1:]:
            common = min(common, len(s))
            while common and s[:common] != best[0][:common]:
                common -= 1
        if common:
            prefix = best[0][:common]
            events.append((frame, prefix))
            last = prefix[-1]
            beams = {s[common:]: v for s, v in beams.items()}
        blank_run = blank_run+1 if p[0] >= blank_threshold else 0
        if blank_run >= blank_frames or max(map(len, beams)) >= 64:
            prefix = max(beams, key=lambda s: sum(beams[s]))
            if prefix:
                events.append((frame, prefix))
                last = prefix[-1]
            beams = {(): (1., 0.)}
            blank_run = 0
    return tuple(label for _, labels in events for label in labels), events


def prefix_decode(logits, width=8, topk=6, insertion_penalty=0.):
    """Return best label tuple; blank is zero. Accumulate alignment probability."""
    beams = {(): (1., 0.)}
    penalty = float(np.exp(-insertion_penalty))
    probability = np.exp(logits-logits.max(axis=1, keepdims=True))
    probability /= probability.sum(axis=1, keepdims=True)
    for p in probability:
        labels = np.argsort(p[1:])[-topk:]+1
        new = defaultdict(lambda: [0., 0.])
        for prefix, (blank, nonblank) in beams.items():
            total = blank+nonblank
            new[prefix][0] += total*float(p[0])
            for label in labels:
                label = int(label)
                value = float(p[label])
                if value < 1e-6:
                    continue
                if prefix and prefix[-1] == label:
                    new[prefix][1] += nonblank*value
                    new[prefix+(label,)][1] += blank*value*penalty
                else:
                    new[prefix+(label,)][1] += total*value*penalty
        best = sorted(new, key=lambda s: sum(new[s]), reverse=True)[:width]
        scale = sum(new[best[0]])
        beams = {s: (new[s][0]/scale, new[s][1]/scale) for s in best}
    return max(beams, key=lambda s: sum(beams[s]))
