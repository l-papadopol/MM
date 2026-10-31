## 0.6.4 source R31 — LOGZIP1: compressed complete session diagnostics

- Write full session events directly to standard .jsonl.gz with fast, lossless gzip on the diagnostics worker. No event filtering or plain temporary log.
- Batch file/compressor flushes at 250 ms, retaining explicit barriers, shutdown drain, error/loss reporting and gzip CRC finalization. Include a reader for legacy logs and incomplete-stream recovery.
- Declare zlib in CMake and platform CI; update six-language settings/help and inherited TXFIX2 runtime log translations.
- Linux Qt5 application build, four targeted CTests, native compression/fatal-prefix checks, Python reader and translation audit passed. RX, Doppler and TXFIX2 control/queue sources unchanged.
- [Format, compression measurement and recovery](docs/architecture/SESSION_LOG_GZIP_R31.md).

## 0.6.4 source R31 — TXFIX2: operator takeover and Auto QSO queue

- Double-click takes control even with Manage CQ queue enabled; stops the previous FT waveform before arming the selected correspondent and preserves explicit late-start permission.
- Studied official WSJT-X sequencing at commit 967c85a6: late operator TX seeks the buffered waveform to its UTC sample position instead of restarting its prefix. Automatic replies retain the existing complete-frame policy.
- Evil Auto QSO retains a bounded priority CQ queue while busy, including asynchronous logbook lookups. Rechecks fresh CQ, directed traffic, blacklist, duplicate and cooldown policies; direct callers and explicit operator lists retain precedence. Queue entries are visible/removable in the CQ list.
- Linux Qt5 full build, 5 targeted CTests and FT8/FT4 response, recovery and operator/queue UI checks passed. RX, Doppler and previous Windows TLS correction preserved. Physical Windows/radio confirmation remains pending.
- [Behavior, WSJT-X comparison and validation](docs/architecture/FT_R31_TXFIX2.md).

## 0.6.4 source R31 — sequencer/audio TX fix 1

- Separated nominal tone time from the hard full-frame start cutoff; complete automatic frames can absorb backend latency while preserving every sample and the slot-end guard.
- Recovered failed FT audio starts without leaving a QSO armed with no TX plan. Preserved newer deferred replies and retry counts; rejected old worker notifications and bounded repeated backend failures.
- Kept valid current-slot duplicate plans and made the watchdog respect an already requested start. Preserved operator STOP and explicit manual partial bursts.
- Full Linux Qt5 build, four targeted CTest checks and normal/Evil FT8/FT4 UI recovery cases passed. Includes WINFIX1. [Details and validation limits](docs/architecture/FT_R31_SEQUENCER_FIX.md).

## 0.6.4 source R31 clean — Windows link fix 1

- Fixed duplicate TLS initialization symbols for the FT cancellation callback reported by MSYS2 UCRT64 MinGW GCC 16.2.0. One definition in budget.cpp replaces the inline object definition in the shared header.
- Preserved per-thread cancellation and all decoder algorithms. Added a cross-file/thread isolation check; Linux full application build and both targeted checks passed. Native Windows confirmation remains with CI.
- Details: [Windows link correction](docs/platform/WINDOWS_R31_TLS_FIX.md).

## 0.6.4 source R31 clean — completed residual integration

- Split native FT8 recovery into consecutive native-model and supplementary-model residuals under one deadline; preserved previously published rows.
- Added separate native recovery diagnostics and safe cancellation on FFT preparation failure; listed native helper headers in CMake and excluded build/ from Git.
- Clean Linux Qt5 application build and 14 targeted checks passed; tester offline result repeated at 62 rows and 57/66 exact JTDX matches in three runs. Live parity remains open.
- See [R31 clean verification](docs/architecture/RELEASE_0_6_4_R31_CLEAN.md).

## 0.6.4 source R31 — additional native FT8 DSP recovery

