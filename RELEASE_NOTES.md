## R31 clean Windows link fix 1

Fixes the duplicate MshvReference::stopRequested TLS initialization symbol reported by the Windows MinGW linker. Decoder algorithms and Doppler are unchanged. [Fix and validation](docs/platform/WINDOWS_R31_TLS_FIX.md).

## Source R31 clean

The pending two-stage native FT8 residual recovery is integrated. [Current verification and limits](docs/architecture/RELEASE_0_6_4_R31_CLEAN.md) supersede the first R31 handoff below. Existing FT4/Doppler functionality and GitHub workflows are retained; general JTDX parity is not claimed.

## Source R31 development handoff

The complete 0.6.4 source now includes additional native FT8 DSP recovery. See [R31 status](docs/architecture/RELEASE_0_6_4_R31.md) for measured gains, live limitations and incomplete checks. Existing FT4 and Doppler remain available. This source handoff does not claim general JTDX parity.

## 0.6.4 source R30 — integrated native FT8/FT4 recovery

- Complete the headless MSHV 2.76.7 FT8/FT4 recovery path alongside the existing receiver. Preserve ordinary decode, incremental publication, GPU options, TX/CAT and Doppler recovery.
- Isolate decoder configuration/history and FFTW plans per instance; share the TX-priority message-codec lock. Cancel native candidate/OSD work and stop admission at the live UTC deadlines.
- Correct an upstream FT8 sync-quality array overrun; retain protocol/CRC checks and reject uncertain `?` recovery rows. Keep early recoveries through the final slot callback and invalidate stale generation/QSO/band context.
- Tester WAV, three paired runs: gate plus final 41 -> 52 messages, offline 36 -> 60, no baseline messages lost. Of the 60 offline messages, 55 match the tester's 66-row JTDX list; five extra rows remain unverified. A boundary-only job retained 41 with no reliable extra recovery time.
- Linux Qt5 and clean Qt6 full builds: 53/53 tests passed in each; independent Doppler and native sanitizer results are in the R30 validation report. Native Windows/macOS, physical GPU/radio/PTT and crowded real FT4 sensitivity remain unverified here.
- Add FFTW3 to Linux/Windows/macOS workflow dependencies. Public version remains 0.6.4; source revision is `0.6.4-source-r30-reference-ft-recovery`.
- Details: [R30 release and verification](docs/architecture/RELEASE_0_6_4_R30.md), [three-source audit](docs/architecture/FT_DECODER_COMPARISON_R30.md).

## 0.6.4 source R29 — FT8/FT4 coherent recovery and CRC-aware FEC

- Replace degenerate FT8 amplitude-group retries with complex 1/2/3-symbol recovery, including complete tail groups extending into Costas symbols.
- Enable FT4 complex 1/2/4-symbol recovery in normal deep reception; retain the cheap early gate. Add CRC-checked order-1 OSD fallback under the shared resource permit.
- Both LDPC implementations now require parity AND CRC for success; parity-only vectors continue iterating and reach recovery on failure.
- Extra recovery admission is limited by cancellation, 24 ms per candidate and the UTC slot/reply deadline. FT4 OSD statistics are included in session diagnostics.
- Actual-decoder regression checks valid/invalid-CRC codewords, FT4 recovery witnesses, noise/silence and gate/expired slots. GitHub's manual Qt5/Qt6 regression workflow runs this test using Unix Makefiles.
- Public version stays 0.6.4; source revision is R29 (R28 was an investigation, not a source release). Windows and real RF/GPU still require field validation. This is not a JTDX engine replacement or parity claim.
- Measurements and limitations: docs/architecture/RELEASE_0_6_4_R29.md and FT_R29_RESULTS.json.

## 0.6.4 source R27 — Session diagnostics and JTDX source audit

