# MadModem 0.5.78
## All-in-one digital modem and station hub for amateur radio

**Less fragmentation, more radio.**  
**Meno frammentazione, più radio.**

MadModem is a C++/Qt application that brings digital modes, audio handling, CAT/PTT, antenna-rotator control, QSO logging, mapping and station utilities into one desktop interface.

MadModem è un’applicazione C++/Qt che riunisce modi digitali, gestione audio, CAT/PTT, controllo del rotore d’antenna, registro QSO, mappe e strumenti di stazione in un’unica interfaccia desktop.

**Current release / Versione corrente: 0.5.78**

## Project status / Stato del progetto

MadModem is under active development. Linux is the primary development platform; Windows and macOS packages are produced by the distribution workflows. FT8, FT4 and RTTY are the most mature operating paths. Other modes and advanced station tools remain subject to on-air validation and refinement.

MadModem è in sviluppo attivo. Linux è la piattaforma principale di sviluppo; i pacchetti Windows e macOS vengono prodotti dai workflow di distribuzione. FT8, FT4 e RTTY sono i percorsi operativi più maturi. Gli altri modi e gli strumenti avanzati di stazione richiedono ancora validazione in aria e affinamento.

## Main features / Funzioni principali

- FT8 and FT4 RX/TX with integrated sequencer, standard-message generation and offline WAV analysis.
- RTTY, BPSK/QPSK, MFSK, Feld Hell, CW, SSTV and WEFAX/MeteoFax operating areas.
- MSK144 and Q65 development modes based on in-tree MSHV-derived protocol components.
- Hamlib-based CAT/PTT support and configurable audio devices.
- Integrated azimuth and Alt-Az rotator control, tracking and scheduler tools.
- Radio Telescope receive-only sky scanning with beam-sized hexagonal sampling, integration controls and CSV export.
- Integrated QSO logbook, ADIF import/export, Maidenhead/DXCC tools and QSO map.
- Cockpit-style theme with scalable UI/decode-table fonts and compact layouts.
- Runtime UI and help in English, Italian, French, German, Norwegian and Czech.

- FT8 e FT4 RX/TX con sequencer integrato, generazione dei messaggi standard e analisi WAV offline.
- Aree operative RTTY, BPSK/QPSK, MFSK, Feld Hell, CW, SSTV e WEFAX/MeteoFax.
- Modi MSK144 e Q65 in sviluppo, basati su componenti di protocollo derivati da MSHV inclusi nel sorgente.
- Supporto CAT/PTT tramite Hamlib e dispositivi audio configurabili.
- Controllo integrato di rotori azimutali e Alt-Az, tracking e pianificatore.
- Radio Telescope in sola ricezione, con scansione del cielo a celle esagonali dimensionate sul fascio, integrazione ed export CSV.
- Registro QSO integrato, import/export ADIF, strumenti Maidenhead/DXCC e mappa QSO.
- Tema cockpit con font globali e della tabella decodifiche regolabili e layout compatti.
- Interfaccia e guida disponibili in inglese, italiano, francese, tedesco, norvegese e ceco.

## 0.5.78 highlights

- The Windows `QComboBox` popup path was corrected without detaching Qt's private popup container.
- Dropdown menus now use a theme-coherent background, slightly lighter than the main panel, on every supported platform.
- Runtime dictionaries were regenerated from the canonical source harvest: **1,683 keys in each of six languages**.
- Obsolete duplicate translation keys were removed and placeholder consistency is now checked by `tools/audit_ui_translations.py`.
- Retired MIND runtime remnants were removed from the active FT8 decoder path; the existing classic decoder policy is retained.
- Current documentation and multilingual Qt Help were consolidated; old lab and point-fix notes were moved under `docs/history/`.
- Test media, obsolete built-in autotest UI and MM Flow Studio are not included in the production source package.

## Supported platforms / Piattaforme

Target platforms:

- Linux x86_64
- Windows x86_64
- macOS Apple Silicon
- macOS Intel

Actual hardware, audio, serial/CAT and rotator compatibility depends on the selected Qt/Hamlib toolchain and connected devices.

## Building from source / Compilazione

The main Linux helper is:

```bash
./build_all.sh
```

Typical requirements:

- CMake and a C++17 compiler;
- Qt 5 or Qt 6 development packages for Widgets, SerialPort, Multimedia, PrintSupport and Network;
- Hamlib, supplied by the system or built from the bundled source;
- FFTW3 for the full Q65 bridge when enabled;
- normal platform audio, serial and packaging development tools.

The CMake configuration keeps a multilingual embedded-HTML help fallback. When `qhelpgenerator` is available, it also generates localized `.qch` manuals.

Lo script Linux principale è:

```bash
./build_all.sh
```

Servono normalmente CMake, un compilatore C++17, i pacchetti di sviluppo Qt 5 o Qt 6, Hamlib e gli strumenti di sviluppo audio/seriale della piattaforma. FFTW3 è necessario quando viene abilitato il bridge Q65 completo. La guida HTML multilingua rimane disponibile anche senza `qhelpgenerator`.

## Translation maintenance / Manutenzione traduzioni

Regenerate the runtime dictionaries and run the audit with:

```bash
python3 tools/update_ui_translations.py
python3 tools/audit_ui_translations.py
```

The dictionaries live in `translations/ui_*.ini`; the localized manuals live in `docs/help/`.

## Documentation

- `RELEASE_NOTES.md` — current release notes
- `CHANGELOG.md` — concise release history
- `TRANSLATION_AUDIT.md` — localization status and checks
- `docs/README.md` — documentation map
- `docs/help/` — multilingual user help
- `docs/history/` — archived lab, audit and point-fix notes
- `THIRD_PARTY_NOTICES.md` — source origins and third-party notices

## Safety notes / Note di sicurezza

Verify PTT, CAT mode, TX audio routing, frequency and antenna direction before transmitting. Radio Telescope is receive-only, but automatic rotator movement can still move large mechanical systems: configure limits, rest positions and emergency stop behavior before starting a scan.

Verificare PTT, modo CAT, instradamento audio TX, frequenza e direzione dell’antenna prima di trasmettere. Radio Telescope opera in sola ricezione, ma la scansione automatica può muovere sistemi meccanici di grandi dimensioni: configurare limiti, posizione di riposo e arresto di emergenza prima dell’avvio.

## Author and license

MadModem is developed by **Lucian-Ioan Papadopol, IZ6NNH**.

- MadExp: <https://www.madexp.it/>
- QRZ: <https://www.qrz.com/db/IZ6NNH>

MadModem is released under the **GNU GPLv3**. See `LICENSE.md`, `COPYING` and `THIRD_PARTY_NOTICES.md`.
