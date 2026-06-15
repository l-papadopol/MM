# MM — MadModem

**English** | [Italiano](#mm--madmodem-italiano)

Current public version: **MadModem 0.5.0-alpha.6**.

MadModem, usually shortened to **MM**, is a GPLv3 amateur-radio digital-mode application for Linux and Windows. It is developed as a single Qt Widgets program that combines audio RX/TX, CAT/PTT, waterfall display, logbook, QSO map, FT4/FT8 automation and an integrated rotator controller.

This repository is intentionally explicit about attribution. MadModem contains original project code, source-level ports of GPL/LGPL/MIT open-source components, and original reimplementations inspired by well-known amateur-radio software. See also [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md), [`AUTHORS.md`](AUTHORS.md), [`LICENSE.md`](LICENSE.md) and the license files preserved under `third_party/`.

---

## What MadModem can currently do

### Digital modes and audio

- FT8 and FT4 RX/TX with UTC slot scheduling, full-passband live decode, offline WAV analysis and an Auto test harness for regression checks.
- FT AutoQSO with QSO-state tracking, automatic reply preparation, TX-plan scheduling, target selection and guarded Flow Studio shadow/runtime integration.
- WEFAX RX/TX tools: LPM, black/white tones, APT start, auto tone, band-pass, stop/end detection, manual RX forcing, image reset and PNG save.
- SSTV RX/TX support with image loading/transmission workflow and MMSSTV-derived receive ideas.
- RTTY RX/TX, BPSK/QPSK, MFSK, CW and Hellschreiber support.
- Dedicated FT TX audio worker for low-latency slot-aligned audio output.
- Audio input/output selection, DSP controls, markers and waterfall display.

### CAT, PTT and station safety

- Hamlib-based radio CAT/PTT backend.
- HRD-compatible backend support.
- CAT frequency readout and QSY support.
- CAT PTT, DATA/PTT handling where the Hamlib backend exposes it, and serial RTS/DTR PTT options.
- Mandatory station identity guard: **My Call** and **My Locator** in Settings → User/QTH are required before TX/PTT/Tune and rotator movement are allowed.

### FT4/FT8 automation

- UTC-locked FT8 15 s and FT4 7.5 s scheduling model.
- TxPlan-based transmission path: UI, scheduler, modulator and log use the same prepared transmission plan.
- AutoQSO candidate ranking by new country, new locator square, new band, new mode, distance and SNR.
- Optional behaviour controls such as TX-frequency strategy, target reclaim/retry and worked/blacklist logic.
- Runtime log access for FT modes.
- MM Flow Studio: a guarded event/capability architecture for visual AutoQSO flow work. Flow actions do not directly key PTT or audio; they request abstract actions through safety gates.

### Rotator and EME

- Independent Hamlib rotator backend, separate from radio CAT.
- Hamlib rotator model enumeration plus fallback entries such as Yaesu G-601 / GS-232A.
- Three rotator profiles with band assignment from HF through microwave bands.
- Manual pointing to Maidenhead locator, country/DXCC or prefix using `cty.csv`.
- QSO/correspondent locator tracking.
- **Moon / EME tracking**: local dependency-free lunar ephemeris from UTC and configured User/QTH locator; the Moon mode bypasses QSO locator tracking and points the rotator to the Moon when it is above the local horizon.
- Azimuth geometry presets: standard 360° stop-at-North, standard 360° stop-at-South, Yaesu 450° overlap and custom mechanical range.
- End-stop/no-movement detection with one automatic reverse retry.
- Inertia/speed calibration for azimuth/elevation and rotator-ready TX guard.
- OpenGL-backed Navball visualization with horizon, useful grid labels, target marker and overlap indication.

### Logbook, map and support tools

- QSO logbook with ADIF-oriented data handling.
- QSO map with OSM/cache/fallback rendering, marker layers, Maidenhead grid, Home→QSO paths and logbook overlays.
- DXCC/country lookup from `cty.csv`.
- Localized Qt/HTML help in English, Italian, French, German, Norwegian and Czech.
- Multilingual runtime UI dictionaries for the same languages.
- Linux + Windows all-in-one packaging through `build_all.sh` and `tools/package_mm_zip.sh`.

---

## Authorship and code-origin map

### Original MadModem project code

Unless a file or directory is explicitly listed as third-party material, the code in this repository is original MadModem project material by **Papadopol Lucian-Ioan** / **IZ6NNH** / MadExp, developed through a human-directed AI-assisted workflow.

Original MadModem material includes, in particular:

- the Qt Widgets application shell, main window, settings dialogs, mode panels and integration glue;
- the audio/DSP integration layer and mode switching infrastructure;
- the waterfall/UI integration and marker workflow;
- the FT scheduler integration, TxPlan ownership model, AutoQSO state integration and safety gates, except for FT protocol material credited below;
- MM Flow Studio model/editor/runtime/capability architecture;
- logbook, map integration, QSO-map layer management and ADIF-oriented application logic;
- rotator panel/controller integration, rotator geometry handling, Moon/EME tracking integration and Navball widget;
- build scripts, packaging scripts, documentation and localized runtime dictionaries.

The author directed the product concept, UI decisions, feature requirements, validation, radio testing, release decisions and integration. AI assistance was used as an implementation accelerator; it does not remove the attribution and licensing obligations for reused open-source code.

### Open-source code copied, ported or bundled in the source tree

| Upstream project | Where in this tree | How it is used | License / credit |
|---|---|---|---|
| **MSHV** FT material, with FT protocol heritage from WSJT-X | `third_party/mshv_gpl/port/` | Source-level port for FT8/FT4 message packing/unpacking, FT TX waveform generation, CRC/LDPC/protocol helper material and related FT integration support. | GPL. Preserve MSHV notices and original WSJT-X-related credits contained in the upstream files. |
| **Hamlib** | `third_party/hamlib_lgpl/source/` plus MadModem wrappers in `rig/` and `rotator/` | Bundled source tree used to build reproducible Linux and Windows CAT/PTT/rotator support. MadModem links to Hamlib APIs for radio and rotator control. | Hamlib library LGPL-2.1-or-later; some upstream tools/examples GPL-2.0-or-later. Credit The Hamlib Group and upstream `AUTHORS`. |
| **ggmorse** | `third_party/ggmorse_mit/` | Bundled CW receive engine used for robust Morse timing/speed estimation and decoding support. | MIT License. Credit Georgi Gerganov. |
| **MMSSTV** | `third_party/mmsstv_lgpl/` | Source-level Qt/CMake-friendly port/reference for SSTV RX ideas, frequency estimation and line/rendering concepts. | LGPLv3-or-later. Credit Makoto Mori and Nobuyuki Oba. |
| **QSSTV** | `third_party/qsstv_gpl/` | Sound-card calibration concept and reference material; MadModem uses a Qt Multimedia adaptation rather than the original UI/audio stack. | GPLv3-or-later. Credit Johan Maes, ON4QZ. |
| **Decodium / Raptor reference material** | `third_party/decodium_gpl/port/` | Small adapted Qt NTP client and waterfall/decoder-hygiene reference material. | GPL. Credit Decodium/Raptor authors as preserved in notices. |
| **AD1C/K1EA country file format/data** | `cty.csv` and `dxcc/` loader | DXCC/country/prefix lookup for map/logbook/rotator target resolution. Users may replace `cty.csv` with an updated file. | External country-file data; keep upstream attribution when updating the file. |
| **Qt** | Build/runtime dependency, not copied as source except generated project integration files | GUI, multimedia, serial port, networking, optional Qt Help and optional Qt Location support. | Respect the license of the Qt version used by the builder/distributor. |

### Inspired-by / reimplemented algorithms without direct source copying

The following MadModem subsystems were designed after studying established open-source amateur-radio programs, but are implemented as MadModem-native code unless a specific file is listed in the previous table:

| Reference project | MadModem subsystem | What was reimplemented or used as design inspiration |
|---|---|---|
| **WSJT-X** | FT4/FT8 timing, sequencing and parser behaviour | UTC slot discipline, post-slot decode timing, prepared TX messages, AutoQSO state progression, FT message classification and safety around PTT/audio scheduling. |
| **MSHV / WSJT-X / JTDX family** | FT receiver architecture | Full-passband candidate search, Costas/sync handling, LDPC/CRC decode strategy, overlap/subtract/rescan ideas and practical live-decode budgeting. Protocol tables/helpers credited above remain third-party. |
| **fldigi / gmfsk family** | PSK/QPSK/MFSK receiver design | Tone-bank receiver architecture, AFC-style tracking ideas, status metrics and conservative modem UI behaviour. No full fldigi UI/runtime subsystem is imported. |
| **QSSTV / MMSSTV** | SSTV and sound-card calibration workflow | Timing/calibration concepts and SSTV receive behaviour were adapted to MadModem's own Qt audio engine and UI. |
| **CatRotator-style workflows** | Rotator UX | Multi-rotator workflow ideas, model list expectations, band assignment and rotator status ergonomics; the integrated MadModem rotator controller/UI is original project code using Hamlib APIs. |
| **Astronomical Meeus/NOAA-style formulae** | Moon / EME tracking | Low-order local lunar ephemeris calculation from UTC and QTH for topocentric Az/El. This is implemented internally and does not require online ephemeris downloads. |

---

## Repository layout

```text
audio/          Audio engines and FT TX worker
dialogs/        Settings, help, logbook and editor dialogs
dsp/            DSP helpers and frequency tracking
dxcc/           cty.csv loader and DXCC/prefix lookup
flow/           MM Flow Studio model/runtime pieces
logbook/        Logbook data handling
modems/         Digital-mode receivers/decoders and FT components
rig/            Radio CAT/PTT wrappers around Hamlib/HRD
rotator/        Rotator controller, panel, geometry, Moon tracking, Navball
settings/       Persistent settings model
third_party/    Preserved upstream source/ports/notices
translations/   Runtime UI dictionaries
tx/             Transmitters for supported modes
widgets/        Custom Qt widgets, map, waterfall and UI components
```

---

## Development/build environment

The normal development machine is Linux. Windows binaries are produced by cross-compiling with **MXE**.

### Native Linux requirements

Install the normal C++/Qt development stack for your distribution. On Debian/Ubuntu-like systems the package names are typically similar to:

```bash
sudo apt install \
  build-essential git cmake make pkg-config zip unzip python3 \
  autoconf automake libtool gettext autopoint bison flex gperf \
  qtbase5-dev qttools5-dev qttools5-dev-tools \
  libqt5serialport5-dev qtmultimedia5-dev \
  qtpositioning5-dev qtlocation5-dev
```

Notes:

- Qt5 is the current practical baseline. The CMake files also probe Qt6 where available.
- Qt Help is optional but recommended for generated `.qch` help files.
- Qt Location is optional; when unavailable, MadModem uses fallback map rendering.
- Hamlib does not need to be preinstalled when using the bundled build scripts; `build_all.sh` builds the bundled Hamlib source first.

Native Linux build:

```bash
./third_party/hamlib_lgpl/build_hamlib.sh
cmake -S . -B build-linux \
  -DCMAKE_BUILD_TYPE=Release \
  -DHAMLIB_ROOT="$PWD/third_party/hamlib_lgpl/install-linux-x86_64" \
  -DMADMODEM_REQUIRE_HAMLIB=ON
cmake --build build-linux -j"$(nproc)"
```

### Windows cross-build with MXE

MadModem's Windows build is produced from Linux through MXE. The scripts expect MXE at either:

```text
/home/iz6nnh/mxe
```

or:

```text
$HOME/mxe
```

You can override it with:

```bash
export MXE_ROOT=/path/to/mxe
export MXE_TARGET=x86_64-w64-mingw32.static
```

The MXE target must provide the static MinGW toolchain and the Qt/OpenSSL modules used by MadModem. In the current workflow the important MXE packages are:

```bash
cd "$MXE_ROOT"
make MXE_TARGETS=x86_64-w64-mingw32.static \
  openssl qtbase qttools qtserialport qtmultimedia qtlocation qtdeclarative
```

Minimum practical notes:

- `qtbase` provides Qt Widgets, Network and PrintSupport.
- `qttools` provides tools such as `qhelpgenerator`.
- `qtserialport` is required for serial/CAT-related Qt code.
- `qtmultimedia` is required for audio I/O.
- `openssl` is needed so the Windows build can use HTTPS map tiles through Qt Network.
- `qtlocation`/`qtdeclarative` are useful for Qt Location/QML map support when enabled; the app has fallback map rendering when this stack is missing.

Then run the all-in-one build script from the MadModem root:

```bash
./build_all.sh
```

`build_all.sh` does the following:

1. builds bundled Hamlib for Linux;
2. builds MadModem for Linux;
3. builds bundled Hamlib for Windows through MXE;
4. builds MadModem for Windows through MXE;
5. installs build trees under `dist/linux` and `dist/windows`;
6. creates the distributable `MM/` folder and `mm.zip`.

Useful environment switches:

```bash
MADMODEM_BUILD_LINUX=off ./build_all.sh      # Windows only
MADMODEM_BUILD_WINDOWS=off ./build_all.sh    # Linux only
MADMODEM_CREATE_MM_ZIP=off ./build_all.sh    # build without packaging
JOBS=8 ./build_all.sh                        # override parallelism
```

---

## License

MadModem is distributed as a **GPLv3 combined work**. See [`COPYING`](COPYING) and [`LICENSE.md`](LICENSE.md).

Because GPL/LGPL/MIT and other upstream material is preserved or used in the tree, do not remove the third-party notices. Any public binary/source redistribution must include the corresponding source and license notices.

---

# MM — MadModem Italiano

Versione pubblica corrente: **MadModem 0.5.0-alpha.6**.

MadModem, spesso abbreviato in **MM**, è un'applicazione radioamatoriale GPLv3 per modi digitali, Linux e Windows. È sviluppata come programma unico Qt Widgets e integra audio RX/TX, CAT/PTT, waterfall, logbook, mappa QSO, automazione FT4/FT8 e controllo rotore.

Questo repository dichiara esplicitamente le attribuzioni. MadModem contiene codice originale del progetto, porting/copie sorgente di componenti open source GPL/LGPL/MIT e reimplementazioni originali ispirate a software radioamatoriali noti. Vedi anche [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md), [`AUTHORS.md`](AUTHORS.md), [`LICENSE.md`](LICENSE.md) e i file licenza conservati in `third_party/`.

---

## Cosa fa attualmente MadModem

### Modi digitali e audio

- FT8 e FT4 RX/TX con scheduling UTC, decode live a banda intera, analisi WAV offline e Auto test per regressioni.
- FT AutoQSO con stato QSO, preparazione automatica reply, pianificazione TX, scelta target e integrazione protetta con MM Flow Studio.
- WEFAX RX/TX: LPM, toni nero/bianco, APT start, auto tone, band-pass, rilevamento stop/end, forzatura RX manuale, reset immagine e salvataggio PNG.
- SSTV RX/TX con workflow immagine e idee RX derivate/adattate da MMSSTV.
- Supporto RTTY RX/TX, BPSK/QPSK, MFSK, CW e Hellschreiber.
- Worker audio FT dedicato per TX a bassa latenza allineato allo slot.
- Selezione input/output audio, controlli DSP, marker e waterfall.

### CAT, PTT e sicurezza stazione

- Backend radio CAT/PTT basato su Hamlib.
- Supporto backend compatibile HRD.
- Lettura frequenza CAT e QSY.
- CAT PTT, gestione DATA/PTT dove esposta dal backend Hamlib e opzioni PTT seriale RTS/DTR.
- Blocco obbligatorio identità stazione: **My Call** e **My Locator** in Settings → User/QTH sono richiesti prima di qualsiasi TX/PTT/Tune e prima di muovere il rotore.

### Automazione FT4/FT8

- Scheduling FT8 15 s e FT4 7,5 s agganciato all'UTC.
- Percorso TX basato su TxPlan: UI, scheduler, modulatore e log usano lo stesso piano TX preparato.
- Ranking AutoQSO per nuovo country, nuovo locator square, nuova banda, nuovo modo, distanza e SNR.
- Strategie TX frequency, reclaim/retry target e logica worked/blacklist.
- Runtime log nei modi FT.
- MM Flow Studio: architettura visuale/event bus/capability per il lavoro AutoQSO. I blocchi non comandano direttamente PTT o audio, ma richiedono azioni astratte attraverso guardie di sicurezza.

### Rotore ed EME

- Backend rotore Hamlib indipendente dalla CAT radio.
- Enumerazione modelli Hamlib e fallback come Yaesu G-601 / GS-232A.
- Tre profili rotore assegnabili per banda da HF a microonde.
- Puntamento manuale verso locator Maidenhead, paese/DXCC o prefisso usando `cty.csv`.
- Tracking locator QSO/corrispondente.
- **Tracking Luna / EME**: effemeridi lunari locali senza dipendenze esterne, calcolate da UTC e locator User/QTH; la modalità Luna bypassa il locator del corrispondente e punta il rotore verso la Luna quando è sopra l'orizzonte locale.
- Preset geometria azimuth: 360° stop a Nord, 360° stop a Sud, Yaesu overlap 450° e range meccanico custom.
- Rilevamento finecorsa/no-movement con un retry automatico nel verso opposto.
- Calibrazione inerzia/velocità azimuth/elevazione e guardia FT TX mentre il rotore non è pronto.
- Navball OpenGL con orizzonte, griglia essenziale, marker target e indicazione overlap.

### Logbook, mappa e strumenti

- Logbook QSO con gestione orientata ADIF.
- QSO map con rendering OSM/cache/fallback, layer marker, griglia Maidenhead, percorsi Home→QSO e overlay logbook.
- Lookup DXCC/country da `cty.csv`.
- Help Qt/HTML localizzato in inglese, italiano, francese, tedesco, norvegese e ceco.
- Dizionari runtime multilingua per le stesse lingue.
- Packaging Linux + Windows all-in-one tramite `build_all.sh` e `tools/package_mm_zip.sh`.

---

## Autorialità e origine del codice

### Codice originale MadModem

Salvo file o directory esplicitamente indicati come materiale di terze parti, il codice di questo repository è materiale originale del progetto MadModem di **Papadopol Lucian-Ioan** / **IZ6NNH** / MadExp, sviluppato con workflow assistito da AI ma diretto dall'autore umano.

Il materiale originale MadModem include in particolare:

- shell applicativa Qt Widgets, main window, dialog impostazioni, pannelli modo e logica di integrazione;
- layer di integrazione audio/DSP e infrastruttura cambio modo;
- integrazione waterfall/UI e workflow marker;
- integrazione scheduler FT, modello TxPlan, stato AutoQSO e guardie di sicurezza, escluso il materiale protocollo FT accreditato sotto;
- modello/editor/runtime/capability di MM Flow Studio;
- logbook, integrazione mappa, gestione layer QSO map e logica applicativa ADIF;
- integrazione pannello/controller rotore, geometria rotore, tracking Luna/EME e widget Navball;
- script build/packaging, documentazione e dizionari runtime localizzati.

L'autore ha diretto concetto prodotto, scelte UI, requisiti, validazione, test radio, decisioni di release e integrazione. L'assistenza AI è stata usata come acceleratore implementativo; non elimina gli obblighi di attribuzione e licenza verso il codice open source riutilizzato.

### Codice open source copiato, portato o incluso nel sorgente

| Progetto upstream | Dove si trova | Uso in MadModem | Licenza / credito |
|---|---|---|---|
| **MSHV** e materiale protocollo FT con eredità WSJT-X | `third_party/mshv_gpl/port/` | Porting sorgente per pack/unpack FT8/FT4, generazione TX FT, helper CRC/LDPC/protocollo e supporto integrazione FT. | GPL. Conservare notice MSHV e crediti WSJT-X presenti nei file upstream. |
| **Hamlib** | `third_party/hamlib_lgpl/source/`, wrapper in `rig/` e `rotator/` | Sorgente incluso per build riproducibili di CAT/PTT/rotore Linux e Windows. MadModem usa le API Hamlib. | Libreria LGPL-2.1-or-later; alcuni tool/esempi GPL-2.0-or-later. Crediti a The Hamlib Group e upstream `AUTHORS`. |
| **ggmorse** | `third_party/ggmorse_mit/` | Motore CW RX per stima timing/velocità Morse e decodifica. | MIT License. Credito a Georgi Gerganov. |
| **MMSSTV** | `third_party/mmsstv_lgpl/` | Porting/riferimento Qt/CMake per idee SSTV RX, stima frequenza e rendering linee. | LGPLv3-or-later. Crediti a Makoto Mori e Nobuyuki Oba. |
| **QSSTV** | `third_party/qsstv_gpl/` | Concetto calibrazione scheda audio e materiale di riferimento; MadModem usa un adattamento Qt Multimedia. | GPLv3-or-later. Credito a Johan Maes, ON4QZ. |
| **Decodium / Raptor** | `third_party/decodium_gpl/port/` | Piccolo client NTP Qt adattato e materiale di riferimento per waterfall/decode hygiene. | GPL. Crediti come preservati nei notice. |
| **AD1C/K1EA country file** | `cty.csv` e loader `dxcc/` | Lookup DXCC/country/prefix per mappa/logbook/rotore. L'utente può sostituire `cty.csv` con una versione aggiornata. | Dati country-file esterni; preservare attribuzione upstream quando si aggiorna. |
| **Qt** | Dipendenza build/runtime | GUI, multimedia, seriale, networking, Qt Help opzionale e Qt Location opzionale. | Rispettare la licenza Qt usata da chi compila/distribuisce. |

### Algoritmi reimplementati ispirandosi ad altri software, senza copia diretta di sorgente

| Progetto di riferimento | Sottosistema MadModem | Cosa è stato reimplementato o usato come ispirazione |
|---|---|---|
| **WSJT-X** | Timing, sequencer e parser FT4/FT8 | Disciplina slot UTC, decode post-slot, messaggi TX preparati, progressione stato AutoQSO, classificazione messaggi FT e sicurezza PTT/audio. |
| **Famiglia MSHV / WSJT-X / JTDX** | Architettura ricevitore FT | Ricerca candidati a banda intera, Costas/sync, strategia LDPC/CRC, idee overlap/subtract/rescan e budget decode live. Tabelle/helper protocollo accreditati sopra restano terze parti. |
| **fldigi / gmfsk** | Design ricevitore PSK/QPSK/MFSK | Idee di tone-bank, AFC, metriche conservative e comportamento UI modem; nessun intero sottosistema GUI/runtime fldigi importato. |
| **QSSTV / MMSSTV** | SSTV e calibrazione sound card | Concetti timing/calibrazione e comportamento SSTV adattati al motore audio Qt e alla UI MadModem. |
| **Workflow tipo CatRotator** | UX rotore | Idee di workflow multi-rotore, lista modelli, assegnazione bande ed ergonomia status; controller/UI integrati MadModem sono originali e usano API Hamlib. |
| **Formule astronomiche stile Meeus/NOAA** | Tracking Luna / EME | Calcolo interno locale delle effemeridi lunari topocentriche da UTC e QTH per Az/El. Non scarica effemeridi online. |

---

## Layout repository

```text
audio/          Motori audio e worker FT TX
dialogs/        Impostazioni, help, logbook ed editor
dsp/            Helper DSP e frequency tracking
dxcc/           Loader cty.csv e lookup DXCC/prefix
flow/           MM Flow Studio model/runtime
logbook/        Gestione dati logbook
modems/         Ricevitori/decoder modi digitali e componenti FT
rig/            Wrapper radio CAT/PTT Hamlib/HRD
rotator/        Controller rotore, pannello, geometria, Luna, Navball
settings/       Modello impostazioni persistenti
third_party/    Sorgenti/port/notices upstream preservati
translations/   Dizionari UI runtime
tx/             Trasmettitori per i modi supportati
widgets/        Widget Qt custom, mappa, waterfall e UI
```

---

## Ambiente di sviluppo/build

La macchina di sviluppo normale è Linux. I binari Windows vengono prodotti tramite cross-compilazione con **MXE**.

### Requisiti Linux nativo

Installare lo stack C++/Qt di sviluppo della propria distribuzione. Su Debian/Ubuntu i pacchetti sono tipicamente simili a:

```bash
sudo apt install \
  build-essential git cmake make pkg-config zip unzip python3 \
  autoconf automake libtool gettext autopoint bison flex gperf \
  qtbase5-dev qttools5-dev qttools5-dev-tools \
  libqt5serialport5-dev qtmultimedia5-dev \
  qtpositioning5-dev qtlocation5-dev
```

Note:

- Qt5 è la baseline pratica corrente; i CMake file provano anche Qt6 quando disponibile.
- Qt Help è opzionale ma consigliato per generare i file `.qch`.
- Qt Location è opzionale; se manca, MadModem usa rendering mappa fallback.
- Hamlib non deve essere installato a sistema quando si usano gli script inclusi; `build_all.sh` compila prima Hamlib dal sorgente bundled.

Build Linux nativa:

```bash
./third_party/hamlib_lgpl/build_hamlib.sh
cmake -S . -B build-linux \
  -DCMAKE_BUILD_TYPE=Release \
  -DHAMLIB_ROOT="$PWD/third_party/hamlib_lgpl/install-linux-x86_64" \
  -DMADMODEM_REQUIRE_HAMLIB=ON
cmake --build build-linux -j"$(nproc)"
```

### Cross-build Windows con MXE

La build Windows di MadModem viene prodotta da Linux con MXE. Gli script cercano MXE in:

```text
/home/iz6nnh/mxe
```

oppure:

```text
$HOME/mxe
```

Si può forzare il percorso con:

```bash
export MXE_ROOT=/path/to/mxe
export MXE_TARGET=x86_64-w64-mingw32.static
```

Il target MXE deve fornire toolchain MinGW statica e moduli Qt/OpenSSL usati da MadModem. Nel workflow corrente i pacchetti MXE importanti sono:

```bash
cd "$MXE_ROOT"
make MXE_TARGETS=x86_64-w64-mingw32.static \
  openssl qtbase qttools qtserialport qtmultimedia qtlocation qtdeclarative
```

Note minime:

- `qtbase` fornisce Qt Widgets, Network e PrintSupport.
- `qttools` fornisce strumenti come `qhelpgenerator`.
- `qtserialport` serve per codice seriale/CAT.
- `qtmultimedia` serve per l'I/O audio.
- `openssl` serve per HTTPS map tiles nella build Windows tramite Qt Network.
- `qtlocation`/`qtdeclarative` sono utili per Qt Location/QML map quando abilitato; l'app ha comunque fallback mappa.

Poi, dalla root MadModem:

```bash
./build_all.sh
```

`build_all.sh`:

1. compila Hamlib bundled per Linux;
2. compila MadModem per Linux;
3. compila Hamlib bundled per Windows tramite MXE;
4. compila MadModem per Windows tramite MXE;
5. installa sotto `dist/linux` e `dist/windows`;
6. crea la cartella distribuibile `MM/` e `mm.zip`.

Switch utili:

```bash
MADMODEM_BUILD_LINUX=off ./build_all.sh      # solo Windows
MADMODEM_BUILD_WINDOWS=off ./build_all.sh    # solo Linux
MADMODEM_CREATE_MM_ZIP=off ./build_all.sh    # build senza package
JOBS=8 ./build_all.sh                        # parallelismo manuale
```

---

## Licenza

MadModem è distribuito come **opera combinata GPLv3**. Vedi [`COPYING`](COPYING) e [`LICENSE.md`](LICENSE.md).

Poiché nel tree sono presenti o usati materiali GPL/LGPL/MIT e altri upstream, non rimuovere i notice di terze parti. Qualsiasi redistribuzione pubblica binaria/sorgente deve includere sorgente corrispondente e notice/licenze.