- Added native spectral downconversion, coherent DF/DT refinement and separate FIR residual recovery after existing results; no additional decoder engine or Fortran runtime.
- Replaced the min-sum row calculation with a numerically equivalent linear implementation and shared the FFTW planner lock.
- Preserved the Doppler method and FT4 search/decode/SIC. The R30 supplementary recovery remains available.
- Linux Qt5 full build and aggregate 58 CTest checks passed. Tester offline: 61–62 rows, 56–57 exact JTDX matches versus R30 60/55; live parity remains open. Remaining validation is explicitly recorded.
- Details: [R31 status and handoff](docs/architecture/RELEASE_0_6_4_R31.md).

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

# MadModem changelog

## 0.6.4 source R25 — archivio automatico SSTV RX

Salvataggio PNG automatico delle immagini SSTV completate, con cartella scelta
dall’operatore, nomi UTC/modo senza sovrascritture e apertura della cartella ricevute.
Scrittura in background, esito nello stato RX e salvataggi accodati completati
alla chiusura. RX e QSL da trasmettere rimangono separate. Dettagli e limiti in
`docs/architecture/RELEASE_0_6_4_R25.md`.

## 0.6.4 source R24 — regolazioni retroattive SSTV

Correzioni di fase e allineamento rosso/blu applicate a tutta l’immagine SSTV,
anche a ricezione terminata. Azzera regolazioni ripristina i pixel originali;
comandi raccolti in un pannello richiudibile. Avvio RX senza sincronismo esplicito,
con indicazione della disattivazione di VIS e sync. Tutte le funzioni R23 incluse.
Dettagli e verifiche in `docs/architecture/RELEASE_0_6_4_R24.md`.

## 0.6.4 source R23 — editor e ricezione SSTV

Editor SSTV a schede con progetto modificabile, annulla/ripristina, elenco elementi
e anteprima nel formato TX. Composizione unica editor/TX, dati stazione e DX distinti.
Intestazione VIS in trasmissione e riconoscimento automatico RX indipendente dal sync;
nuova immagine al VIS successivo. WAV annullabile. Dettagli e limiti in
`docs/architecture/RELEASE_0_6_4_R23.md`.

## 0.6.4 source R22 — periodi, CQ e chiamanti

Barre TX rosse/RX verdi secondo il periodo selezionato; serie CQ con conteggio
visibile e ripartenza pulita; nuova finestra Chiamanti (N) per la coda FIFO
in ingresso. Dettagli in `docs/architecture/RELEASE_0_6_4_R22.md`.

## 0.6.4 source R21 — clock audio FT8/FT4

Stima del clock di cattura e ricampionamento FT8/FT4 su griglia UTC12kHz,
con correzione continua e diagnostica separata della latenza. Build Linux,
15simulazioni prolungate,10casi PCM/CRC e12test selezionati passati.
Verifica sulla USB Windows ancora da eseguire. Dettagli in
`docs/architecture/RELEASE_0_6_4_R21.md`.

## 0.6.4 source R20 — stato e controlli FT compatti

Stato e messaggio TX nella scheda Modo, indicatori più sobri e controlli sopra
il waterfall su quattro righe. Tutte le opzioni GPU R19 conservate. Dettagli in
`docs/architecture/RELEASE_0_6_4_R20.md`.

## 0.6.4 source R19 — GPU demodulazione sperimentale

Nuova opzione GPU separata dal drift, valida su tutte le bande, per le
energie dei simboli del decoder ordinario FT8/FT4. Confronto con CPU nativa,
misura dei trasferimenti e ritorno automatico alla CPU. OpenCL software e
parità dei candidati verificati; GPU fisica e guadagno end-to-end non verificati.
Dettagli in `docs/architecture/RELEASE_0_6_4_R19.md`.

## 0.6.4 source R18 — GPU drift sperimentale

- Backend OpenCL FP64 opzionale per la matrice Costas del recupero drift FT8/FT4.
- Riscaldamento asincrono, buffer persistenti, confronto iniziale con CPU multicore
  e ritorno automatico alla CPU per errori, lentezza o divergenze numeriche.
- Nuova casella disattivata inizialmente e log CPU/OpenCL; normale decoder e FEC su CPU.
- Kernel verificato su OpenCL software: 493816 punteggi, errore massimo 5.05e-14.
- GPU fisica e relativo guadagno ancora da misurare. 80 casi decoder/fallback,
  UI, build Linux e 15 test selezionati superati. Rapporto `docs/architecture/RELEASE_0_6_4_R18.md`.

