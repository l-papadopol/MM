> Current CW comparison checkpoint: live Bayesian posterior beam; see `docs/CW_BAYESIAN_BEAM_DECODER_0_5_78.md`.

# MadModem 0.5.78
## All-in-one digital modem and station hub for amateur radio

MadModem is a Qt/C++ amateur-radio application for Linux, Windows and macOS.
The current source tree contains one native CW receive implementation. Carrier
discrimination, timing-family estimation and probabilistic Morse sequence
decoding are independent live tasks; previous experimental decoder branches
have been removed.

MadModem è un'applicazione radioamatoriale Qt/C++ per Linux, Windows e macOS.
Il sorgente corrente contiene una sola implementazione CW nativa. La
discriminazione della portante, la stima delle famiglie temporali e la decodifica
probabilistica della sequenza Morse sono task live indipendenti; i precedenti
rami sperimentali sono stati rimossi.

## Main features / Funzioni principali

- FT8 and FT4 RX/TX with integrated sequencer and live adaptive decoding.
- RTTY, BPSK/QPSK, MFSK, Feld Hell, CW, SSTV and WEFAX/MeteoFax work areas.
- MSK144 and Q65 development modes using GPL-compatible in-tree components.
- Hamlib CAT/PTT and rotator control.
- Integrated QSO logbook, ADIF import/export, DXCC lookup and QSO map.
- Radio Telescope receive-only scanning and CSV export.
- Runtime UI and help in English, Italian, French, German, Norwegian and Czech.

- FT8 e FT4 RX/TX con sequencer integrato e decoder live adattivo.
- Aree operative RTTY, BPSK/QPSK, MFSK, Feld Hell, CW, SSTV e WEFAX/MeteoFax.
- Modi MSK144 e Q65 in sviluppo con componenti GPL compatibili inclusi.
- Controllo CAT/PTT e rotore tramite Hamlib.
- Registro QSO, import/export ADIF, ricerca DXCC e mappa QSO.
- Scansione Radio Telescope in sola ricezione ed esportazione CSV.
- Interfaccia e guida in inglese, italiano, francese, tedesco, norvegese e ceco.

## Adaptive FT runtime / Runtime FT adattivo

FT8/FT4 use a persistent worker pool sized from the processors available to the
process rather than a fixed worker limit. Gate, boundary and OSD budgets adapt
between slots using audio-queue and GUI/waterfall latency. Capture generations
and timestamp checks prevent stale work from contaminating later slots.

FT8/FT4 usano un pool persistente dimensionato sulle risorse realmente
disponibili. I budget di gate, boundary e OSD si adattano fra gli slot usando la
latenza audio e della GUI/waterfall. Generazioni di cattura e controlli temporali
impediscono a lavori obsoleti di contaminare gli slot successivi.

## Native CW receiver / Ricevitore CW nativo

The live path is split into four responsibilities for each RX A/RX B receiver:

```text
selected complex carrier
  -> CwCarrierDiscriminator
       timestamped MARK/SPACE runs + carrier quality
  -> CwRelativeTimingTask
       robust dit/dah and spacing-family estimation
  -> CwMorseBeamDecoder
       per-path dit/dah/gap duration posteriors
       credible-posterior stable-prefix commit
  -> continuous RX A/RX B text
```

The exact-tone DSP, bounded neighbouring-lane separation, narrow fourth-order
I/Q filtering, AFC and carrier/noise measurements remain unchanged. Timing is
updated only by credible short/long pairs; isolated fragments cannot move WPM.
SPACE observations train element, character and word families by relative
likelihood rather than one hard threshold. Measured family centres are scored
alongside canonical 1/3/7-unit centres at the acquired dit scale, with soft
minimum-duration penalties: an initial WPM hint that is far too fast or slow no
longer turns ordinary character gaps into words or intra-element jitter into
character boundaries.

The Morse layer keeps a bounded beam of concurrent dot/dash and boundary paths.
Each path owns Normal-Inverse-Gamma posteriors for dit, dah and the three SPACE
families. Predictive Student-t likelihoods are mixed with the MARK/SPACE, QSB,
noise and carrier-centering probabilities exported by the discriminator. Its
short observation window is replayed whenever the external timing prior changes,
so a wrong initial WPM hint cannot permanently classify the first dash. Text is
published from shared credible posterior mass or a decisively dominant path. The
delay is normally one following element/gap, not phrase-level or end-of-
transmission decoding. No dictionary or language model is used.

Carrier continuity is no longer a fixed 8.5/12-dit timeout. Each receiver keeps
a session probability with fast attack and a decay constant derived from the
learned word-space family and prior carrier stability. Normal/Farnsworth word
gaps remain alive, while a dead noisy lane loses qualification progressively.
A bounded timeout remains only as a final safety for confirmed carrier loss.

