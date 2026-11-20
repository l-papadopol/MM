# MadModem 0.6.6

Questa versione integra i contributi di **Costantino Cerrotta, IC8TEM** nel
progetto MadModem di **Lucian-Ioan Papadopol, IZ6NNH**.

- Noise blanker RX NB2/LPC e notch delle portanti MSK144.
- Strumenti potenza/ROS/ALC, preset e memoria RF per banda, potenza Tune e protezione ROS.
- Fonti del locatore, lista utenti LoTW con indicatori e filtro FT.
- Richiamo FT del DX decodificato, reset dei soli indicatori già lavorato,
  durata dei ping MSK144 e tema YA-NEC.
- Split Rig/Fake It anche per MSK144 e Q65 e miglioramenti CAT Icom.
- Ripristino dell'identità Windows originale: MadModem, stesso AppId,
  installazione predefinita in `%LOCALAPPDATA%\Programs\MadModem` e dati personali
  nelle cartelle AppData di sistema.
- Aggiornamenti dal repository `l-papadopol/MM`, crediti aggiornati e documentazione consolidata.

Sono mantenuti correzione Doppler, decoder, sequencer, Evil Auto QSO, tracce
operative e numero di build nel titolo. Il numero di build viene assegnato dal
workflow di distribuzione GitHub.

Consultare la [guida agli strumenti](docs/FEATURES.md) e le
[istruzioni sui dati di installazioni precedenti](docs/INSTALLATION.md).
