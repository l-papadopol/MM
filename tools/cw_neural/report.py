#!/usr/bin/env python3
"""Produce comparable-subset CER tables without hiding unsupported inputs.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import csv
import json
from pathlib import Path


def summarize(records,engines):
    chars=sum(len(r['reference']) for r in records)
    return {'records':len(records),'characters':chars,**{
        k:{'errors':sum(r['edits'][k] for r in records),
           'cer':sum(r['edits'][k] for r in records)/chars if chars else None,
           'exact':sum(r['reference']==r['decoded'][k] for r in records)} for k in engines}}


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--comparison',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True);args=ap.parse_args()
    data=json.loads(args.comparison.read_text());args.out.mkdir(parents=True,exist_ok=True)
    engines=[k for k in ['Native','DeepCW','R5','R6','R7'] if k in data['details'][0]['decoded']]
    labels={'original':'Messaggi ordinari','receiver_filtered':'Filtri ricevitore difficili','clean':'Segnali puliti / lievemente degradati'}
    common={}
    for group in labels:
        rows=[r for r in data['details'] if r['group']==group]
        common[group]=summarize([r for r in rows if all(r['decoded'][k] is not None for k in engines)],engines)
    (args.out/'COMMON_SUBSET.json').write_text(json.dumps(common,indent=2)+'\n')
    lines=['# Confronto decoder CW — R7','',
        'CER = (sostituzioni + cancellazioni + inserimenti) / caratteri di riferimento, spazi inclusi.',
        'La tabella principale usa gli stessi messaggi per tutti i motori. Ogni messaggio riparte a decoder azzerato.',
        'Native è il vero percorso CwDecoder di MadModem, con impostazioni predefinite e WPM automatici; DeepCW è il modello ufficiale eseguito esternamente.',
        'DeepCW usa il frontend Python ufficiale e finestre di 5–20 s: gli audio più corti sono completati con silenzio; quelli oltre 20 s sono esclusi dal sottoinsieme comune.',
        'Non vengono forniti al decoder testo atteso o velocità vera. Gli altri motori ricevono gli stessi campioni in blocchi di 997 campioni.',
        '','| Prova comune | Messaggi / caratteri | '+' | '.join(engines)+' |',
        '| --- | ---: | '+' | '.join(['---:']*len(engines))+' |']
    for group,name in labels.items():
        s=common[group];lines.append('| '+name+f" | {s['records']} / {s['characters']} | "+' | '.join(f"{100*s[k]['cer']:.2f}%" for k in engines)+' |')
    lines+=['','Messaggi interamente corretti, nello stesso ordine dei motori della tabella:','']
    for group,name in labels.items():
        s=common[group];lines.append('- '+name+': '+', '.join(f"{k} {s[k]['exact']}/{s['records']}" for k in engines)+'.')
    lines+=['','## Tutti i record, comprese le finestre non supportate da DeepCW','',
            '| Prova | Motore | Messaggi valutati | CER |','| --- | --- | ---: | ---: |']
    for group,name in labels.items():
        for k in engines:
            s=data['summary'][group][k]
            lines.append(f"| {name} | {k} | {s['supported_records']}/{data['summary'][group]['records']} | {100*s['cer']:.2f}% |")
    lines+=['','## Rumore e portanti non manipolate','',
            '36 registrazioni da 10 s (6 minuti totali), larghezze 70–1.200 Hz e rumore a banda larga; alcune contengono una portante continua. Qui si contano i caratteri falsi, non una CER.','',
            '| Motore | Caratteri falsi | Registrazioni con testo |','| --- | ---: | ---: |']
    for k in engines:
        s=data['summary']['negatives'][k];lines.append(f"| {k} | {s['characters']} | {s['nonempty_records']}/36 |")
    lines+=['','## Messaggi lunghi','',
            '| Motore | Messaggi supportati | CER |','| --- | ---: | ---: |']
    for k in engines:
        s=data['summary']['long_messages'][k];v=f"{100*s['cer']:.2f}%" if s['cer'] is not None else 'N/D'
        lines.append(f"| {k} | {s['supported_records']}/7 | {v} |")
    lines+=['','## Limiti','',
        f"Test congelato con seed {data['seed']}; 120 messaggi per ciascuno dei tre gruppi, campionamenti 8/16/44,1/48 kHz, velocità 12–36 WPM.",
        'I nuovi parametri R7 sono stati scelti esclusivamente sui precedenti insiemi di sviluppo, prima di generare questo test. Si modificano due bias costanti del modello MM: spazio +5, blank +1,5. Nessun dizionario o correzione dei nominativi.',
        'Le prove sono sintetiche e riflettono il generatore distribuito. La CER su radio reale non è misurata: le due registrazioni WebSDR non hanno trascrizioni verificate e non sono state usate come etichette.',
        'Un risultato sotto il 5% in un gruppo non certifica quel valore per tutte le condizioni. I casi difficili, i messaggi lunghi e i falsi sul rumore restano visibili.',
        'DeepCW resta esterno al prodotto. Il suo modello e il codice ufficiale non sono redistribuiti nel pacchetto MM né usati come insegnante per addestrarlo.',
        'Dettagli per messaggio, hash degli eseguibili e del modello ufficiale sono nel JSON completo.','']
    (args.out/'CW_NEURAL_R7.md').write_text('\n'.join(lines))
    with (args.out/'CW_DECODER_COMPARISON_R7.csv').open('w',newline='') as f:
        writer=csv.writer(f);writer.writerow(['scope','group','engine','records','characters','errors','cer_percent','exact_records'])
        for group,s in common.items():
            for k in engines:writer.writerow(['common',group,k,s['records'],s['characters'],s[k]['errors'],100*s[k]['cer'],s[k]['exact']])
        for group in labels:
            for k in engines:
                s=data['summary'][group][k]
                writer.writerow(['all_supported',group,k,s['supported_records'],s['characters'],s['errors'],100*s['cer'],s['exact_records']])
    print(json.dumps(common,indent=2))


if __name__=='__main__':main()
