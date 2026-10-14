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

### Skimmer CW e RTTY

Nel pannello CW, abilita **Skimmer** e scegli **Testo** oppure **Solo CQ**. Lo skimmer decodifica fino a 16 portanti indipendenti nella banda audio ricevuta da 100 a 3500 Hz; non riceve le frequenze escluse dal filtro della radio. Il testo scorre in colonne accanto alle portanti. La vista Solo CQ mostra un nominativo dopo due letture concordanti nel contesto di una chiamata CQ. Per segnali molto vicini usa lo zoom del waterfall: le colonne che non trovano spazio vengono indicate, senza sovrapporre il testo. I ricevitori manuali RX A e RX B restano disponibili.

In **RTTY** trovi gli stessi comandi Skimmer e Testo/Solo CQ. Il controllo **Canali** sceglie 2–16 decoder indipendenti (12 iniziali), ciascuno con stato lettere/cifre proprio. Tutte le coppie usano baud, shift e polarità del preset selezionato. Lo skimmer segue le coppie senza modificare lo shift; il terminale e il QSO rimangono legati alla stazione scelta manualmente. Nessun testo skimmer avvia TX o registra automaticamente contatti.

### Installazione e aggiornamenti — 0.6.2

**Installer e pacchetti portabili rimangono entrambi disponibili. L'installazione è facoltativa.**

| Sistema | Senza installazione | Installer |
| --- | --- | --- |
| Windows 10/11 a 64 bit | ZIP separati `Portable-Compatible` e `Portable-AVX2-FMA`; estrarre e avviare `bin/MadModem.exe`. Nel dubbio scegliere `Compatible`. | Un unico `Installer-AutoCPU.exe` sceglie automaticamente la variante compatibile con CPU e sistema operativo. |
| Ubuntu 24.04 / distribuzioni compatibili | ZIP o TAR.GZ Ubuntu24.04; avviare `AppRun`. | `Linux-Ubuntu24.04-64bit-Installer.run`, con DEB e dipendenze gestite da APT. |
| Fedora 43 | ZIP o TAR.GZ Fedora43; avviare `AppRun`. | `Linux-Fedora43-64bit-Installer.run`, con RPM e dipendenze gestite da DNF. |
| macOS 15 o successivo | ZIP dell'app o DMG, dedicati a Intel oppure Apple Silicon. | `Intel-Installer.pkg` oppure `AppleSilicon-Installer.pkg`. |

Gli archivi Linux includono Qt5, Hamlib, i plugin Qt, i moduli QML necessari e una configurazione Fontconfig portabile. Non serve installare separatamente Qt o Hamlib. Scegliere il pacchetto della propria distribuzione: glibc, driver grafici e servizi desktop/audio restano quelli del sistema. Per aggiornare una copia portabile, chiudere MM e conservare `settings.mad`, logbook ADIF e registrazioni prima di sostituire i file del programma.

Gli installer mostrano il copyright di **Papadopol Lucian-Ioan** e la **GNU GPLv3**. La procedura interattiva richiede l'accettazione della licenza. Su Linux avviare `bash nome-del-file-Installer.run`: viene mostrata la licenza in una finestra, se Zenity è disponibile, oppure nel terminale.

Per aggiornare, chiudere MM ed eseguire il nuovo installer: viene aggiornata la stessa installazione. Preferenze e logbook delle installazioni gestite risiedono nelle cartelle dell'utente. Se si passa da una vecchia copia portabile conservata altrove, seguire le [istruzioni di migrazione](docs/platform/INSTALLERS.md) prima di rimuoverla.

Le finestre si ridimensionano trascinando bordi o angoli; i contenuti mantengono una dimensione minima utilizzabile. L'editor SSTV permette di scorrere gli strumenti anche su schermi più piccoli.


Il workflow GitHub **Build installers** crea i pacchetti automaticamente su `main`/`master` e tag `v*`, oppure con **Actions → Build installers → Run workflow**. Quando tutte le piattaforme hanno completato la compilazione e i controlli, pubblica installer, archivi portabili e checksum SHA256 nella sezione **Releases → Assets**. Un tag corrispondente alla versione, per esempio `v0.6.2`, pubblica la release ufficiale; una compilazione da un ramo crea una prerelease `build-0.6.2-N`, senza modificare la release stabile. Per una release ufficiale manuale, selezionare il tag corretto in **Run workflow**. Gli artifact `portable-*` e `installer-*` rimangono disponibili anche in Actions. Gli installer Windows e macOS non hanno ancora una firma commerciale/Developer ID; il sistema operativo può quindi mostrare un avviso.

