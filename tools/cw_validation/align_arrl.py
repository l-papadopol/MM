#!/usr/bin/env python3
"""Audit published ARRL labels against measured Morse pulse sequences.
Original annotation utility; no neural/native decoder predictions are used.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
from collections import Counter, defaultdict
import difflib
import hashlib
import json
from pathlib import Path
import re
import subprocess

import numpy as np
from scipy.signal import butter, sosfiltfilt

MORSE = dict(zip("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/?.,", [
    '.-', '-...', '-.-.', '-..', '.', '..-.', '--.', '....', '..', '.---',
    '-.-', '.-..', '--', '-.', '---', '.--.', '--.-', '.-.', '...', '-',
    '..-', '...-', '.--', '-..-', '-.--', '--..', '-----', '.----', '..---',
    '...--', '....-', '.....', '-....', '--...', '---..', '----.', '-..-.',
    '..--..', '.-.-.-', '--..--']))
MORSE.update({"=": "-...-", "'": ".----.", '"': '.-..-.',
              "-": "-....-", ":": "---...", ";": "-.-.-.",
              "(": "-.--.", ")": "-.--.-", "+": ".-.-.", "@": ".--.-."})
# Verified from the actual pulse sequences in this imported set, not inferred
# from a text codec or from a decoder prediction. The trailing ASCII '<'
# present in six files has no transmitted symbol in these six recordings.
CONTROL_MORSE = {0x89: ('<BT>', '-...-'), 0x83: ('<AS>', '.-...'),
                 0x82: ('<AR>', '.-.-.')}


def runs(mask):
    edges = np.flatnonzero(np.diff(np.r_[False, mask, False]))
    return list(zip(edges[::2], edges[1::2]))


def pulses(path, wpm):
    rate = 2000
    audio = np.frombuffer(subprocess.check_output([
        'ffmpeg', '-hide_banner', '-loglevel', 'error', '-i', str(path),
        '-ac', '1', '-ar', str(rate), '-f', 'f32le', '-'], timeout=60), '<f4')
    # 750 Hz at 2 kHz repeats every 8 samples. Resampling is annotation-only.
    oscillator = np.exp(-2j*np.pi*750*np.arange(8)/rate).astype(np.complex64)
    mixed = audio * np.resize(oscillator, len(audio))
    envelope = np.abs(sosfiltfilt(butter(3, 70, fs=rate, output='sos'), mixed))[::4]
    frame_rate = rate/4
    threshold = float(np.quantile(envelope, .92)*.35)
    state = envelope > threshold
    # Remove sub-8 ms codec glitches, not Morse elements.
    for a, b in runs(state):
        if b-a < .008*frame_rate:
            state[a:b] = False
    for a, b in runs(~state):
        if a and b<len(state) and b-a < .008*frame_rate:
            state[a:b] = True
    marks = [(a/frame_rate, b/frame_rate) for a, b in runs(state)]
    unit = 1.2/(15 if wpm <= 15 else wpm)
    rows = []
    code = ''
    start = None
    for i, (a, b) in enumerate(marks):
        if start is None:
            start = a
        code += '-' if b-a > 2*unit else '.'
        gap = marks[i+1][0]-b if i+1 < len(marks) else None
        if gap is None or gap > 2*unit:
            rows.append({'start': start, 'end': b, 'code': code, 'following_gap': gap})
            start = None; code = ''
    return rows, {'threshold': threshold, 'annotation_sample_rate': rate,
                  'element_unit_seconds': unit, 'duration_seconds': len(audio)/rate}


def audit(audio, text, out):
    speed = audio.stem.split('_', 1)[1].removesuffix('WPM')
    wpm = 7.5 if speed == '075' else float(speed)
    raw = text.read_bytes()
    for position, value in enumerate(raw):
        if value == ord('<') and raw[position+1:].strip(b' \t\r\n\x1a'):
            raise ValueError('Unverified non-terminal control: <')
    raw_reference = raw.replace(b'<', b'')
    # Preserve unknown controls as tokens until their physical Morse is checked.
    reference = []
    word_index = -1
    for word in re.findall(rb'[^\s\x1a]+', raw_reference.upper()):
        word_index += 1
        for char in word:
            symbol = chr(char)
            if char in CONTROL_MORSE:
                token, code = CONTROL_MORSE[char]
            else:
                token, code = ('<BT>' if symbol == '=' else symbol), MORSE.get(symbol, f'UNKNOWN_{char:02X}')
            reference.append({'symbol': token, 'code': code, 'word': word_index})
    measured, settings = pulses(audio, wpm)
    expected_codes = [x['code'] for x in reference]
    actual_codes = [x['code'] for x in measured]
    match = difflib.SequenceMatcher(None, expected_codes, actual_codes, autojunk=False)
    aligned = []; problems = []; unknown = defaultdict(Counter)
    known_count = sum(not x.startswith('UNKNOWN_') for x in expected_codes)
    known_matches = 0
    for tag, a, b, c, d in match.get_opcodes():
        if tag == 'equal':
            for i, j in zip(range(a, b), range(c, d)):
                aligned.append({**reference[i], **measured[j], 'reference_index': i})
                known_matches += 1
        elif b-a == d-c:
            for i, j in zip(range(a, b), range(c, d)):
                if reference[i]['code'].startswith('UNKNOWN_'):
                    unknown[reference[i]['symbol']][measured[j]['code']] += 1
                    aligned.append({**reference[i], **measured[j], 'reference_index': i})
                else:
                    problems.append({'reference_index': i, 'expected': reference[i]['code'], 'measured': measured[j]})
        else:
            problems.append({'tag': tag, 'reference_range': [a, b], 'measured_range': [c, d]})
    stats = {'audio': audio.name, 'text': text.name, 'wpm': wpm, **settings,
        'audio_sha256': hashlib.sha256(audio.read_bytes()).hexdigest(),
        'text_sha256': hashlib.sha256(raw).hexdigest(),
        'reference_symbols': len(reference), 'measured_symbols': len(measured),
        'known_symbols': known_count, 'known_symbols_exact': known_matches,
        'known_symbol_match_fraction': known_matches/known_count,
        'unknown_symbol_observations': {k: dict(v) for k,v in unknown.items()},
        'unresolved_alignment_regions': len(problems),
        'trailing_untransmitted_control_removed': raw.count(b'<'),
        'control_mapping_scope': 'Verified on these 11 imported files only; controls must be re-audited on other sets',
        'alignment_method': 'Independent 750-Hz envelope, nominal element speed and Morse pulse durations; published reference sequence alignment, no evaluated decoder output'}
    out.mkdir(parents=True, exist_ok=True)
    (out/(audio.stem+'.alignment-private.json')).write_text(json.dumps({'summary': stats,
        'aligned': sorted(aligned, key=lambda x: x['reference_index']), 'problems': problems}, indent=2)+'\n')
    return stats


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--input', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    summary = []
    for audio in sorted(args.input.glob('*WPM.mp3')):
        token = audio.stem.split('_', 1)[1].removesuffix('WPM')
        txt_token = '07_5' if token == '075' else token
        text = audio.with_name(audio.stem.split('_',1)[0]+'_'+txt_token+'.txt')
        row = audit(audio, text, args.out); summary.append(row)
        (args.out/'ARRL_ALIGNMENT_AUDIT.json').write_text(json.dumps(summary, indent=2)+'\n')
        print(audio.name, row['known_symbol_match_fraction'], row['unresolved_alignment_regions'],
              row['unknown_symbol_observations'], flush=True)


if __name__ == '__main__':
    main()
