# MadModem 0.6.6

Questa versione integra i contributi di **Costantino Cerrotta, IC8TEM** nel
progetto MadModem di **Lucian-Ioan Papadopol, IZ6NNH**.

### Revisione R10

In FT8 la R10 controlla i recuperi deboli sovrapposti a un segnale più forte
verificando il messaggio sull'audio residuo. Controlla anche la forma dei
nominativi lunghi nei CQ non standard recuperati via OSD.

Il [confronto tra sette decoder](README.md#benchmark-ft8-e-ft4-della-r10)
riporta i risultati sui WAV di IC8TEM, i falsi nei sintetici e i tempi.

### Novità della 0.6.6

- Con CAT Hamlib, la preferenza USB o Data/Pkt si applica anche in ricezione
  per FT8, FT4, Q65 e MSK144. Vale al collegamento, al cambio di modo e durante
  il QSY delle bande FT.
- Recupero CQ in FT8 e FT4 tramite combinazione delle metriche di due slot della stessa parità.
- NB2 Auto rileva i possibili impulsi e avvia NB2 solo nelle finestre interessate.
  Il ritardo audio resta costante.
- Noise blanker NB2 ottimizzato anche per CW, RTTY e i loro skimmer. LPC viene
  sostituito da NB2 normale nelle configurazioni esistenti. Notch delle portanti MSK144.
- Strumenti potenza/ROS/ALC, preset e memoria RF per banda, potenza Tune e protezione ROS.
- Fonti del locatore, lista utenti LoTW con indicatori e filtro FT.
- Richiamo FT del DX decodificato, reset dei soli indicatori già lavorato,
  durata dei ping MSK144 e tema YA-NEC.
- Split Rig/Fake It anche per MSK144 e Q65 e miglioramenti CAT Icom.
- Ripristino dell'identità Windows originale: MadModem, stesso AppId,
  installazione predefinita in `%LOCALAPPDATA%\Programs\MadModem` e dati personali
  nelle cartelle AppData di sistema.
- Aggiornamenti dal repository `l-papadopol/MM`, crediti aggiornati e documentazione consolidata.
- STOP FT8/FT4 resta attivo durante il richiamo automatico. Il blocco ROS si applica
  anche al CW seriale e richiede misure valide consecutive per scattare.
- Tune attende la conferma CAT della potenza ridotta. MSK144 e Q65 verificano
  la scadenza del periodo prima di avviare l'audio.
- GitHub pubblica i pacchetti dopo le verifiche di tutte le piattaforme selezionate.

Sono mantenuti correzione Doppler, decoder, sequencer, Evil Auto QSO, tracce
operative e numero di build nel titolo. Il numero di build viene assegnato dal
workflow di distribuzione GitHub.

Consultare la [guida agli strumenti](docs/FEATURES.md) e le
[istruzioni sui dati di installazioni precedenti](docs/INSTALLATION.md).
