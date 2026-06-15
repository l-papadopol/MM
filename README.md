# MadModem (MM)

**English** | [Italiano](#madmodem-mm-italiano)

Current version: **0.5.0-alpha.12**

MadModem, usually called **MM**, is an amateur-radio digital-mode application for Linux and Windows.

It is a single Qt Widgets program with modem RX/TX, audio routing, waterfall, CAT/PTT, logbook, QSO map, FT4/FT8 automation and rotator control. The project is still alpha software, but it is already used for real radio testing.

---

## What it does

### Digital modes

MadModem currently contains support for:

- FT8 / FT4;
- WEFAX;
- SSTV;
- RTTY;
- BPSK / QPSK;
- MFSK;
- CW;
- Hellschreiber.

The FT side includes live full-passband decoding, UTC slot timing, prepared TX messages, slot-aligned audio output, WAV analysis and a regression auto-test tool.

### Audio, CAT and PTT

- selectable audio input/output devices;
- waterfall with RX/TX markers;
- dedicated FT TX audio worker;
- Hamlib radio backend;
- HRD-compatible backend;
- CAT frequency readout and QSY;
- CAT PTT and serial RTS/DTR PTT;
- station safety guard: callsign and locator must be configured before TX/PTT/Tune.

### FT4 / FT8 automation

- AutoQSO candidate ranking;
- QSO state tracking;
- automatic reply preparation;
- TX frequency handling;
- retry/reclaim logic;
- runtime log for FT modes;
- experimental MM Flow Studio layer for visual AutoQSO logic.

MM Flow Studio does not directly key PTT or audio. Flow blocks request guarded actions from the scheduler/runtime.

### Rotator and EME

- independent Hamlib rotator backend;
- multiple rotator profiles;
- pointing to locator, country/DXCC or prefix;
- QSO/correspondent locator tracking;
- Moon / EME tracking from local lunar ephemeris;
- azimuth geometry presets, including Yaesu 450° overlap rotators;
- end-stop/no-movement detection;
- speed/inertia calibration;
- OpenGL-backed Navball display.

### Logbook and map

- ADIF-oriented logbook;
- QSO map with OSM/cache/fallback rendering;
- Maidenhead grid and Home→QSO paths;
- DXCC/prefix lookup through `cty.csv`.

### Localization

Runtime UI dictionaries and HTML/Qt help are currently present for English, Italian, French, German, Norwegian and Czech.

---

## Code origin and credits

MadModem is GPLv3 software. The repository contains three different kinds of material:

1. original MadModem code;
2. reused/bundled open-source code;
3. MadModem-native code inspired by other programs but not copied from them.

This distinction matters. The table below is based on the current source tree and CMake target, not on generic assumptions.

### Original MadModem code

Unless a file or directory is explicitly marked as third-party material, the code belongs to the MadModem project by:

**Papadopol Lucian-Ioan / IZ6NNH / MadExp**

This includes the Qt application, UI, settings system, mode panels, audio/DSP integration, waterfall integration, logbook/map logic, rotator integration, Moon tracking integration, Navball widget, MM Flow Studio integration, build scripts, packaging scripts and documentation.

MadModem was developed with AI assistance under human direction. Product concept, requirements, UI decisions, validation, radio testing and release decisions are by the project author.

### Reused, compiled or linked open-source code

These components are actually compiled into MadModem, linked by MadModem, or built as part of the normal package workflow.

| Upstream project | Current use in MadModem | Location |
|---|---|---|
| **MSHV**, with WSJT-X FT protocol heritage | FT8/FT4 protocol support: packing/unpacking, CRC/LDPC helpers, FT8/FT4 TX waveform generation and selected protocol support code. | `third_party/mshv_gpl/port/` |
| **Hamlib** | Radio CAT/PTT and rotator control. Built from the bundled source by the build scripts and linked by MadModem. | `third_party/hamlib_lgpl/source/`, wrappers in `rig/` and `rotator/` |
| **ggmorse** | CW receive engine used by the CW decoder. | `third_party/ggmorse_mit/` |
| **MMSSTV-derived MadModem SSTV core** | SSTV RX helper code derived from MMSSTV concepts/constants and compiled in the target. | `third_party/mmsstv_lgpl/MmsstvRxCore.*` |
| **Decodium/Raptor-derived NTP client** | Small adapted Qt NTP client used by the application. | `third_party/decodium_gpl/port/NtpClient.*` |
| **AD1C/K1EA country file** | DXCC/country/prefix lookup data. | `cty.csv` |
| **Qt** | Application framework: Widgets, Multimedia, SerialPort, Network, PrintSupport, optional Help/Location pieces. | external dependency |

Preserve the upstream notices and license files in `third_party/`.

### Bundled reference material not compiled into the MadModem target

These files are kept for attribution, traceability and future comparison. They are not listed in the current `MadModem` CMake target.

| Upstream project | Current status |
|---|---|
| **QSSTV** | Reference material only. MadModem's sound-card calibration dialog is QSSTV-style / QSSTV-inspired, but the QSSTV reference tree is not compiled or linked into the MadModem executable. |
| **MMSSTV reference tree** | Reference material. The compiled SSTV helper is the smaller `MmsstvRxCore.*` file listed above. |
| **MSHV reference/upstream tree** | Reference material. The compiled FT material is the reduced port under `third_party/mshv_gpl/port/`. |
| **Decodium/Raptor reference tree** | Reference material. The compiled part is the adapted NTP client under `third_party/decodium_gpl/port/`. |

### Reimplemented / inspired-by work

These MadModem parts were designed after studying other open-source programs, but the current MadModem files are native project code unless they are listed in the previous reused-code table.

| Reference software | MadModem area |
|---|---|
| **WSJT-X** | FT4/FT8 slot timing, QSO-state behaviour, message sequencing and parser behaviour. |
| **MSHV / WSJT-X / JTDX family** | FT receiver architecture ideas: candidate search, Costas/sync handling, LDPC/CRC workflow, subtract/rescan concepts and live-decode budgeting. |
| **fldigi / gmfsk family** | PSK/QPSK/MFSK receiver design ideas. |
| **QSSTV** | Sound-card calibration workflow idea. |
| **CatRotator-style programs** | Rotator workflow and UX ideas. |
| **Meeus/NOAA-style astronomical formulae** | Internal Moon / EME topocentric azimuth/elevation calculation. |

See also `THIRD_PARTY_NOTICES.md`, `LICENSE.md`, `AUTHORS.md`, `COPYING`, `docs/SOURCE_AUDIT.md` and the license files under `third_party/`.

---

## Repository layout

```text
audio/          Audio engines and FT TX worker
dialogs/        Settings, help, logbook and editor dialogs
dsp/            DSP helpers and frequency tracking
dxcc/           cty.csv loader and DXCC/prefix lookup
flow/           MM Flow Studio model/runtime
logbook/        Logbook data handling
modems/         Digital-mode receivers/decoders and FT components
rig/            Radio CAT/PTT wrappers
rotator/        Rotator controller, geometry, Moon tracking and Navball
settings/       Persistent settings model
third_party/    Upstream source, ports, references and notices
translations/   Runtime UI dictionaries
tx/             Transmitters for supported modes
widgets/        Custom Qt widgets, map, waterfall and UI components
```

---

## Building

The normal development environment is Linux. Windows binaries are cross-compiled from Linux with **MXE**.

### Native Linux build

Typical Debian/Ubuntu packages:

```bash
sudo apt install \
  build-essential git cmake make pkg-config zip unzip python3 \
  autoconf automake libtool gettext autopoint bison flex gperf \
  qtbase5-dev qttools5-dev qttools5-dev-tools \
  libqt5serialport5-dev qtmultimedia5-dev \
  qtpositioning5-dev qtlocation5-dev
```

Build:

```bash
./third_party/hamlib_lgpl/build_hamlib.sh

cmake -S . -B build-linux \
  -DCMAKE_BUILD_TYPE=Release \
  -DHAMLIB_ROOT="$PWD/third_party/hamlib_lgpl/install-linux-x86_64" \
  -DMADMODEM_REQUIRE_HAMLIB=ON

cmake --build build-linux -j"$(nproc)"
```

### Windows cross-build with MXE

The build scripts expect MXE in one of these locations:

```text
/home/iz6nnh/mxe
$HOME/mxe
```

Optional override:

```bash
export MXE_ROOT=/path/to/mxe
export MXE_TARGET=x86_64-w64-mingw32.static
```

Required MXE packages for the current workflow:

```bash
cd "$MXE_ROOT"

make MXE_TARGETS=x86_64-w64-mingw32.static \
  openssl qtbase qttools qtserialport qtmultimedia qtlocation qtdeclarative
```

Full Linux + Windows build:

```bash
./build_all.sh
```

Useful switches:

```bash
MADMODEM_BUILD_LINUX=off ./build_all.sh      # Windows only
MADMODEM_BUILD_WINDOWS=off ./build_all.sh    # Linux only
MADMODEM_CREATE_MM_ZIP=off ./build_all.sh    # build without final package
JOBS=8 ./build_all.sh                        # manual parallelism
```

---

## License

MadModem is distributed as a **GPLv3 combined work**.

See:

- `COPYING`
- `LICENSE.md`
- `THIRD_PARTY_NOTICES.md`
- license files inside `third_party/`

Do not remove upstream notices or license files when redistributing source or binaries.

---

# MadModem (MM) Italiano

Versione corrente: **0.5.0-alpha.12**

MadModem, di solito abbreviato in **MM**, è un programma radioamatoriale per modi digitali, Linux e Windows.

Integra modem RX/TX, audio, waterfall, CAT/PTT, logbook, mappa QSO, automazione FT4/FT8 e controllo rotore in una singola applicazione Qt Widgets. Il progetto è ancora in alpha, ma è già usabile per test radio reali.

---

## Cosa fa

### Modi digitali

MadModem attualmente contiene supporto per:

- FT8 / FT4;
- WEFAX;
- SSTV;
- RTTY;
- BPSK / QPSK;
- MFSK;
- CW;
- Hellschreiber.

Il sottosistema FT supporta decode live a banda intera, timing UTC, messaggi TX preparati, audio allineato allo slot, analisi WAV e auto-test di regressione.

### Audio, CAT e PTT

- scelta dispositivi audio ingresso/uscita;
- waterfall con marker RX/TX;
- worker audio FT dedicato;
- backend radio Hamlib;
- backend compatibile HRD;
- lettura frequenza CAT e QSY;
- CAT PTT e PTT seriale RTS/DTR;
- blocco sicurezza stazione: callsign e locator devono essere configurati prima di TX/PTT/Tune.

### Automazione FT4 / FT8

- ranking candidati AutoQSO;
- tracking stato QSO;
- preparazione automatica reply;
- gestione frequenza TX;
- retry/reclaim;
- runtime log per modi FT;
- livello sperimentale MM Flow Studio per logica AutoQSO visuale.

MM Flow Studio non comanda direttamente PTT o audio. I blocchi richiedono azioni protette tramite scheduler/runtime.

### Rotore ed EME

- backend rotore Hamlib indipendente;
- profili rotore multipli;
- puntamento verso locator, country/DXCC o prefisso;
- tracking locator QSO/corrispondente;
- tracking Luna / EME con effemeridi lunari locali;
- preset geometria azimuth, incluso Yaesu overlap 450°;
- rilevamento finecorsa/no-movement;
- calibrazione velocità/inerzia;
- Navball OpenGL.

### Logbook e mappa

- logbook orientato ADIF;
- QSO map con rendering OSM/cache/fallback;
- griglia Maidenhead e tratte Home→QSO;
- lookup DXCC/prefix tramite `cty.csv`.

### Localizzazione

Sono presenti dizionari runtime e help HTML/Qt per inglese, italiano, francese, tedesco, norvegese e ceco.

---

## Origine del codice e crediti

MadModem è software GPLv3. Il repository contiene tre tipi diversi di materiale:

1. codice originale MadModem;
2. codice open source riusato/bundled;
3. codice MadModem nativo ispirato ad altri programmi ma non copiato da essi.

Questa distinzione è importante. La tabella sotto è basata sul sorgente e sul target CMake attuali.

### Codice originale MadModem

Salvo file o directory indicati esplicitamente come terze parti, il codice appartiene al progetto MadModem di:

**Papadopol Lucian-Ioan / IZ6NNH / MadExp**

Questo include applicazione Qt, UI, impostazioni, pannelli modo, integrazione audio/DSP, waterfall, logbook/mappa, integrazione rotore, tracking Luna, Navball, MM Flow Studio, script build/package e documentazione.

MadModem è stato sviluppato con assistenza AI sotto direzione umana. Concetto prodotto, requisiti, scelte UI, validazione, test radio e decisioni di release sono dell'autore del progetto.

### Codice open source riusato, compilato o linkato

Questi componenti sono effettivamente compilati in MadModem, linkati da MadModem o compilati nel normale workflow di pacchetto.

| Progetto upstream | Uso attuale in MadModem | Posizione |
|---|---|---|
| **MSHV**, con eredità protocollo FT da WSJT-X | Supporto protocollo FT8/FT4: pack/unpack, helper CRC/LDPC, generazione TX FT8/FT4 e codice supporto protocollo. | `third_party/mshv_gpl/port/` |
| **Hamlib** | CAT/PTT radio e controllo rotore. Compilato dagli script e linkato da MadModem. | `third_party/hamlib_lgpl/source/`, wrapper in `rig/` e `rotator/` |
| **ggmorse** | Motore CW RX usato dal decoder CW. | `third_party/ggmorse_mit/` |
| **Core SSTV MadModem derivato da MMSSTV** | Helper SSTV RX derivato da concetti/costanti MMSSTV e compilato nel target. | `third_party/mmsstv_lgpl/MmsstvRxCore.*` |
| **Client NTP derivato/adattato da Decodium/Raptor** | Piccolo client NTP Qt usato dall'applicazione. | `third_party/decodium_gpl/port/NtpClient.*` |
| **AD1C/K1EA country file** | Dati lookup DXCC/country/prefix. | `cty.csv` |
| **Qt** | Framework applicativo: Widgets, Multimedia, SerialPort, Network, PrintSupport, parti opzionali Help/Location. | dipendenza esterna |

Conservare notice e file licenza upstream in `third_party/`.

### Materiale reference bundled ma non compilato nel target MadModem

Questi file sono tenuti per attribuzione, tracciabilità e confronto futuro. Non sono elencati nel target CMake `MadModem` attuale.

| Progetto upstream | Stato attuale |
|---|---|
| **QSSTV** | Solo materiale reference. Il dialog di calibrazione audio MadModem è QSSTV-style / ispirato a QSSTV, ma l'albero reference QSSTV non è compilato o linkato nell'eseguibile MadModem. |
| **Reference tree MMSSTV** | Materiale reference. L'helper SSTV compilato è il file più piccolo `MmsstvRxCore.*` indicato sopra. |
| **Reference/upstream tree MSHV** | Materiale reference. Il materiale FT compilato è il port ridotto in `third_party/mshv_gpl/port/`. |
| **Reference tree Decodium/Raptor** | Materiale reference. La parte compilata è il client NTP adattato in `third_party/decodium_gpl/port/`. |

### Reimplementazioni / ispirazioni

Queste parti MadModem sono state progettate studiando altri programmi open source, ma i file MadModem attuali sono codice nativo del progetto, salvo dove indicato nella tabella del codice riusato.

| Software di riferimento | Area MadModem |
|---|---|
| **WSJT-X** | Timing slot FT4/FT8, comportamento stato QSO, sequenza messaggi e parser. |
| **Famiglia MSHV / WSJT-X / JTDX** | Idee architettura ricevitore FT: ricerca candidati, Costas/sync, workflow LDPC/CRC, subtract/rescan e budget decode live. |
| **fldigi / gmfsk** | Idee per ricevitori PSK/QPSK/MFSK. |
| **QSSTV** | Idea workflow calibrazione scheda audio. |
| **Programmi tipo CatRotator** | Workflow e UX rotore. |
| **Formule astronomiche stile Meeus/NOAA** | Calcolo interno Az/El topocentrico Luna / EME. |

Vedi anche `THIRD_PARTY_NOTICES.md`, `LICENSE.md`, `AUTHORS.md`, `COPYING`, `docs/SOURCE_AUDIT.md` e i file licenza in `third_party/`.

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
rig/            Wrapper CAT/PTT radio
rotator/        Controller rotore, geometria, Luna e Navball
settings/       Modello impostazioni persistenti
third_party/    Sorgenti upstream, port, reference e notice
translations/   Dizionari UI runtime
tx/             Trasmettitori per i modi supportati
widgets/        Widget Qt custom, mappa, waterfall e UI
```

---

## Compilazione

L'ambiente normale di sviluppo è Linux. I binari Windows sono cross-compilati da Linux con **MXE**.

### Build Linux nativa

Pacchetti tipici Debian/Ubuntu:

```bash
sudo apt install \
  build-essential git cmake make pkg-config zip unzip python3 \
  autoconf automake libtool gettext autopoint bison flex gperf \
  qtbase5-dev qttools5-dev qttools5-dev-tools \
  libqt5serialport5-dev qtmultimedia5-dev \
  qtpositioning5-dev qtlocation5-dev
```

Build:

```bash
./third_party/hamlib_lgpl/build_hamlib.sh

cmake -S . -B build-linux \
  -DCMAKE_BUILD_TYPE=Release \
  -DHAMLIB_ROOT="$PWD/third_party/hamlib_lgpl/install-linux-x86_64" \
  -DMADMODEM_REQUIRE_HAMLIB=ON

cmake --build build-linux -j"$(nproc)"
```

### Cross-build Windows con MXE

Gli script cercano MXE in una di queste posizioni:

```text
/home/iz6nnh/mxe
$HOME/mxe
```

Override opzionale:

```bash
export MXE_ROOT=/path/to/mxe
export MXE_TARGET=x86_64-w64-mingw32.static
```

Moduli MXE richiesti nel workflow attuale:

```bash
cd "$MXE_ROOT"

make MXE_TARGETS=x86_64-w64-mingw32.static \
  openssl qtbase qttools qtserialport qtmultimedia qtlocation qtdeclarative
```

Build completa Linux + Windows:

```bash
./build_all.sh
```

Switch utili:

```bash
MADMODEM_BUILD_LINUX=off ./build_all.sh      # solo Windows
MADMODEM_BUILD_WINDOWS=off ./build_all.sh    # solo Linux
MADMODEM_CREATE_MM_ZIP=off ./build_all.sh    # build senza package finale
JOBS=8 ./build_all.sh                        # parallelismo manuale
```

---

## Licenza

MadModem è distribuito come **opera combinata GPLv3**.

Vedi:

- `COPYING`
- `LICENSE.md`
- `THIRD_PARTY_NOTICES.md`
- file licenza in `third_party/`

Non rimuovere notice o licenze upstream quando redistribuisci sorgente o binari.
