# MadModem 0.6.6

**MadModem (MM)** è un'applicazione libera per radioamatori: modi digitali, audio,
waterfall, controllo radio e rotore, logbook ADIF e mappe in un unico programma Qt.
Sviluppato da **Lucian-Ioan Papadopol, IZ6NNH** — [madexp.it](https://www.madexp.it).

Free amateur-radio software combining digital modes, CAT/PTT, antenna rotator
control and logging. Interface and built-in help are available in six languages.

## Funzioni

- FT8, FT4, MSK144, Q65, CW, RTTY, BPSK/QPSK, MFSK, Feld Hell, SSTV e WEFAX.
- Sequencer FT8/FT4, code CQ con priorità, Evil Auto QSO e correzione Doppler.
- Noise blanker RX NB2 per FT8, FT4, CW, RTTY e altri modi, e notch delle portanti MSK144; durata dei ping.
- CAT/PTT Hamlib, split Rig/Fake It, potenza RF per banda e strumenti Po/ROS/ALC.
- Ricevitori CW nativo e neurale, skimmer CW/RTTY, manipolazione audio e seriale.
- Logbook ADIF, strumenti contest, fonti del locatore, indicatori e filtro LoTW.
- Mappe QSO, inseguimento rotore, scheduler UTC, macro, registrazione e analisi WAV.
- Temi grafici, incluso YA-NEC; log operativo e diagnostica di sessione compressa.

I contributi di **Costantino Cerrotta, IC8TEM** sono integrati in MadModem.
Funzioni e comandi sono descritti nella [guida agli strumenti](docs/FEATURES.md).
Crediti completi in [AUTHORS.md](AUTHORS.md).

## Benchmark FT8 e FT4 della R10

Il confronto comprende **1.988 esecuzioni offline**, su **136 WAV di IC8TEM**
e **148 periodi sintetici** per ciascuno dei sette decoder. Il banco fattoriale
incrocia rumore impulsivo, QSB globale e selettivo, sovrapposizioni e Doppler
in 32 combinazioni per modo. Il test Mega combina tutti i disturbi con 48
portanti per periodo. Per MadModem sono attivi Auto-NB2 e, nei sintetici VHF,
la correzione Doppler.

Le colonne dei sintetici mostrano **messaggi corretti / falsi**, su 768
trasmissioni per modo nel fattoriale e 288 nel Mega. La colonna dei 40 m
mostra i messaggi con riscontro nei log di IC8TEM e quanti dei sei sospetti
già verificati sull'audio sono riemersi.

| Decoder | FT8 fattoriale | FT4 fattoriale | FT8 Mega | FT4 Mega | IC8TEM FT8 40 m, riscontri / sospetti su 6 |
| --- | ---: | ---: | ---: | ---: | ---: |
| MadModem R7 | 303 / 2 | 236 / 0 | 7 / 0 | 3 / 0 | 2.152 / 5 |
| MadModem R8 | 304 / 2 | 236 / 0 | 7 / 0 | 3 / 0 | 2.155 / 4 |
| MadModem R9 | 303 / 1 | 236 / 0 | 7 / 0 | 3 / 0 | 2.153 / 2 |
| **MadModem R10** | **303 / 1** | **236 / 0** | **7 / 0** | **3 / 0** | **2.153 / 0** |
| JTDX | 283 / 0 | 176 / 0 | 3 / 0 | 0 / 0 | 2.076 / 2 |
| MSHV | 242 / 0 | 186 / 0 | 3 / 1 | 0 / 0 | 2.160 / 2 |
| WSJT-X | 241 / 0 | 183 / 0 | 3 / 0 | 0 / 0 | 2.115 / 2 |

Nei 40 m la R10 mantiene i 2.153 riscontri FT8 della R9 e scarta entrambi
i sospetti che la R9 riportava. I log sul traffico reale offrono una copertura
parziale. Nel fattoriale FT8 la R10 supera JTDX di 20 messaggi corretti e
produce un falso. La R10 richiede in media 5,11 s di CPU per periodo FT8 e
1,22 s per FT4, mentre JTDX richiede 2,95 s e 0,42 s.

Versioni dei decoder, condizioni, tempi e risultati per periodo sono nel
[foglio completo del benchmark FT8/FT4](docs/benchmarks/MadModem_Benchmark_FT8_FT4.xlsx).
Le registrazioni sono disponibili nel [repository di IC8TEM](https://github.com/ic8tem/MadModem-test-recordings/tree/47b3538d2f997f878811986ba599b8e72ab10631).

## Download e installazione

Scaricare i pacchetti da [GitHub Releases](https://github.com/l-papadopol/MM/releases).
Anche il controllo aggiornamenti del programma utilizza questo repository.

| Sistema | Installer | Portabile |
| --- | --- | --- |
| Windows x64 | EXE AutoCPU | ZIP Compatible o AVX2/FMA |
| Ubuntu 24.04 / Mint 22.x x64 | RUN, DEB | TAR.GZ, ZIP |
| Fedora 43 x64 | RUN, RPM | TAR.GZ, ZIP |
| macOS 15+ Intel / Apple Silicon | PKG | App ZIP, DMG |

Su Windows l'installer usa il nome **MadModem**, il percorso predefinito
`%LOCALAPPDATA%\Programs\MadModem` e le cartelle dati personali di sistema.
Percorsi, aggiornamenti e passaggio da precedenti installazioni:
[installazione](docs/INSTALLATION.md).

I pacchetti Linux richiedono una base compatibile con quella indicata.
Per la versione portabile Linux, estrarre l'archivio e avviare `AppRun`.

## Primo avvio

1. Inserire nominativo e locatore in Settings → User/QTH.
2. Configurare audio, CAT e PTT; verificare ricezione e controllo radio.
3. Selezionare il modo, avviare RX e regolare il livello senza saturazione.
4. Per FT8/FT4 sincronizzare UTC e controllare interlocutore e messaggio TX.
5. Consultare l'Aiuto integrato per i singoli modi e le automazioni.

Il titolo mostra versione e build. Le decisioni di Evil Auto QSO sono consultabili
nel Runtime log; la diagnostica completa si abilita in Settings → Diagnostics.
Consultare [risoluzione dei problemi](docs/TROUBLESHOOTING.md).

## Sorgenti e documentazione

- [Compilazione](docs/BUILDING.md)
- [Pacchetti e workflow GitHub](docs/DISTRIBUTION.md)
- [Documentazione](docs/README.md) · [Novità](RELEASE_NOTES.md)

Licenza GNU GPLv3: [COPYING](COPYING), [LICENSE.md](LICENSE.md) e
[attribuzioni dei componenti](THIRD_PARTY_NOTICES.md).
