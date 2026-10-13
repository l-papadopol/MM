# MadModem

## 🇮🇹 Italiano

**MadModem (MM)** è un programma libero per radioamatori che riunisce in un'unica applicazione modi digitali, waterfall, controllo della radio, rotore e logbook.

Il progetto nasce da un'esigenza molto semplice: quando faccio radio non voglio avere cinque o sei programmi aperti per decodificare un segnale, comandare il ricetrasmettitore, muovere le antenne e registrare un QSO. Volevo un ambiente unico, pratico e soprattutto aperto, che potessi modificare e sperimentare senza dipendere da software chiuso.

Mi chiamo **Lucian-Ioan Papadopol, IZ6NNH**. Sono radioamatore, appassionato di elettronica e informatica, e MadModem è nato prima di tutto per la mia stazione e per il piacere di sperimentare. Con il tempo il progetto è cresciuto parecchio e ho deciso di renderlo disponibile a chiunque abbia voglia di provarlo, usarlo e contribuire.

### Cosa c'è dentro

MadModem comprende oggi **FT8, FT4, MSK144, Q65, CW, RTTY, BPSK/QPSK, MFSK, Feld Hell, SSTV e WEFAX**, con ricezione e trasmissione secondo lo stato di sviluppo delle singole modalità.

Attorno ai modem c'è tutto quello che normalmente serve in stazione: **waterfall**, gestione audio RX/TX, **CAT e PTT tramite Hamlib**, controllo dei **rotori**, logbook **ADIF**, informazioni DXCC, mappa dei QSO, macro e strumenti per contest RTTY. È presente anche una modalità **Radio Telescope** per effettuare scansioni del cielo con un sistema antenna/rotore.

L'idea non è quella di mettere insieme una collezione di finestre indipendenti: radio, segnali, decoder, TX, log e controlli di stazione devono lavorare nello stesso ambiente e rimanere a portata di mano durante il QSO.

MM è scritto in **C++/Qt**, è pensato per **Linux, Windows e macOS** e il codice sorgente è pubblicato sotto licenza **GNU GPL v3**.

### Installazione e aggiornamenti — 0.6.1

**Installer e pacchetti portabili rimangono entrambi disponibili. L'installazione è facoltativa.**

| Sistema | Senza installazione | Installer |
| --- | --- | --- |
| Windows 10/11 a 64 bit | ZIP separati legacy e AVX2/FMA; estrarre e avviare `bin/MadModem.exe`. Nel dubbio scegliere legacy. | Un unico `Setup.exe` sceglie automaticamente la variante compatibile con CPU e sistema operativo. |
| Ubuntu 24.04 / distribuzioni compatibili | ZIP o TAR.GZ Ubuntu24.04; avviare `AppRun`. | `Ubuntu24.04-…-Setup.run`, con DEB e dipendenze gestite da APT. |
| Fedora 43 | ZIP o TAR.GZ Fedora43; avviare `AppRun`. | `…fc43…-Setup.run`, con RPM e dipendenze gestite da DNF. |
| macOS 15 o successivo | ZIP dell'app o DMG, dedicati a Intel oppure Apple Silicon. | `unsigned.pkg` dedicato all'architettura. |

Gli archivi Linux includono Qt5, Hamlib, i plugin Qt, i moduli QML necessari e una configurazione Fontconfig portabile. Non serve installare separatamente Qt o Hamlib. Scegliere il pacchetto della propria distribuzione: glibc, driver grafici e servizi desktop/audio restano quelli del sistema. Per aggiornare una copia portabile, chiudere MM e conservare `settings.mad`, logbook ADIF e registrazioni prima di sostituire i file del programma.

Gli installer mostrano il copyright di **Papadopol Lucian-Ioan** e la **GNU GPLv3**. La procedura interattiva richiede l'accettazione della licenza. Su Linux avviare `bash nome-del-file-Setup.run`: viene mostrata la licenza in una finestra, se Zenity è disponibile, oppure nel terminale.

Per aggiornare, chiudere MM ed eseguire il nuovo installer: viene aggiornata la stessa installazione. Preferenze e logbook delle installazioni gestite risiedono nelle cartelle dell'utente. Se si passa da una vecchia copia portabile conservata altrove, seguire le [istruzioni di migrazione](docs/platform/INSTALLERS.md) prima di rimuoverla.