- New Settings → Diagnostics: opt-in persistent full session JSONL logging, folder picker/status/open-folder. Starts immediately with OK and on future launches; normal exit drains accepted events.
- Dedicated writer formats/redacts/flushes events. Records runtime/Qt/Hamlib CAT protocol, audio-block identities and timing, all FT8/FT4 candidate attempts/rejects, full decoder/GUI performance, UI, other-mode outputs and TX events. Overflow/error is explicit; WAV samples remain separate.
- Native concurrency, privacy, crash-flush and actual settings/FT8/FT4 workflow checks; measured full-trace CPU overhead documented. No decoder-algorithm/parity claim.
- Complete public JTDX repository downloaded and pinned; source-based FT8/FT4 pipeline audit and 48-file inventory included. Identifies coherent-metric gates, BP/OSD/AP/history/cancellation/time-window differences requiring controlled follow-up.

## 0.6.4 source R26 — FT8 nearby SIC and decode-table copy

- FT8: reduce post-subtraction candidate exclusion from25Hz to3.125Hz so nearby distinct signals can be tried; existing timing/FEC/CRC/OSD limits retained.
- FT8/FT4 RX table: Ctrl+C copies selected visible rows/columns as plain tab-separated text; Ctrl+A, noncontiguous selection and Unicode supported.
- Tester WAV: three matched Linux replays38–39 ->41 messages versus66JTDX reference rows. Broader8-file/3-run total529 ->543; individual results vary with concurrent OSD admission. No JTDX parity claim.
- Detailed comparison CSV, measured JSON, developer WAV replay probe and limitations in docs/architecture/RELEASE_0_6_4_R26.md.

# MadModem 0.6.4

### R25 — salvataggio automatico SSTV ricevute

Salvataggio PNG automatico delle immagini SSTV completate, con cartella scelta
dall’operatore, nomi UTC/modo senza sovrascritture e apertura della cartella ricevute.
Scrittura in background, esito nello stato RX e salvataggi accodati completati
alla chiusura. RX e QSL da trasmettere rimangono separate. Dettagli e limiti in
`docs/architecture/RELEASE_0_6_4_R25.md`.

### R24 — regolazioni immagine SSTV

Correzioni di fase e allineamento rosso/blu applicate a tutta l’immagine SSTV,
anche a ricezione terminata. Azzera regolazioni ripristina i pixel originali;
comandi raccolti in un pannello richiudibile. Avvio RX senza sincronismo esplicito,
con indicazione della disattivazione di VIS e sync. Tutte le funzioni R23 incluse.
Dettagli e verifiche in `docs/architecture/RELEASE_0_6_4_R24.md`.

### R23 — editor e ricezione SSTV

Editor SSTV a schede con progetto modificabile, annulla/ripristina, elenco elementi
e anteprima nel formato TX. Composizione unica editor/TX, dati stazione e DX distinti.
Intestazione VIS in trasmissione e riconoscimento automatico RX indipendente dal sync;
nuova immagine al VIS successivo. WAV annullabile. Dettagli e limiti in
`docs/architecture/RELEASE_0_6_4_R23.md`.

### R22 — controllo CQ e coda chiamanti

Barre TX rosse/RX verdi secondo il periodo selezionato; serie CQ con conteggio
visibile e ripartenza pulita; nuova finestra Chiamanti (N) per la coda FIFO
in ingresso. Dettagli in `docs/architecture/RELEASE_0_6_4_R22.md`.

### R21 — allineamento del clock audio

Stima del clock di cattura e ricampionamento FT8/FT4 su griglia UTC12kHz,
con correzione continua e diagnostica separata della latenza. Build Linux,
15simulazioni prolungate,10casi PCM/CRC e12test selezionati passati.
Verifica sulla USB Windows ancora da eseguire. Dettagli in
`docs/architecture/RELEASE_0_6_4_R21.md`.

### R20 — riordino interfaccia FT

Stato e messaggio TX nella scheda Modo, indicatori più sobri e controlli sopra
il waterfall su quattro righe. Tutte le opzioni GPU R19 conservate. Dettagli in
`docs/architecture/RELEASE_0_6_4_R20.md`.

### R19 — GPU per il decoder ordinario

