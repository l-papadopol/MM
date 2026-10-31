# R31 LOGZIP1 — diagnostica completa compressa

La diagnostica completa produceva JSONL UTF-8 non compresso e poteva raggiungere centinaia di megabyte. La nuova scrittura conserva tutti gli eventi accettati, campi e timestamp: non sono stati aggiunti filtri, campionamenti o tagli del log.

## Formato e prestazioni

I nuovi file terminano in `.jsonl.gz`. Sono gzip standard, scritti direttamente senza un file JSONL temporaneo. La compressione zlib a livello 1 avviene nel thread già dedicato alla diagnostica. Due buffer da 64 KiB e lo stato limitato del compressore evitano di conservare una sessione intera in memoria. Il limite preesistente della coda eventi rimane invariato.

Gli eventi sono ancora redatti prima della serializzazione. JSON e compressione sono eseguiti dal writer. Le scritture e gli aggiornamenti del flusso sono raggruppati a intervalli di circa 250 ms, anche durante batch lunghi. Una richiesta esplicita di flush mantiene la barriera sugli eventi precedenti. La chiusura normale svuota la coda e finalizza il trailer/CRC gzip. Errori, eventi persi e sessioni incomplete continuano a essere segnalati.

Su un estratto reale della sessione Windows già fornita dall'utente, 4.369.853 byte diventano 451.752 byte con gzip livello 1: riduzione dell'89,66%, con confronto byte per byte dopo decompressione. È una misura sull'estratto, non una promessa di rapporto fisso per ogni sessione. Il rapporto varia con i dati.

## Uso e recupero

Chiudi MadModem o disattiva la registrazione per avere un gzip finalizzato. Il file contiene lo stesso JSONL dopo estrazione; i log `.jsonl` già esistenti restano validi e non vengono modificati.

Il lettore incluso usa soltanto la libreria standard Python:

```sh
python3 tools/read_session_log.py MadModem_sessione.jsonl.gz -o sessione.jsonl
```

Il file di destinazione deve essere nuovo: il comando non sovrascrive file esistenti. Per file ancora aperti o interrotti da un crash:

```sh
python3 tools/read_session_log.py MadModem_sessione.jsonl.gz --recover -o prefisso_recuperato.jsonl
```

Il recupero emette solo righe complete disponibili e restituisce codice 2 con avviso quando il flusso gzip è incompleto o danneggiato. Questo non trasforma un crash in una sessione dichiarata completa. In caso di arresto improvviso possono mancare gli ultimi dati non ancora scaricati dai buffer; il test di processo fatale verifica che il flush esplicito conservi il messaggio fatale. L'estrazione normale verifica il trailer gzip tramite il lettore standard.

## Dipendenze e compatibilità

CMake richiede ora zlib e collega `ZLIB::ZLIB`. Sono dichiarati `zlib1g-dev` in Ubuntu, `zlib-devel` in Fedora e `mingw-w64-ucrt-x86_64-zlib` in Windows/UCRT64. I controlli di packaging Windows già raccolgono le DLL dipendenti. Su macOS si usa la zlib della piattaforma/SDK tramite CMake. Le spiegazioni in Impostazioni e nell'aiuto sono aggiornate in sei lingue.

Durante l'audit delle traduzioni sono state riallineate anche quattro chiavi dei messaggi di log TXFIX2, senza modificare il sequencer.

## Verifica

Build Linux Qt5 dell'applicazione riuscita. Quattro CTest mirati passano: diagnostica della sessione, contabilità audio, sequencer FT e controlli del percorso FT. Il test nativo ricostruisce gli eventi con un decoder gzip indipendente e controlla CRC, ordine, 3.000 eventi concorrenti, flush esplicito e periodico, cartelle Unicode, riavvio senza sovrascrittura, redazione dei segreti dopo decompressione, overflow e conservazione del prefisso dopo qFatal in un processo figlio.

Il lettore Python è verificato su JSONL precedente, gzip, flusso troncato, membri concatenati e rifiuto della sovrascrittura. Le traduzioni superano l'audit delle sei lingue.

I sorgenti RX, Doppler, tutti i modem e il controllo TX/coda della TXFIX2 sono identici byte per byte. Questa modifica riguarda la diagnostica e include TXFIX2 e WINFIX1. Non sono stati ripetuti benchmark dei decoder né prove radio. Build Windows/macOS e dispositivi fisici restano da confermare tramite CI e sulla stazione.