## 0.6.4 source R17 — drift RX VHF/UHF

- Recupero drift solo con frequenza RF almeno 144 MHz; CAT o banda manuale.
- Limite configurabile fino a ±20 Hz/s, mantenendo valore e spunta del profilo.
- Ricerca Costas per simbolo, baseband ampliata e calcolo CPU multicore.
- 40 prove mirate e UI superate; build Linux e 14 CTest selezionati superati.
- GPU DSP non introdotta; dettagli e limiti in `docs/architecture/RELEASE_0_6_4_R17.md`.

## 0.6.4 source R16 — Hellschreiber RX

- Filtro I/Q a quattro poli per Feld Hell e tempi di risposta uniformi fra sample rate.
- FSK-105 con discriminatore di frequenza e correzione del nastro che sbiancava.
- Confronto sintetico R15/R16: errore inferiore in 18 casi base e 54 estesi.
- Continuità audio: 900/900 blocchi; build Linux e 14 test selezionati superati.
- Prove/limiti: `docs/architecture/RELEASE_0_6_4_R16.md`.

## 0.6.4 source R15 — MeteoFax RX

- Filtro I/Q Butterworth a quattro poli al posto del passa-basso a un polo.
- Minore levigatura orizzontale: più dettagli conservati nella carta ricevuta.
- Meno radici quadrate nel percorso per campione; nessuna finestra futura.
- Test automatici immagine, rumore/fading/interferenza, 60/90/120/240 LPM,
  continuità tra blocchi, reset e cambi sample rate.
- Confronto sintetico con R14: migliora 34/36 immagini; due casi a 240 LPM
  e SNR −5 dB peggiorano leggermente. Validazione su registrazioni reali aperta.
- Dettagli: `docs/architecture/RELEASE_0_6_4_R15.md`.


## 0.6.4 source R14 — audio continuity audit

- Separate Q65 period decoding from audio collection, retaining one native engine and serial averaging state.
- Move Radio Telescope band-power windows to the RX owner; reject discontinuous or incomplete measurements.
- Add lifetime queue accounting and explicit FT/Q65 overflow diagnostics.
- Cancel MSK144 background search on reset/shutdown without reducing normal candidate coverage.
- Add real-rate, fragmented-PCM and queue/window regression probes.

## 0.6.4 source R13 — continuous FT capture, QRZ page and selected CQ worklist

- Keep FT period bars and UTC at the bottom of Mode; dedicate the QRZ tab to station information.
- Replace capture-side blocking profile reads with an atomic FT8/FT4 period snapshot while preserving serialized decoder configuration and all DSP search/FEC limits.
- Include the on-air log analysis, show source/build identity at startup and record FFT/tone reuse in live diagnostics.
- Clarify the existing incoming-caller FIFO and add a separate operator-selected CQ worklist. Ctrl/Shift selects CQ rows; adding targets prepares a list; Start/Pause controls execution through the existing sequencer.
- Require a fresh canonical CQ observation for the next target, skip directed/busy or absent stations, respect full-frame automatic timing and preserve one QSO at a time. STOP pauses the list; band/mode changes clear it.
- Preserve CW Native R12 / Neural R13, incoming caller behavior, QRZ settings, R12 incremental analysis and Linux update fixes.

## 0.6.4 source R12 — incremental FT receive, DX sidebar and Linux updates

- Prepare exact FT8 FFT rows and FT4 synchronization-tone energies in a separate worker as capture arrives; reuse only validated original-audio windows, retain ordinary fallback and recompute modified SIC/drift audio.
- Bound the input mailbox, invalidate old capture/slot/config generations, retain immutable snapshots for deferred boundary jobs, and keep capture/decode independent of worker progress or failure.
- Preserve candidate scores, ranking limits, thresholds, LDPC/CRC, normal passes, FT8 coherent tail and existing RX-to-TX timing policy. Include exact candidate parity and measured timing/count comparisons with explicit corpus limits.
- Move FT periods and expanding QRZ station information to the DX tab between Mode and Rotator; retain QRZ account settings and automatic station refresh. Rewrite drift help in all six UI languages.
- Stop DEB installation before APT when dpkg has an unfinished operation, with the administrator repair command. Do not automatically configure unrelated system packages.
- Select Linux update assets by bundle identity before host distribution; support noble derivatives, canonical/legacy portable TAR/ZIP names and precise missing-package/checksum diagnostics. Preserve checksum checks, state, backup and rollback.
- Keep public version 0.6.4 and CW Native R12 / Neural R13.