Nuova opzione GPU separata dal drift, valida su tutte le bande, per le
energie dei simboli del decoder ordinario FT8/FT4. Confronto con CPU nativa,
misura dei trasferimenti e ritorno automatico alla CPU. OpenCL software e
parità dei candidati verificati; GPU fisica e guadagno end-to-end non verificati.
Dettagli in `docs/architecture/RELEASE_0_6_4_R19.md`.

### R18 — prova GPU per il drift

Nuova opzione sperimentale in Impostazioni → FT8 / FT4 per provare la matrice
Costas del recupero drift su GPU OpenCL FP64, confrontandola con la CPU.
La selezione dipende da parità numerica e tempo totale; il percorso CPU resta
disponibile. Verificati qui il kernel su OpenCL software e i fallback, non
un'accelerazione su GPU fisica. Dettagli in `docs/architecture/RELEASE_0_6_4_R18.md`.

### R17 — drift RX da 144 MHz

Il recupero FT8/FT4 lavora solo da 144 MHz in su e consente fino a ±20 Hz/s.
La ricerca segue i singoli simboli Costas e usa il pool CPU multicore.
Impostazioni e guida chiariscono la selezione RF tramite CAT o banda manuale.
Prove sintetiche, tempi e limiti: `docs/architecture/RELEASE_0_6_4_R17.md`.

### R16 — ricezione Hellschreiber

Feld Hell usa un filtro più selettivo e tempi DSP uniformi fra sample rate. FSK-105 distingue i toni con un discriminatore di frequenza e conserva il segnale durante trasmissioni continue, correggendo la crescita della stima di rumore. Confronto sintetico con R15, prove e limiti in `docs/architecture/RELEASE_0_6_4_R16.md`.

### R14 — trasporto audio RX

Q65 e Radio Telescope ora raccolgono/elaborano l’audio indipendentemente rispettivamente dal job di decodifica e dalla GUI. Le code mantengono contatori verificabili e segnalano le perdite. MSK144 interrompe la ricerca in corso quando viene resettato o chiuso. Vedere `docs/architecture/RELEASE_0_6_4_R14.md` per prove e limiti.

FT periods and UTC are now at the bottom of Mode; the QRZ tab contains station information. Select CQ rows with Ctrl/Shift and use Add selected CQs to prepare a worklist. Start works one selected station at a time after a fresh CQ check; Pause lets the current QSO finish, and STOP stops TX and pauses the list. The separate incoming caller queue remembers stations calling your callsign.

FT capture profile reads use an atomic period snapshot so they can continue while the decoder owns its configuration lock. Search thresholds, passes, FEC and TX timing limits are preserved. Live logs now identify the source/build and report prepared FFT/tone reuse.

The QRZ tab between Mode and Rotator contains information for the selected station. QRZ account settings and saved
credentials remain in Settings. The drift help explains how to select a signal
with the RX marker and what frequency changes the option can recover.

FT8/FT4 prepare reusable synchronization calculations while receiving audio.
Message validation and the existing decode passes are preserved. Recorded FT8
checks show earlier results; the detailed comparison also records output
variations and does not claim increased sensitivity. See
`docs/architecture/RELEASE_0_6_4_R12.md` for measurements and limits.

The Linux DEB installer checks for unfinished dpkg operations before starting
APT and shows the repair command `sudo dpkg --configure -a` when required.
Portable updates identify the existing Ubuntu24.04/Fedora43 bundle before the
host distribution, accept matching TAR.GZ and ZIP downloads, and provide clearer
messages when a package or its checksums are unavailable. User settings,
logbooks, backup, SHA-256 verification and rollback are preserved.

The updater checks all published GitHub releases, including pre-releases,
independently of Latest. It compares the public version and distribution build.
Download and installation require confirmation and wait for idle operation.
If an old executable cannot select the matching Linux package, install this
revision manually once. Future checks use its corrected package selection.

Retained: experimental FT8/FT4 RX drift recovery, CW Native R12 / Neural R13,
compact FT periods, table font controls, response-window fixes and consistent
release metadata across the platform builds.
