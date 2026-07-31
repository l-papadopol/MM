# MadModem 0.5.78
## All-in-one digital modem and station hub for amateur radio

MadModem is a Qt/C++ amateur-radio application for Linux, Windows and macOS.
The current source tree contains one native CW receive implementation and no
legacy CW decoder, hidden fallback, external CW core or obsolete experimental
branch.

MadModem è un'applicazione radioamatoriale Qt/C++ per Linux, Windows e macOS.
Il sorgente corrente contiene una sola implementazione CW nativa, senza decoder
CW legacy, fallback nascosti, core CW esterni o rami sperimentali obsoleti.

## Main features / Funzioni principali

- FT8 and FT4 RX/TX with integrated sequencer and offline WAV analysis.
- RTTY, BPSK/QPSK, MFSK, Feld Hell, CW, SSTV and WEFAX/MeteoFax work areas.
- MSK144 and Q65 development modes using GPL-compatible in-tree components.
- Hamlib CAT/PTT and rotator control.
- Integrated QSO logbook, ADIF import/export, DXCC lookup and QSO map.
- Radio Telescope receive-only scanning and CSV export.
- Runtime UI and help in English, Italian, French, German, Norwegian and Czech.

- FT8 e FT4 RX/TX con sequencer integrato e analisi WAV offline.
- Aree operative RTTY, BPSK/QPSK, MFSK, Feld Hell, CW, SSTV e WEFAX/MeteoFax.
- Modi MSK144 e Q65 in sviluppo con componenti GPL compatibili inclusi.
- Controllo CAT/PTT e rotore tramite Hamlib.
- Registro QSO, import/export ADIF, ricerca DXCC e mappa QSO.
- Scansione Radio Telescope in sola ricezione ed esportazione CSV.
- Interfaccia e guida in inglese, italiano, francese, tedesco, norvegese e ceco.

## Native CW receiver / Ricevitore CW nativo

MadModem ships a single clean-room C++ CW path:

```text
full-band carrier discovery
→ independent exact-tone RX A / RX B
→ complex baseband and bounded AFC
→ one-millisecond soft MARK probability
→ Bayesian joint timing decoder
→ continuous text in the RX panes
```

Measured MARK and SPACE intervals are not resized or fused. SNR controls
observation confidence, not Morse duration. WPM is a weak acquisition hint and a
derived display value, not a rigid decoder clock. QSB is represented as
uncertainty rather than being forced immediately into a Morse space.

MadModem include un solo percorso CW C++ riscritto nativamente. Gli intervalli
MARK e SPACE misurati non vengono allungati, accorciati o fusi. L'SNR modifica
la confidenza dell'osservazione, non la durata Morse. Il WPM è solo un debole
riferimento iniziale e un valore derivato per la UI. Il QSB viene trattato come
incertezza, non trasformato automaticamente in una pausa Morse.

RX A and RX B are fully independent. The waterfall shows carrier-lane labels and
markers only; decoded letters remain in the continuous receiver panes.

RX A e RX B sono completamente indipendenti. Il waterfall mostra solo marker e
lane delle portanti; le lettere decodificate restano nei pannelli continui RX.

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

The passband-aware leveler ignores digitally silent regions outside the actual
audio band, uses a stable display span and prevents a narrow strong carrier from
pumping the whole waterfall orange. The zoom/pan bar is below the frequency
labels.

Il livellatore ignora le zone digitalmente silenziose fuori dalla banda audio
reale, mantiene una dinamica stabile e impedisce a una portante stretta e forte
di rendere arancione l'intero waterfall. La barra zoom/pan è sotto le label di
frequenza.

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

The CW test is pure C++ and checks clean messages, a wrong initial WPM hint,
short QSB notches, noise-only input and two carrier lanes 25 Hz apart. Real
acceptance remains based on recorded and on-air signals.

Il test CW è C++ puro e verifica messaggi puliti, WPM iniziale volutamente
errato, brevi notch QSB, solo rumore e due lane distanti 25 Hz. L'accettazione
finale resta basata su registrazioni e segnali reali in aria.

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