## 0.6.4 source R11 — FT reply deadlines and additive coherent recovery

- Share the existing full-frame TX admission constants with RX recovery; preserve TX timing and protocol guards.
- Re-evaluate recovery by its complete reply deadline, including GUI/decision reserve, instead of requiring a lower median decode time.
- Retry at most 32 failed first-pass FT8 candidates after every baseline stage, including drift. Append new validated messages without changing baseline output; skip optional work when the original slot deadline is exhausted.
- Count actual new tail messages and log final delivery margin. Preserve the early QSO gate and the FT4 live decoder.
- Include real-audio, expired-deadline, UI/sequencer/waveform and message-retention measurements.
- Retain R10 bars/fonts/DT/cache and CW Native R12 / Neural R13.

## 0.6.4 source R10 — compact FT periods, table fonts and isolated DT correction

- Replace the FT dial with compact period I/II bars; retain digital UTC and scheduler timing.
- Apply table fonts to existing/new CQ, TX and history cells while preserving emphasis.
- Report FT8 DT relative to 0.5 seconds while retaining the absolute internal tie-break used for SIC.
- Cache coherent phasors with bit-identical regression coverage for FT8/FT4.
- Test and reject live coherent recovery variants: no established improvement in both speed and reliability. Keep the validated R9 live paths.
- Include measured comparisons and explicit limits; retain CW Native R12 / Neural R13, updater, CAT and sequencer behavior.

## 0.6.4 source R9 — compact mode panels and earlier FT display

- Display validated passive FT8/FT4 pass results early while preserving canonical sequencing, Auto CQ selection and complete decoder work.
- Deduplicate previews and final rows across the full bounded table, including dense slots.
- Place analogue/digital clock displays side by side, size the TX table to six rows, and adapt QRZ height to its contents.
- Compact the RTTY scope and keep its Mark/Space captions inside the circular display.
- Correct the FT4 sequencer-stop log label while retaining all six translations.
- Preserve 0.6.4 release/update metadata, optional RX drift, QRZ account persistence and CW Native R12 / Neural R13.

## 0.6.4 source R8 — consistent release metadata and early CI validation

- Validate the header string/numeric fields, CMake, Windows resources, UI title and six Qt Help projects together, reporting all inconsistencies in one run.
- Make every distribution build depend on one source validation job before platform dependency installation; include the check in the manual source workflow.
- Align the Designer title, six translation dictionaries and current installation documentation with 0.6.4.
- Retain all R7 updater behavior and all modem, audio, runtime, QRZ and sequencer implementation files unchanged.

## 0.6.4 source R7 — all published release updates

- Inspect the paged published-release list, including pre-releases, independently of Latest.
- Compare numeric public version and distribution build identity; skip drafts, installed/older builds and public-version downgrades.
- Embed the distribution workflow build number/commit in each platform binary and stamp release notes while preserving custom text.
- Show installed/available build identity and the pre-release label before asking to download/install.
- Verify portable replacement version and build, retaining backup, user state and rollback.
- Defer automatic startup checks while operations are active; preserve the user's startup-check switch.
- Advance the public version to 0.6.4 and retain the R6 modem and QRZ changes.


## 0.6.3 source R6 — optional FT drift RX and QRZ account settings