The Runtime Log shows the exact committed pattern plus Bayesian hypothesis count,
sequence confidence, best posterior and best/second posterior odds. Auto-WPM remains bounded to 5-50 WPM. All processing is in
the production RX live path; the pure-C++ tests only validate that same code.

Il percorso CW live separa discriminatore di portante, stima robusta del timing
e decoder Morse a ipotesi concorrenti. Le osservazioni non ancora pubblicate
vengono ricalcolate quando cambia il clock, evitando che il WPM iniziale errato
condanni il primo elemento. Il testo viene emesso con un piccolo ritardo solo
quando le ipotesi migliori concordano. La continuità della sessione è
probabilistica e dipende dallo spazio-parola appreso, non da un timeout fisso in
dit.

The implementation was informed by architectural study of the two open-source
CW skimmer projects supplied for comparison. No source code from either project
was copied into MadModem.

## Direct RX WAV recording / Registrazione WAV RX

Use **File → Start RX audio recording…** (`Ctrl+Shift+R`) to record the exact
normalized mono stream used by the waterfall and decoders. The recorder writes
16-bit PCM WAV at the active input sample rate and closes safely on Stop RX,
audio error, sample-rate change or application shutdown.

Usare **File → Avvia registrazione audio RX…** (`Ctrl+Shift+R`) per registrare lo
stesso flusso mono normalizzato usato dal waterfall e dai decoder. Il file WAV è
PCM 16 bit al sample rate attivo e viene chiuso in sicurezza all'arresto RX, in
caso di errore audio, cambio sample rate o chiusura dell'applicazione.

## Waterfall

The waterfall uses WSJT-X-style per-row Flatten processing: the lowest ten
percent of each of ten frequency segments define a fourth-order lower-envelope
fit that is subtracted before fixed gain/zero colour mapping. Receiver AGC steps
therefore disappear in the next row instead of pumping the occupied band orange.
A persistent passband detector excludes digitally silent monitor bins. Downward
scrolling uses a circular OpenGL texture with one-row presentation, HiDPI viewport
correction and a QImage fallback.

Il waterfall usa il Flatten per-riga derivato da WSJT-X: il dieci per cento più
basso di ciascuno dei dieci segmenti di frequenza definisce l'inviluppo inferiore
polinomiale di quarto ordine, sottratto prima della mappatura colore a guadagno
fisso. Le variazioni dell'AGC del ricevitore vengono quindi compensate nella riga
successiva senza rendere arancione tutta la banda. Il rilevatore persistente della
banda esclude i bin digitalmente silenziosi; scorrimento OpenGL, HiDPI e fallback
QImage restano invariati.

## Building from source / Compilazione

```bash
./build_all.sh
```

Requirements: CMake, a C++17 compiler, Qt 5 or Qt 6 development packages,
Hamlib and the normal audio/serial development tools. FFTW3 is required for the
full optional Q65 bridge.

Servono CMake, un compilatore C++17, i pacchetti di sviluppo Qt 5 o Qt 6,
Hamlib e i normali strumenti di sviluppo audio/seriale. FFTW3 è necessario per
il bridge Q65 completo opzionale.

## Regression checks

```bash
./scripts/run_cw_native_regression.sh
./scripts/run_waterfall_leveler_regression.sh
```

The CW test compiles the production discriminator, temporal worker, timing
model and beam decoder. It covers replay after a wrong initial clock, the real
30.5 WPM/8.8-dit OG50YL case, additive noise, deep QSB notches, human timing, a
non-ideal 2.45 dash ratio, a stronger known carrier at +70 Hz, post-message noise
and noise-only suppression. Recorded and on-air signals remain the decisive
acceptance test.

Il test CW compila il discriminatore, il worker temporale e il decoder relativo
di produzione. Verifica WPM iniziale errato, rumore, notch QSB profondi, timing
umano, rapporto linea/punto 2,45, una portante nota più forte a +70 Hz e la
soppressione del solo rumore. L'accettazione decisiva resta su registrazioni e
segnali in aria.

## Documentation

- `RELEASE_NOTES.md` — current release notes
- `CHANGELOG.md` — current concise changelog
- `docs/cwskimmer/` — native CW architecture and tests
- `docs/help/` — localized user help
- `THIRD_PARTY_NOTICES.md` — source origins and third-party notices

## Safety notes / Note di sicurezza

Verify PTT, CAT mode, TX audio routing, frequency and antenna direction before
transmitting. Configure rotator limits and emergency-stop behaviour before any
automatic scan.

Verificare PTT, modo CAT, instradamento audio TX, frequenza e direzione
dell'antenna prima di trasmettere. Configurare limiti del rotore e arresto di
emergenza prima di ogni scansione automatica.

## Author and license

MadModem is developed by **Lucian-Ioan Papadopol, IZ6NNH** and released under
the **GNU GPLv3**. See `LICENSE.md`, `COPYING` and `THIRD_PARTY_NOTICES.md`.
