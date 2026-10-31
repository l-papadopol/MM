#!/usr/bin/env python3
"""Fresh generated QSO holdout with unseen calls, names and places.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse,json,hashlib,subprocess
from pathlib import Path
from scipy.signal import resample_poly
import numpy as np
from train import distance
from qso_refine import qso_record
from qso_compare import align_words

def main():
    ap=argparse.ArgumentParser()
    for n in ['baseline','candidate','stage-a','out']:ap.add_argument('--'+n,type=Path,required=True)
    ap.add_argument('--records',type=int,default=120);args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    probes={'R7':args.baseline.resolve(),'R8_A':args.stage_a.resolve(),'R8':args.candidate.resolve()};rows=[]
    hashes={k:hashlib.sha256(p.read_bytes()).hexdigest() for k,p in probes.items()}
    for i in range(args.records):
        seed=111117000+i;record,audio,meta=qso_record(seed,'test',True)
        audio=resample_poly(audio,2,1).astype('<f4');p=args.out/'input.f32';audio.tofile(p);reference=record[3]
        decoded={};memo={}
        for k,probe in probes.items():
            if hashes[k] not in memo:
                memo[hashes[k]]=subprocess.check_output([str(probe),'audio','8000','1000',str(p),'997'],text=True,timeout=30).strip()
            decoded[k]=memo[hashes[k]]
        metrics={}
        for k,v in decoded.items():
            aligned=align_words(reference,v);reports={'599','559','579','589','5NN'}
            metrics[k]={'errors':distance(reference,v),'exact':reference==v,
              'calls_correct':sum(a in meta['calls'] and a==b for a,b in aligned),'calls_total':sum(a in meta['calls'] for a,b in aligned),
              'reports_correct':sum(a in reports and a==b for a,b in aligned),'reports_total':sum(a in reports for a,b in aligned)}
        rows.append({'seed':seed,'reference':reference,**meta,'decoded':decoded,'metrics':metrics})
        if (i+1)%20==0:print('QSO holdout',i+1,'/',args.records,flush=True)
    summary={}
    for width in ['all',*sorted({r['width_hz'] for r in rows})]:
        part=[r for r in rows if width=='all' or r['width_hz']==width];characters=sum(len(r['reference']) for r in part)
        summary[str(width)]={'records':len(part),'characters':characters,
          **{k:{'cer':sum(r['metrics'][k]['errors'] for r in part)/characters,
               **{field:sum(r['metrics'][k][field] for r in part) for field in ['errors','exact','calls_correct','calls_total','reports_correct','reports_total']}} for k in probes}}
    result={'scope':'New QSO holdout after both training phases, excluded from model selection. Callsigns and complete messages generated from disjoint seeds; names/QTH vocabulary separate from training/development.',
            'seed_range':[111117000,111117000+args.records-1],'probe_sha256':{k:hashlib.sha256(p.read_bytes()).hexdigest() for k,p in probes.items()},
            'summary':summary,'details':rows}
    (args.out/'QSO_HOLDOUT.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(summary,indent=2))

if __name__=='__main__':main()
