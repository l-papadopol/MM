#!/usr/bin/env python3
"""Report complete-message and continuous-QSO evidence without mixed denominators.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse,csv,json
from pathlib import Path

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--comparison',type=Path,required=True)
    ap.add_argument('--holdout',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    d=json.loads(args.comparison.read_text());h=json.loads(args.holdout.read_text())
    engines=['Native','R7','R8'];names={'clean':'Puliti','receiver_300hz':'Filtro 300 Hz','receiver_100hz':'Filtro 100 Hz','continuous_qso':'QSO continui, pause di 3 s'}
    lines=['# Prove CW con messaggi completi e QSO','',
      'Le prove includono `CQ CQ CQ DE IZ6NNH IZ6NNH PSE K`, `UR 599 599 BK`, risposte con nominativi ripetuti, NAME/QTH, rapporti 559/5NN e chiusure TNX QSO 73 SK.',
      '72 messaggi: sei sequenze a 12, 20, 28 e 36 WPM in tre condizioni. Altri tre test mantengono il decoder attivo durante un QSO intero, con quattro passaggi a velocità diverse e pause di tre secondi.',
      'Nessuna velocità o testo di riferimento viene fornito ai decoder. Il nativo usa l’acquisizione automatica standard. CER sui caratteri, spazi inclusi.','',
      '| Prova | Numero | Nativo | MM R7 | MM R8 |','| --- | ---: | ---: | ---: | ---: |']
    for group,name in names.items():
        s=d['summary'][group];lines.append(f"| {name} | {s['Native']['records']} | "+' | '.join(f"{100*s[k]['cer']:.2f}%" for k in engines)+' |')
    lines+=['','## Messaggi, nominativi e rapporti esatti','',
       '| Prova | Motore | Messaggi esatti | Nominativi esatti | Rapporti esatti |','| --- | --- | ---: | ---: | ---: |']
    for group,name in names.items():
        for k in engines:
            s=d['summary'][group][k]
            lines.append(f"| {name} | {k} | {s['exact']}/{s['records']} | {s['calls_correct']}/{s['calls_total']} | {s['reports_correct']}/{s['reports_total']} |")
    lines+=['','Un nominativo o rapporto si conta una volta per ciascuna occorrenza attesa. La valutazione allinea le parole conservandone l’ordine: parole unite e ripetizioni perse contano come errori, anche quando una parte dei caratteri è corretta.','',
        '## DeepCW ufficiale: sottoinsieme comune','',
        'L’esempio ufficiale accetta finestre di 5–20 s. Sono supportati 45 dei 72 messaggi; le sequenze più lente/lunghe e i tre QSO continui restano fuori dal suo limite. Per questo confronto tutti i motori usano esattamente gli stessi 45 messaggi.','',
        '| Motore | Messaggi | CER |','| --- | ---: | ---: |']
    common=[r for r in d['details'] if r['decoded'].get('DeepCW') is not None];characters=sum(len(r['reference']) for r in common)
    for k in [*engines,'DeepCW']:
        errors=sum(r['metrics'][k]['errors'] for r in common)
        lines.append(f'| {k} | {len(common)} | {100*errors/characters:.2f}% |')
    lines+=['','## Esempi a 20 WPM, audio pulito','']
    for r in d['details']:
        if r.get('condition')=='clean' and r.get('wpm')==20 and r['id'] in ['cq','rst']:
            lines+=['Atteso: `'+r['reference']+'`','']
            for k in [*engines,'DeepCW']:lines.append('- '+k+': `'+str(r['decoded'][k])+'`')
            lines.append('')
    lines+=['## QSO nuovi con semi disgiunti e nome/QTH distinti','',
        'I messaggi fissi sopra sono prove funzionali richieste dall’utente e hanno evidenziato il limite della prima fase. Per verificare la generalizzazione si aggiunge un test nuovo con semi disgiunti e un vocabolario NAME/QTH distinto da training e sviluppo. Questo test non sceglie i pesi.','',
        '| Motore | Messaggi | CER | Messaggi esatti | Nominativi esatti | Rapporti esatti |','| --- | ---: | ---: | ---: | ---: | ---: |']
    s=h['summary']['all']
    for k in ['R7','R8_A','R8']:
        if k not in s:continue
        v=s[k];lines.append(f"| {k} | {s['records']} | {100*v['cer']:.2f}% | {v['exact']}/{s['records']} | {v['calls_correct']}/{v['calls_total']} | {v['reports_correct']}/{v['reports_total']} |")
    lines+=['','R8_A indica il candidato dopo i primi 15 minuti; R8 indica i pesi effettivamente consegnati. Se la seconda fase non supera la selezione, i due modelli coincidono.',
      'Tutte le prove sono audio generato con riferimento noto. Le registrazioni WebSDR restano prive di trascrizione verificata: non esiste ancora una CER misurata sulla radio reale. Risultati completi e testi decodificati sono nei JSON allegati.','']
    if 'call_identity_audit' in h:
        audit=h['call_identity_audit']
        lines+=['Audit dei nominativi generati: '+str(audit['unique_test_calls'])+' distinti nel test; '+str(audit['overlap_with_discarded_qso_training'])+' compare anche nel corpus della fase QSO scartata. Quella fase non contribuisce ai pesi consegnati. I messaggi completi e i semi restano distinti; non si dichiara una separazione assoluta di tutte le identità dei nominativi.','']
    (args.out/'CW_QSO_R8.md').write_text('\n'.join(lines))
    with (args.out/'CW_QSO_COMPARISON_R8.csv').open('w',newline='') as f:
        writer=csv.writer(f);writer.writerow(['condition','engine','records','characters','cer_percent','exact_messages','correct_calls','total_calls','correct_reports','total_reports'])
        for group in names:
            for k in engines:
                s=d['summary'][group][k];writer.writerow([group,k,s['records'],s['characters'],100*s['cer'],s['exact'],s['calls_correct'],s['calls_total'],s['reports_correct'],s['reports_total']])
    print(args.out/'CW_QSO_R8.md')

if __name__=='__main__':main()
