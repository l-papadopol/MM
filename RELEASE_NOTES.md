# MadModem 0.6.4

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
