# Reviewed UI phrases for the optional FT drift recovery and persistent QRZ account.
PHRASES = [
    ("Username", "Nome utente", "Nom d’utilisateur", "Benutzername", "Brukernavn", "Uživatelské jméno"),
    ("Password", "Password", "Mot de passe", "Passwort", "Passord", "Heslo"),
    ("RX drift compensation", "Compensazione drift RX", "Compensation de dérive RX", "RX-Driftkompensation", "RX-driftkompensasjon", "Kompenzace driftu RX"),
    ("Recover drifting signals near the RX marker (experimental)",
     "Recupera segnali con drift vicino al marcatore RX (sperimentale)",
     "Récupérer les signaux dérivants près du marqueur RX (expérimental)",
     "Driftende Signale nahe dem RX-Marker wiederherstellen (experimentell)",
     "Gjenfinn drivende signaler nær RX-markøren (eksperimentelt)",
     "Obnovovat signály s driftem poblíž RX markeru (experimentální)"),
    ("Maximum drift (±)", "Drift massimo (±)", "Dérive maximale (±)", "Maximale Drift (±)", "Maksimal drift (±)", "Maximální drift (±)"),
    ("The account is saved in the local profile and reused after restart. QRZ XML access is required for complete station data.",
     "L’account viene salvato nel profilo locale e riutilizzato al riavvio. Per i dati completi della stazione serve l’accesso XML QRZ.",
     "Le compte est enregistré dans le profil local et réutilisé au redémarrage. Les données complètes nécessitent un accès XML QRZ.",
     "Das Konto wird im lokalen Profil gespeichert und nach dem Neustart wiederverwendet. Vollständige Stationsdaten erfordern QRZ-XML-Zugang.",
     "Kontoen lagres i den lokale profilen og brukes etter omstart. Fullstendige stasjonsdata krever QRZ XML-tilgang.",
     "Účet se ukládá do místního profilu a použije se po restartu. Úplné údaje o stanici vyžadují přístup QRZ XML."),
    ("Select the signal with the RX marker. This option helps decode it when its frequency changes during reception. You control the transmit frequency. Results appear in the runtime log.",
     "Posiziona il marcatore RX sul segnale da seguire. Questa opzione aiuta a decodificarlo se la frequenza cambia durante la ricezione. La frequenza TX resta quella impostata. I risultati compaiono nel registro.",
     "Placez le marqueur RX sur le signal à suivre. Cette option aide à le décoder si sa fréquence change pendant la réception. Vous réglez la fréquence d’émission. Les résultats apparaissent dans le journal.",
     "Setzen Sie den RX-Marker auf das gewünschte Signal. Diese Option hilft beim Dekodieren, wenn sich seine Frequenz während des Empfangs ändert. Die Sendefrequenz stellen Sie selbst ein. Ergebnisse stehen im Laufzeitprotokoll.",
     "Plasser RX-markøren på signalet du vil følge. Dette alternativet hjelper med dekoding når frekvensen endres under mottak. Du velger sendefrekvensen selv. Resultatene vises i loggen.",
     "Umístěte RX marker na sledovaný signál. Tato volba pomáhá s dekódováním, pokud se jeho frekvence během příjmu mění. Vysílací frekvenci nastavujete sami. Výsledky se zobrazují v protokolu."),
]
QRZ_SETTINGS = {
    'it': 'Configura l’account QRZ in Impostazioni → QRZ.com. I dati completi richiedono l’accesso XML.',
    'fr': 'Configurez le compte QRZ dans Paramètres → QRZ.com. Les données complètes nécessitent un accès XML.',
    'de': 'QRZ-Konto unter Einstellungen → QRZ.com konfigurieren. Vollständige Daten erfordern XML-Zugang.',
    'no': 'Konfigurer QRZ-kontoen i Innstillinger → QRZ.com. Fullstendige data krever XML-tilgang.',
    'cs': 'Nastavte účet QRZ v Nastavení → QRZ.com. Úplné údaje vyžadují přístup XML.',
}

