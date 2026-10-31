#!/usr/bin/env python3
"""Acquire and independently pulse-audit three dated ARRL practice recordings.
SPDX-License-Identifier: GPL-3.0-or-later
Requires NumPy, SciPy, ffmpeg. Raw recordings/transcripts stay at --out locally.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import urllib.request,json,hashlib,subprocess,sys
import argparse
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);args=ap.parse_args()
root=args.out;dest=root/'arrl';dest.mkdir(parents=True,exist_ok=True)
items=[]
for date in ['260107','260204','260218']:
 for ext in ['18WPM.mp3','18.txt']:
  name=date+'_'+ext;url='https://www.arrl.org/files/file/Morse/Archive/18%20WPM/'+name;items.append((name,url))
def get(item):
 name,url=item;p=dest/name
 if not p.exists():
  req=urllib.request.Request(url,headers={'User-Agent':'Mozilla/5.0'})
  with urllib.request.urlopen(req,timeout=60) as r:p.write_bytes(r.read())
 return {'file':name,'source':url,'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'role':'heldout' if name.startswith('260218') else 'additional_training_material_not_yet_used'}
with ThreadPoolExecutor(max_workers=3) as pool:rows=list(pool.map(get,items))
(root/'SOURCES.json').write_text(json.dumps(rows,indent=2)+'\n');print('downloaded',len(rows),flush=True)
from align_arrl import audit
out=root/'aligned';out.mkdir(exist_ok=True);audits=[]
for name,url in items:
 if name.endswith('.mp3'):
  a=audit(dest/name,dest/name.replace('18WPM.mp3','18.txt'),out);audits.append(a);print(name,a['known_symbol_match_fraction'],a['unresolved_alignment_regions'],flush=True)
(root/'ALIGNMENT_AUDIT.json').write_text(json.dumps(audits,indent=2)+'\n')

import json,collections,pathlib,hashlib
r=root;out=r/'prepared';out.mkdir(exist_ok=True);segments=[];summary=[]
for p in sorted((r/'aligned').glob('*.alignment-private.json')):
 d=json.loads(p.read_text());s=d['summary'];assert s['known_symbol_match_fraction']==1 and not s['unresolved_alignment_regions'];rows=d['aligned'];words=collections.defaultdict(list)
 for x in rows:words[x['word']].append(x)
 words=[words[k] for k in sorted(words)];bound=[0.]+[(words[i-1][-1]['end']+words[i][0]['start'])/2 for i in range(1,len(words))]+[s['duration_seconds']];i=0
 while i<len(words):
  j=i+1
  while j<len(words) and bound[j+1]-bound[i]<=20:j+=1
  tok=[];st=[]
  for k in range(i,j):
   if k>i:tok.append(' ')
   for x in words[k]:tok.append(x['symbol']);st.append({'token':x['symbol'],'word':k-i,'start':x['start']-bound[i],'end':x['end']-bound[i]})
  segments.append({'id':s['audio']+f':{i}-{j}','audio':'arrl/'+s['audio'],'audio_sha256':s['audio_sha256'],'group_id':'arrl-transition-'+s['audio'][:6],'split':'heldout' if s['audio'].startswith('260218') else 'training','start_seconds':bound[i],'end_seconds':bound[j],'wpm':s['wpm'],'tokens':tok,'symbols':st});i=j
 summary.append({k:s[k] for k in ['audio','audio_sha256','text_sha256','duration_seconds','known_symbol_match_fraction','unresolved_alignment_regions']})
(out/'segments-private.jsonl').write_text(''.join(json.dumps(x)+'\n' for x in segments));(out/'PUBLIC_MANIFEST.json').write_text(json.dumps({'files':summary,'roles':{'260107':'additional_training','260204':'additional_training','260218':'unseen_final_clean_recording'},'segments':len(segments)},indent=2)+'\n');print(len(segments),summary)