- Add bounded, marker-local linear-drift recovery after unchanged standard FT8/FT4 passes; use Costas evidence, an analytic audio copy and normal LDPC/CRC validation.
- Keep recovery off by default, expose an absolute 0.25–5 Hz/s limit in Settings → FT8 / FT4, and report verified estimates and recovery cost in the runtime log.
- Predict the next local search from a recent verified estimate for the same selected DX; leave RX/TX markers and CAT under operator control.
- Keep drifting recovery results out of constant-frequency SIC; retain all baseline results.
- Move QRZ account fields to Settings → QRZ.com while preserving the main station-information panel.
- Save username/password in the local profile and reuse them after restart; preserve Cancel semantics and invalidate old sessions/cache after account changes.
- Retain R5 reply-window/QRZ fixes, R4 updates, Windows UCRT64/Hamlib fixes, CW Native R12 / Neural R13 and the pinned FT clock.


## 0.6.3 source R5 — FT replies and selected QRZ station

- Preserve the final opposite RX period before declaring an FT8/FT4 QSO unanswered; consume each TX attempt once and invalidate obsolete timeouts.
- Publish validated active-QSO results between existing passes and during FT8 residual recovery while retaining remaining decoder work, thresholds, candidates and recovery budgets.
- Apply coalesced FT DX/marker updates after the running job without blocking the capture-owner thread.
- Remove repeated bounds/COW work from FT8 cancellation loops with sample-for-sample parity checks.
- Refresh QRZ explicitly when the FT correspondent changes under blocked widget signals.
- Preserve R4 updater support, R3 Windows/Hamlib fixes, Native R12, Neural R13 and the pinned clock.

## 0.6.3 source R4 — updates from promoted builds

- Accept automatic `build-VERSION-N` tags after a release is promoted to stable and Latest, alongside numeric version tags.
- Keep draft/prerelease exclusion, numeric version comparison, package selection and verified downloads. Release titles remain display text.
- Retain the R3 Windows/Hamlib fixes, Native R12, Neural R13 and the FT clock layout.

## 0.6.3 source R3 — bundled Hamlib Windows build

- Added the missing assert header in Hamlib's Windows timer and used GCC's constructor attribute for startup initialization; retained the MSVC CRT path.
- Normalized the Autoconf header/function lists and separated two concatenated BSD header names, eliminating malformed feature probes.
- Kept UCRT64 packaging, both CW engines and the FT clock layout unchanged.

## 0.6.3 source R2 — Windows UCRT64 packaging

- Migrated the installer workflow and its complete package list from deprecated MINGW64 to UCRT64.
- Aligned build/deploy paths, isolated Hamlib build/install prefixes by environment and removed cross-environment DLL fallback.
- Kept public version 0.6.3, both CPU variants, installer/portable asset names, Native R12, Neural R13 and the FT clock fix.

## 0.6.3 — Native R12 and Neural R13

- Two CW RX A/B choices only: Native R12 (default) and Neural R13 (experimental). Native algorithms, R13 numerical weights and R13 CTC search parameters are unchanged.
- Removed the older neural runtime paths and their bundled weights. Old saved CTC/Envelope selections migrate to Neural R13; unknown settings fall back to Native R12.
- Pinned the FT clock outside the mode settings scroll area, above QRZ. Removed the nested FT sequencer summary and duplicated period/next-message labels above the clock. Sequencer details remain in the clock tooltip; the live TX banner and message selection continue to update.
- Updated public version, Windows resources, localized help and UI strings to 0.6.3. Removed obsolete training checkpoints, caches and generated build products from this source delivery.
- Validation details: `docs/architecture/RELEASE_0_6_3.md`. Historical R12/R13 benchmark reports describe those earlier source packages.


## 0.6.2 source R13 — adapted neural CW with causal CTC search

- Added optional MM Neural Search for RX A/B, with a separately adapted MM checkpoint, causal envelope frontend and four-hypothesis acoustic CTC search. Native stays default; legacy CTC and Envelope remain available.
- New QSO errors: Envelope 96/1,017 → Search 21/1,017. New clean ARRL clips: 152/592 → 28/592. Existing RF development: 127/249 → 92/249. Native remains ahead on clean QSO/ARRL.
- Vectorized the unchanged-weight inference core with bit-identical output; full-app CPU timings remain variable and do not establish a general speed win over Native.
- Added source-backed research, 118 minutes of independently pulse-audited archive material acquisition, training/replay tools, comparative results and causal streaming checks.
- Full Linux build, Native regressions, Search A/B routing and four-choice UI checks passed. Details: `docs/architecture/CW_NEURAL_R13_RESULTS.md`.