PHRASES.append(('Active only at 144 MHz and above, up to ±20 Hz/s. Uses CAT frequency, or the selected band without CAT. Keep the entire drifting signal inside the receiver passband.', 'Attivo solo da 144 MHz in su, fino a ±20 Hz/s. Usa la frequenza CAT oppure la banda selezionata senza CAT. Mantieni tutto il segnale in deriva dentro la banda passante del ricevitore.', 'Actif uniquement à partir de 144 MHz, jusqu’à ±20 Hz/s. Utilise la fréquence CAT ou la bande sélectionnée sans CAT. Gardez tout le signal dérivant dans la bande passante du récepteur.', 'Nur ab 144 MHz aktiv, bis ±20 Hz/s. Verwendet die CAT-Frequenz oder ohne CAT das gewählte Band. Das gesamte driftende Signal muss im Durchlassbereich des Empfängers bleiben.', 'Aktiv kun fra 144 MHz, opptil ±20 Hz/s. Bruker CAT-frekvensen eller valgt bånd uten CAT. Hold hele signalet innenfor mottakerens passbånd.', 'Aktivní pouze od 144 MHz, až ±20 Hz/s. Používá frekvenci CAT nebo zvolené pásmo bez CAT. Celý driftující signál musí zůstat v propustném pásmu přijímače.'))

PHRASES.extend([('Try GPU drift sync (experimental, automatic CPU fallback)', 'Prova GPU per il drift (sperimentale, ritorno automatico alla CPU)', 'Essayer le GPU pour la dérive (expérimental, repli CPU automatique)', 'GPU für Drift testen (experimentell, automatischer CPU-Rückfall)', 'Prøv GPU for drift (eksperimentelt, automatisk CPU-reserve)', 'Vyzkoušet GPU pro drift (experimentální, automatický návrat na CPU)'), ('Requires an OpenCL GPU with double precision. The first batch checks CPU parity and total time; slower or failed devices keep the CPU path. Applies to drift recovery from 144 MHz.', 'Richiede una GPU OpenCL con doppia precisione. Il primo blocco confronta risultati e tempo totale con la CPU; se la GPU è più lenta o dà errori, resta attiva la CPU. Vale per il recupero drift da 144 MHz.', 'Nécessite un GPU OpenCL en double précision. Le premier lot compare les résultats et le temps total au CPU ; si le GPU est plus lent ou échoue, le CPU reste actif. Pour la dérive à partir de 144 MHz.', 'Erfordert eine OpenCL-GPU mit doppelter Genauigkeit. Der erste Stapel vergleicht Ergebnisse und Gesamtzeit mit der CPU; bei langsamerer GPU oder Fehlern bleibt die CPU aktiv. Für Drift ab 144 MHz.', 'Krever OpenCL-GPU med dobbel presisjon. Første gruppe sammenligner resultater og totaltid med CPU; ved feil eller tregere GPU brukes CPU. Gjelder drift fra 144 MHz.', 'Vyžaduje OpenCL GPU s dvojitou přesností. První dávka porovná výsledky a celkový čas s CPU; při chybě nebo pomalejší GPU zůstává CPU. Pro drift od 144 MHz.')])

PHRASES.extend([
 ("Try GPU FT8/FT4 decoding (experimental, all bands)", "Prova decodifica FT8/FT4 con GPU (sperimentale, tutte le bande)", "Essayer le décodage FT8/FT4 sur GPU (expérimental, toutes bandes)", "FT8/FT4-Dekodierung mit GPU testen (experimentell, alle Bänder)", "Prøv FT8/FT4-dekoding med GPU (eksperimentelt, alle bånd)", "Vyzkoušet dekódování FT8/FT4 na GPU (experimentální, všechna pásma)"),
 ("Offloads symbol tone energies to OpenCL FP64. Checks CPU parity and total time, with automatic CPU fallback. Independent of drift compensation.", "Calcola le energie dei toni dei simboli con OpenCL FP64. Confronta risultati e tempo totale con la CPU e torna automaticamente alla CPU se necessario. Indipendente dalla compensazione drift.", "Calcule les énergies des tons avec OpenCL FP64. Compare les résultats et le temps total au CPU, avec repli automatique. Indépendant de la compensation de dérive.", "Berechnet Symboltonenergien mit OpenCL FP64. Vergleicht Ergebnisse und Gesamtzeit mit der CPU, mit automatischem Rückfall. Unabhängig von der Driftkompensation.", "Beregner symboltonenes energi med OpenCL FP64. Sammenligner resultater og totaltid med CPU, med automatisk CPU-reserve. Uavhengig av driftkompensasjon.", "Počítá energie tónů symbolů pomocí OpenCL FP64. Porovnává výsledky a celkový čas s CPU, s automatickým návratem. Nezávislé na kompenzaci driftu.")
])
