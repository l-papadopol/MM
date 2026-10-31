#!/usr/bin/env python3
"""Report a frozen before/after timed-training comparison.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import csv
import json
from pathlib import Path
import numpy as np


def metrics(rows,engine):
    edits=np.array([r['edits'][engine] for r in rows]);lengths=np.array([len(r['reference']) for r in rows])
    rng=np.random.default_rng(27102026)
    indices=rng.integers(0,len(rows),(10000,len(rows)))
    boot=edits[indices].sum(1)/lengths[indices].sum(1)
    return {'cer':float(edits.sum()/lengths.sum()),'errors':int(edits.sum()),'characters':int(lengths.sum()),
            'exact':sum(r['reference']==r['decoded'][engine] for r in rows),'records':len(rows),
            'message_bootstrap_95_percent':[float(x*100) for x in np.quantile(boot,[.025,.975])]}


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--comparison',type=Path,required=True)
    ap.add_argument('--model',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    data=json.loads(args.comparison.read_text());card=json.loads((args.model/'MODEL_CARD.json').read_text())
    engines=list(data['details'][0]['decoded'])
    groups={'original':'Ordinari','receiver_filtered':'Filtri difficili','clean':'Puliti / lievemente degradati'}
    result={}
    for group in [*groups,'overall']:
        rows=[r for r in data['details'] if group=='overall' or r['group']==group]
        result[group]={k:metrics(rows,k) for k in engines}
    (args.out/'CW_NEURAL_R8_STATISTICS.json').write_text(json.dumps(result,indent=2)+'\n')
    lines=['# Training prolungato CW — R8','',
        'Confronto su audio sintetico nuovo, escluso da training, calibrazione e selezione dei checkpoint. CER sui caratteri, spazi inclusi.',
        f"Seed test: {data['seed']}. Il checkpoint distribuito è quello selezionato sui dati di sviluppo allo step {card['selected_step']}.",
        '','| Prova | Messaggi | Caratteri | '+' | '.join(engines)+' |',
        '| --- | ---: | ---: | '+' | '.join(['---:']*len(engines))+' |']
    for group in [*groups,'overall']:
        s=result[group];one=s[engines[0]]
        lines.append('| '+groups.get(group,'Complessivo')+f" | {one['records']} | {one['characters']} | "+
                     ' | '.join(f"{100*s[k]['cer']:.2f}%" for k in engines)+' |')
    widths=sorted({r['filter_width_hz'] for r in data['details'] if 'filter_width_hz' in r})
    lines+=['','## Larghezze dei filtri','',
            '| Filtro | Messaggi | '+' | '.join(engines)+' |','| --- | ---: | '+' | '.join(['---:']*len(engines))+' |']
    for width in widths:
        rows=[r for r in data['details'] if r.get('filter_width_hz')==width]
        values={k:metrics(rows,k) for k in engines}
        result['filter_'+str(width)]=values
        lines.append(f"| {width:g} Hz | {len(rows)} | "+' | '.join(f"{100*values[k]['cer']:.2f}%" for k in engines)+' |')
    lines+=['','## Messaggi lunghi e rumore','',
            '| Prova | '+' | '.join(engines)+' |','| --- | '+' | '.join(['---:']*len(engines))+' |',
            '| CER su 7 messaggi lunghi | '+' | '.join(f"{100*data['summary']['long_messages'][k]['cer']:.2f}%" for k in engines)+' |',
            '| Caratteri emessi su 6 minuti di negativi | '+' | '.join(str(data['summary']['negatives'][k]['characters']) for k in engines)+' |',
            '','## Protocollo','',
            'Il modello mantiene la stessa architettura e lo stesso frontend di R7. Il training aggiunge 8.900 esempi generati nuovi: 4.000 filtrati, 1.000 non filtrati, 1.500 puliti, 1.000 ordinari, 1.000 negativi e 400 messaggi lunghi casuali. Nessun output DeepCW o WebSDR è usato come etichetta.',
            'Durata impostata: 900 secondi del ciclo di ottimizzazione e validazione, oltre alla preparazione dei dati. A ogni controllo vengono calibrati sui dati di sviluppo due bias costanti, spazio e blank; si conserva il miglior checkpoint che rispetta i limiti di regressione sui segnali puliti/ordinari e sul rumore.',
            'La schedulazione del learning rate dipende dal tempo trascorso: l’esatta sequenza di passi può variare con hardware e carico. Seed, pesi selezionati, cronologia e intervalli dei dati sono distribuiti.',
            'Le statistiche includono intervalli bootstrap al 95%, ricampionando interi messaggi. Descrivono l’incertezza di questo corpus sintetico; non certificano una CER in radio.',
            'Il confronto con il nativo e con DeepCW ufficiale resta documentato nella revisione R7. Questa tabella misura il cambiamento di MM mantenendo identici gli ingressi per R7 e R8.',
            'I risultati reali WebSDR restano privi di trascrizioni verificate. Windows/macOS e sensibilità sulla radio reale restano da verificare.','']
    (args.out/'CW_NEURAL_R8.md').write_text('\n'.join(lines))
    (args.out/'CW_NEURAL_R8_STATISTICS.json').write_text(json.dumps(result,indent=2)+'\n')
    with (args.out/'CW_DECODER_COMPARISON_R8.csv').open('w',newline='') as f:
        writer=csv.writer(f);writer.writerow(['group','engine','records','characters','errors','cer_percent','exact_records','ci95_low_percent','ci95_high_percent'])
        for group,s in result.items():
            for k,v in s.items():writer.writerow([group,k,v['records'],v['characters'],v['errors'],v['cer']*100,v['exact'],*v['message_bootstrap_95_percent']])
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
