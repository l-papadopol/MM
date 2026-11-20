# MadModem 0.6.5 — edizione IC8TEM

> **Edizione IC8TEM** di MadModem, a cura di **Costantino Cerrotta, IC8TEM**: il programma
> di IZ6NNH con in più le funzioni che IC8TEM ha sviluppato per la sua versione di MSHV
> ([MSHV-IC8TEM](https://github.com/ic8tem/MSHV-IC8TEM)). Le modifiche sono in commit
> separati, a disposizione dell'autore. Scaricate questa edizione dalle
> [Releases del fork](https://github.com/ic8tem/MM/releases); il programma originale è su
> [l-papadopol/MM](https://github.com/l-papadopol/MM).
>
> **IC8TEM edition** of MadModem by Costantino Cerrotta, IC8TEM: IZ6NNH's program plus the
> features IC8TEM developed for MSHV-IC8TEM. Downloads: [fork releases](https://github.com/ic8tem/MM/releases).

| Aggiunta / Addition | Dove / Where |
| --- | --- |
| **Noise blanker**: toglie gli impulsi (QRN, scintille, alimentatori switching) dall'audio prima di ogni decodificatore. NB2 alla NR0V (consigliato) o predizione lineare DL2FW, tre soglie; ping MSK144, CW manipolato e toni FT/Q65 mai toccati. / Impulse blanker before every decoder, NR0V NB2 style or DL2FW linear prediction. | Decode › Noise Blanker |
| **Notch delle portanti MSK144 (ZAP)**: portanti fisse e birdie tolti con notch di 10 Hz; un segnale MSK continuo non viene mai filtrato. / MSK144 carrier notch. | Decode › MSK144 Carrier Notch (ZAP) |
| **Potenza e ROS**: potenza RF via CAT Hamlib con preset, −1/+1 e memoria per banda; misure Po, ROS e ALC in trasmissione; Tune a potenza ridotta; protezione ROS. / RF power, Po/SWR/ALC meters, tune power and SWR protection through Hamlib. | Tools › Power and SWR |
| **Fonti del locatore**: locatore del DX dalle decodifiche ricevute, da CALL3.TXT (WSJT-X Improved, W7GJ EME), da JTDX o online (G7VRD), con l'indicazione della fonte. / Locator lookup for the DX call. | Tools › Locator sources |
| **Utenti LoTW**: lista ARRL, pallino accanto agli utenti LoTW nella lista FT e filtro "solo utenti LoTW". / LoTW users marker and FT filter. | Tools › LoTW users |
| **FT: richiama il DX quando viene decodificato**: dopo lo stop la trasmissione riparte se il DX torna a farsi sentire (massimo 3 volte). / FT call-again. | Tools |
| **Reset fittizio del log**: i QSO prima di una data non contano per già lavorato, nuovo paese e nuovo quadrato. / Fictitious log reset for worked-before marks. | Tools |
| **Durata del ping MSK144**: colonna "ms" nella lista MSK144. / MSK144 ping duration column. | MSK144 |
| **Tema YA-NEC**: tema chiaro con lo stile di YA-NEC (barre blu scuro, pulsanti a pillola, riquadri bianchi). / Light theme in the YA-NEC style. | Settings › Logbook / FT colours › Theme |
| **Split anche in MSK144 e Q65**: audio TX tra 1500 e 2000 Hz e frequenza TX spostata a passi di 500 Hz, come in WSJT-X/JTDX (Rig o Fake It). / Split Operation for MSK144 and Q65 too. | Settings › Audio / PTT / CAT › Split operation |
| Aggiornamenti automatici da questo fork. / Updates come from this fork. | Help |

**MadModem (MM)** è un'applicazione libera per radioamatori: modi digitali, audio,
waterfall, controllo della radio e del rotore, logbook e mappe in un unico programma.
Sviluppato da **Lucian-Ioan Papadopol, IZ6NNH** — [madexp.it](https://www.madexp.it).

**MadModem** is free amateur-radio software combining digital modes, a waterfall,
CAT/PTT, antenna rotator control and an ADIF logbook in one Qt desktop application.

## Funzioni / Features

- FT8, FT4, MSK144, Q65, CW, RTTY, BPSK/QPSK, MFSK, Feld Hell, SSTV e WEFAX.
- FT8/FT4 QSO sequencer, CQ queues, Evil Auto QSO and Doppler correction.
- CW native/neural receivers, CW/RTTY skimmers, audio CW and serial keying.
- Hamlib CAT/PTT, rotator tracking, station/DX information, QSO maps and UTC scheduler.
- ADIF logging, contest tools, macro messages, WAV recording and offline analysis.
- Help and interface in English, Italian, French, German, Norwegian and Czech.

## Download e installazione / Downloads

Scaricare il pacchetto dalla pagina [Releases](https://github.com/ic8tem/MM/releases) (edizione IC8TEM).

| Sistema / System | Download |
| --- | --- |
| Windows x64 | AutoCPU EXE installer; Compatible or AVX2/FMA portable ZIP |
| Ubuntu 24.04 / Mint 22.x x64 | RUN, DEB, portable TAR.GZ/ZIP |
| Fedora 43 x64 | RUN, RPM, portable TAR.GZ/ZIP |
| macOS 15+ Intel / Apple Silicon | PKG, app ZIP, DMG |

Select the package for your system and processor. Linux packages target the named
base distribution. Portable Linux: extract into a writable folder and run `AppRun`.
Installation details and user-data paths: [Installation](docs/INSTALLATION.md).

## Primo avvio / First run

1. Enter your callsign and locator in Settings → User/QTH.
2. Configure audio devices, CAT and PTT; check radio control and RX before TX.
3. Select a mode, start RX and adjust the input level without clipping.
4. For FT8/FT4, synchronize UTC. Check the selected correspondent and TX message.
5. Open the built-in Help for mode-specific instructions. Stop cancels TX and automation.

The title shows both version and build. Evil automation decisions are recorded in
Runtime log; complete compressed session logging is available in Settings → Diagnostics.
See [Troubleshooting](docs/TROUBLESHOOTING.md).

## Sorgenti / Source

- [Build instructions](docs/BUILDING.md)
- [GitHub distribution workflow](docs/DISTRIBUTION.md)
- [Documentation](docs/README.md) · [Release notes](RELEASE_NOTES.md)

Licensed under GNU GPLv3; see [COPYING](COPYING), [LICENSE.md](LICENSE.md),
[authors](AUTHORS.md) and [third-party notices](THIRD_PARTY_NOTICES.md).