Le finestre si ridimensionano trascinando bordi o angoli; i contenuti mantengono una dimensione minima utilizzabile. L'editor SSTV permette di scorrere gli strumenti anche su schermi più piccoli.

La suite completa di regressione si avvia solo manualmente da **Actions → Full regression suite (manual)**. La pipeline dei pacchetti compila l’app e verifica dipendenze, avvio e installer senza ricompilare ed eseguire l’intera suite.

I pacchetti vengono creati dal workflow GitHub **Build installers**, automaticamente su `main`/`master` e tag `v*`, oppure con **Actions → Build installers → Run workflow**. Scaricarli dagli artifact della compilazione completata: `portable-*` per gli archivi, `installer-*` per gli installer. Gli installer Windows e macOS non hanno ancora una firma commerciale/Developer ID; il sistema operativo può quindi mostrare un avviso.

### Stato del progetto

MadModem è alla versione **0.6.1**. Viene usato e provato realmente in radio, ma diverse parti sono tuttora in sviluppo e possono cambiare rapidamente. Segnalazioni di bug, prove con radio e configurazioni differenti e contributi al codice sono quindi benvenuti.

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

MM is written in **C++/Qt**, targets **Linux, Windows and macOS**, and its source code is released under the **GNU GPL v3** licence.

### Installation and updates — 0.6.1

**Installers and portable packages are both available. Installation is optional.**

| Platform | Without installation | Installer |
| --- | --- | --- |
| Windows 10/11, 64-bit | Separate legacy and AVX2/FMA ZIPs; extract and run `bin/MadModem.exe`. Choose legacy if unsure. | One `Setup.exe` selects the variant supported by the CPU and OS. |
| Ubuntu 24.04 / compatible distributions | Ubuntu24.04 ZIP or TAR.GZ; run `AppRun`. | `Ubuntu24.04-…-Setup.run`, backed by DEB and APT. |
| Fedora 43 | Fedora43 ZIP or TAR.GZ; run `AppRun`. | `…fc43…-Setup.run`, backed by RPM and DNF. |
| macOS 15 or later | App ZIP or DMG, separately for Intel and Apple Silicon. | Architecture-specific `unsigned.pkg`. |

Linux archives bundle Qt5, Hamlib, required Qt plugins, QML modules and a portable Fontconfig configuration. No separate Qt or Hamlib installation is needed. Choose the matching distribution build: glibc, graphics drivers and desktop/audio services remain provided by the host. Before updating a portable copy, close MM and preserve `settings.mad`, ADIF logbooks and recordings when replacing program files.

Installers display **Papadopol Lucian-Ioan's copyright** and the **GNU GPLv3**. Interactive installation requires license acceptance. On Linux, run `bash installer-filename-Setup.run`; the license opens in a window when Zenity is available, otherwise in the terminal.

To update, close MM and run the new installer. It updates the existing installation while keeping managed-install preferences and logbooks in user folders. When switching from a portable copy stored elsewhere, follow the [migration instructions](docs/platform/INSTALLERS.md) before removing it.

Resize windows by dragging their edges or corners. Layouts retain a usable minimum size, and SSTV editing tools remain scrollable on smaller displays.

The full regression suite runs only on demand through **Actions → Full regression suite (manual)**. Package builds retain dependency, startup and installer checks without compiling or running the full regression suite.

GitHub's **Build installers** workflow creates packages on pushes to `main`/`master`, on `v*` tags, or through **Actions → Build installers → Run workflow**. Download the completed run's `portable-*` artifacts for archives or `installer-*` artifacts for installers. Windows and macOS installers do not yet have commercial/Developer ID signatures, so operating-system warnings may appear.

### Project status

MadModem is at version **0.6.1**. It is actually used and tested on the air, but several parts are still under development and may change quickly. Bug reports, tests with different radios and station configurations, and code contributions are therefore very welcome.

Build instructions, configuration notes and technical information are available in the [`docs`](docs/README.md) directory.

**73 de IZ6NNH**  
Lucian-Ioan Papadopol

---

Copyright (C) 2026 Papadopol Lucian-Ioan <l.i.papadopol@gmail.com>.
Released under [GNU GPLv3](COPYING). See [third-party notices](THIRD_PARTY_NOTICES.md).
