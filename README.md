# MadModem

## 🇮🇹 Italiano

**MadModem (MM)** è un programma libero per radioamatori che riunisce in un'unica applicazione modi digitali, waterfall, controllo della radio, rotore e logbook.

Il progetto nasce da un'esigenza molto semplice: quando faccio radio non voglio avere cinque o sei programmi aperti per decodificare un segnale, comandare il ricetrasmettitore, muovere le antenne e registrare un QSO. Volevo un ambiente unico, pratico e soprattutto aperto, che potessi modificare e sperimentare senza dipendere da software chiuso.

Mi chiamo **Lucian-Ioan Papadopol, IZ6NNH**. Sono radioamatore, appassionato di elettronica e informatica, e MadModem è nato prima di tutto per la mia stazione e per il piacere di sperimentare. Con il tempo il progetto è cresciuto parecchio e ho deciso di renderlo disponibile a chiunque abbia voglia di provarlo, usarlo e contribuire.

### Cosa c'è dentro

MadModem comprende oggi **FT8, FT4, MSK144, Q65, CW, RTTY, BPSK/QPSK, MFSK, Feld Hell, SSTV e WEFAX**, con ricezione e trasmissione secondo lo stato di sviluppo delle singole modalità.

Attorno ai modem c'è tutto quello che normalmente serve in stazione: **waterfall**, gestione audio RX/TX, **CAT e PTT tramite Hamlib**, controllo dei **rotori**, logbook **ADIF**, informazioni DXCC, mappa dei QSO, macro e strumenti per contest RTTY. È presente anche una modalità **Radio Telescope** per effettuare scansioni del cielo con un sistema antenna/rotore.

L'idea non è quella di mettere insieme una collezione di finestre indipendenti: radio, segnali, decoder, TX, log e controlli di stazione devono lavorare nello stesso ambiente e rimanere a portata di mano durante il QSO.

MM è scritto in **C++/Qt**, è pensato per **Linux e Windows** e il codice sorgente è pubblicato sotto licenza **GNU GPL v3**.

### Stato del progetto

MadModem è ancora in versione **alpha**. Viene usato e provato realmente in radio, ma diverse parti sono tuttora in sviluppo e possono cambiare rapidamente. Segnalazioni di bug, prove con radio e configurazioni differenti e contributi al codice sono quindi benvenuti.

Per compilazione, configurazione e dettagli tecnici trovate la documentazione nella cartella [`docs`](docs/README.md).

**73 de IZ6NNH**  
Lucian-Ioan Papadopol

---

## 🇬🇧 English

**MadModem (MM)** is free software for amateur radio that brings digital modes, waterfall, radio control, rotator control and logging together in a single application.

The project started from a very simple need: when I am on the radio I don't want five or six different programs open just to decode a signal, control the transceiver, move the antennas and log a QSO. I wanted one practical and open environment that I could modify and experiment with without depending on closed software.

My name is **Lucian-Ioan Papadopol, IZ6NNH**. I am a radio amateur with a passion for electronics and computing, and MadModem was originally created for my own station and for the fun of experimenting. Over time it grew considerably, so I decided to make it available to anyone who wants to try it, use it or contribute to it.

### What's inside

MadModem currently includes **FT8, FT4, MSK144, Q65, CW, RTTY, BPSK/QPSK, MFSK, Feld Hell, SSTV and WEFAX**, with receive and transmit support according to the current development status of each mode.

Around the modems there is the rest of what is normally useful in a station: a **waterfall**, RX/TX audio management, **CAT and PTT through Hamlib**, **rotator** control, an **ADIF** logbook, DXCC information, QSO mapping, macros and tools for RTTY contest operation. A **Radio Telescope** mode is also included for sky scans using an antenna/rotator system.

The idea is not to build a collection of unrelated windows. Radio control, signals, decoders, TX, logging and station controls should work together and remain immediately accessible while making a QSO.

MM is written in **C++/Qt**, targets **Linux and Windows**, and its source code is released under the **GNU GPL v3** licence.

### Project status

MadModem is still **alpha software**. It is actually used and tested on the air, but several parts are still under development and may change quickly. Bug reports, tests with different radios and station configurations, and code contributions are therefore very welcome.

Build instructions, configuration notes and technical information are available in the [`docs`](docs/README.md) directory.

**73 de IZ6NNH**  
Lucian-Ioan Papadopol