## 0.6.2 source R12 — native CW acquisition and processing cost

- Fixed startup loss of the first Morse element after a short noise prelude; added adaptive character/word spacing from confident completed gaps.
- Cached exact duration hazards and spectral work. Native CPU cost fell 18.9% in the documented local comparison.
- Added optional MM Neural Envelope RX normalization while preserving legacy Neural CTC and the Native default. No new weights were promoted.
- Fresh synthetic QSO: native errors 13/489 → 0/489. Existing RF development excerpts: 143/249 → 134/249. These scopes are distinct; independent new radio validation remains open.
- Added actual-route comparison tooling and regressions. Details and limits: `docs/architecture/CW_R12_CHECKS.md`.

## 0.6.2 source R6 — CW neural receiver refinement

- Expanded independent training with receiver-filtered messages, narrowband noise,
  steady carriers and more word boundaries. Increased blank and CTC supervision.
- Kept Native as the default and retained the saved RX A/B engine selector.
- Added reproducible refinement and frozen-model comparison tools, with the R5
  training checkpoint retained for lineage and baseline comparisons.
- WebSDR captures are evaluated separately from labelled synthetic tests; no
  unverified decoder predictions are used as training truth.
- Measurements and remaining limitations: `docs/architecture/CW_NEURAL_R6.md`.

## 0.6.2 source R5 — selectable CW receive engine

- Added persistent RX A/B engine selection: Native (default) or independently
  trained MM Neural CTC (experimental). Band skimmer remains native.
- Added a causal temporal-CNN/CTC C++ receiver with bundled original weights and a
  reproducible synthetic-audio training/export tool; no external ML runtime.
- Reset neural state on engine/tone changes, receiver clear, capture gaps and
  local TX resume. TX audio and serial KEY behavior is independent.
- Experimental model trained for 12–36 WPM. Native filter/AFC/WPM controls are
  disabled for this engine. Real-radio and native Windows/macOS validation remain open.
- See `tools/cw_neural/README.md`, `modems/cw/neural/model/MODEL_CARD.json`,
  and `docs/architecture/CW_NEURAL_R5.md` for origin, measurements and limitations.


0.6.2 R4: native CW serial KEY with optional same-port PTT, exclusive CAT port handoff, interruptible timing/Stop and failed-release recovery; shared Morse plan and consistent 80-WPM TX range.

0.6.2 R3: QRZ selected-station panel; clarified local callsign recognition; resizable waterfall with rounded frequency ticks; scrollable mode controls and explicit audio-CW TX explanation.

0.6.2 R2: isolated/adaptive CW band worker; GitHub stable-release updater with verified downloads and portable backup/rollback; IK7IMK Nick tester credit; corrected main installer license footer.

## 0.6.2

RTTY band skimmer with independent receivers, Text/CQ waterfall views and bounded worker processing. Shared CW/RTTY overlay and CQ recognition; persistent pair acquisition suppresses sideband duplicates. Full multilingual help refresh, source cleanup and aligned release metadata. See [release notes](RELEASE_NOTES.md) for operating details.

## 0.6.1

Optional installers alongside portable archives; managed user-data storage and migration; Windows CPU selection; bundled Linux Qt/Hamlib runtime; macOS package/deployment corrections; release asset publication. Resizable windows, clearer FT/CQ/logbook controls, editable rotator memories and standard serial speeds. CW band skimmer and playback-based CW text highlighting were introduced in subsequent source updates.

## 0.6.0

CW acquisition/filter/AFC work; RTTY letters/figures, AFC and contest macros; FT scheduling and TX audio recovery; asynchronous CAT/PTT; RX worker separation and audio-gap recovery. Logbook parsing and contest edition export with QSO preview; narrow-band waterfall and CAT configuration improvements.

Historical build logs, screenshots and per-revision audit reports are not distributed with source releases. Version control preserves earlier implementations. Licenses, attribution and maintained tests remain part of the source.