### Stato del progetto

MadModem è alla versione **0.6.2**. Viene usato e provato realmente in radio, ma diverse parti sono tuttora in sviluppo e possono cambiare rapidamente. Segnalazioni di bug, prove con radio e configurazioni differenti e contributi al codice sono quindi benvenuti.

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

### CW and RTTY skimmers

In the CW panel, enable **Skimmer** and select **Text** or **CQ calls only**. The skimmer decodes up to 16 independent carriers within the received 100–3500 Hz audio passband; it cannot receive frequencies excluded by the radio filter. Text scrolls vertically beside each carrier. CQ view requires two matching callsign readings within a CQ context. Use waterfall frequency zoom for crowded signals: columns that cannot fit are indicated instead of overlapping text. Manual RX A and RX B remain available.

**RTTY** provides the same Skimmer and Text/CQ controls. **Channels** selects 2–16 independent decoders (12 by default), each with its own letters/figures state. All pairs use the selected baud, shift and polarity preset. Pair tracking preserves shift; the main terminal and QSO remain attached to the manually selected station. Skimmer text never starts TX or logs contacts automatically.

### Installation and updates — 0.6.2

**Installers and portable packages are both available. Installation is optional.**

| Platform | Without installation | Installer |
| --- | --- | --- |
| Windows 10/11, 64-bit | Separate `Portable-Compatible` and `Portable-AVX2-FMA` ZIPs; extract and run `bin/MadModem.exe`. Choose `Compatible` if unsure. | One `Installer-AutoCPU.exe` selects the variant supported by the CPU and OS. |
| Ubuntu 24.04 / compatible distributions | Ubuntu24.04 ZIP or TAR.GZ; run `AppRun`. | `Linux-Ubuntu24.04-64bit-Installer.run`, backed by DEB and APT. |
| Fedora 43 | Fedora43 ZIP or TAR.GZ; run `AppRun`. | `Linux-Fedora43-64bit-Installer.run`, backed by RPM and DNF. |
| macOS 15 or later | App ZIP or DMG, separately for Intel and Apple Silicon. | `Intel-Installer.pkg` or `AppleSilicon-Installer.pkg`. |

Linux archives bundle Qt5, Hamlib, required Qt plugins, QML modules and a portable Fontconfig configuration. No separate Qt or Hamlib installation is needed. Choose the matching distribution build: glibc, graphics drivers and desktop/audio services remain provided by the host. Before updating a portable copy, close MM and preserve `settings.mad`, ADIF logbooks and recordings when replacing program files.

Installers display **Papadopol Lucian-Ioan's copyright** and the **GNU GPLv3**. Interactive installation requires license acceptance. On Linux, run `bash installer-filename-Installer.run`; the license opens in a window when Zenity is available, otherwise in the terminal.

To update, close MM and run the new installer. It updates the existing installation while keeping managed-install preferences and logbooks in user folders. When switching from a portable copy stored elsewhere, follow the [migration instructions](docs/platform/INSTALLERS.md) before removing it.

Resize windows by dragging their edges or corners. Layouts retain a usable minimum size, and SSTV editing tools remain scrollable on smaller displays.


GitHub's **Build installers** workflow creates packages on pushes to `main`/`master`, on `v*` tags, or through **Actions → Build installers → Run workflow**. Once all platform builds and checks succeed, installers, portable archives and SHA256 checksums are published under **Releases → Assets**. A tag matching the source version, such as `v0.6.2`, publishes an official release; branch builds publish numbered prereleases such as `build-0.6.2-N`, leaving the stable release unchanged. To publish an official release manually, select its version tag in **Run workflow**. The `portable-*` and `installer-*` artifacts also remain available in Actions. Windows and macOS installers do not yet have commercial/Developer ID signatures, so operating-system warnings may appear.

### Project status

MadModem is at version **0.6.2**. It is actually used and tested on the air, but several parts are still under development and may change quickly. Bug reports, tests with different radios and station configurations, and code contributions are therefore very welcome.

Build instructions, configuration notes and technical information are available in the [`docs`](docs/README.md) directory.

**73 de IZ6NNH**  
Lucian-Ioan Papadopol

---

Copyright (C) 2026 Papadopol Lucian-Ioan <l.i.papadopol@gmail.com>.
Released under [GNU GPLv3](COPYING). See [third-party notices](THIRD_PARTY_NOTICES.md).
