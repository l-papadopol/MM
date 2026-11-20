# MadModem 0.6.5 IC8TEM

Edizione IC8TEM di [MadModem](https://github.com/l-papadopol/MM) 0.6.5 di IZ6NNH, con le
funzioni di [MSHV-IC8TEM](https://github.com/ic8tem/MSHV-IC8TEM).

- **Noise blanker** (menu Decode › Noise Blanker): toglie gli impulsi (QRN, scintille,
  alimentatori switching) dall'audio prima di ogni decodificatore: FT8, FT4, Q65, MSK144,
  CW, RTTY, PSK, MFSK, Hell, SSTV, WEFAX. Metodo NB2 alla NR0V (consigliato) o predizione
  lineare di DL2FW, tre soglie. I ping MSK144, il CW manipolato e i toni FT/Q65 non vengono
  mai toccati. Aggiunge 48 ms di ritardo, compensati negli orari di decodifica. Spento di serie.
- **Notch delle portanti MSK144 (ZAP)** (menu Decode): portanti fisse e birdie tolti con
  notch di 10 Hz trovati sugli ultimi 5 secondi; un segnale MSK continuo non viene mai filtrato.
- **Potenza e ROS** (menu Strumenti): potenza RF via CAT Hamlib con preset, −1/+1 e memoria
  per banda; misure Po, ROS e ALC in trasmissione; Tune a potenza ridotta; protezione ROS.
- **Fonti del locatore** (menu Strumenti): se il DX non ha locatore, MadModem lo cerca nelle
  decodifiche ricevute, in CALL3.TXT (WSJT-X Improved e W7GJ EME), in JTDX e online (G7VRD).
- **Utenti LoTW** (menu Strumenti): pallino accanto agli utenti LoTW nella lista FT e filtro
  "solo utenti LoTW".
- **FT: richiama il DX quando viene decodificato** e **Reset fittizio del log** (menu
  Strumenti); colonna "ms" con la durata del ping nella lista MSK144.
- **Tema YA-NEC** (Impostazioni › Logbook / colori FT › Tema): tema chiaro con lo stile di
  YA-NEC: barre blu scuro, pulsanti a pillola, riquadri bianchi arrotondati.
- Gli aggiornamenti automatici arrivano da questo fork.

**English.** Noise blanker before every decoder (Decode › Noise Blanker: NR0V NB2 style or
DL2FW linear prediction, three thresholds; pings, keyed CW and FT/Q65 tones are never
blanked) and MSK144 carrier notch (ZAP). Tools menu: Power and SWR (RF power, Po/SWR/ALC
meters, SWR protection), locator sources for the DX call, LoTW users, FT call-again,
fictitious log reset. MSK144 ping duration column. YA-NEC light theme (Settings ›
Logbook / FT colours › Theme). Updates come from this fork.

Pacchetti / Packages: Windows (installer and portable), Ubuntu 24.04 / Mint 22 (RUN, DEB,
portable), Fedora 43 (RUN, RPM, portable), macOS 15+ Intel and Apple Silicon (PKG, DMG, ZIP).
The macOS packages are not signed: right-click the app and choose Open the first time.
