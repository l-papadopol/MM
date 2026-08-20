#!/usr/bin/env python3
"""Regenerate the reviewed, operator-facing MadModem help pages.

The navigation is kept from each language project.  Page bodies are complete
translations: never build prose by replacing individual English words.
"""
from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HELP = ROOT / "docs" / "help"
VERSION = (ROOT / "MADMODEM_VERSION.txt").read_text(encoding="utf-8").strip()

LANGUAGE_NAMES = {
    "en": "English",
    "it": "Italiano",
    "fr": "Français",
    "de": "Deutsch",
    "no": "Norsk",
    "cs": "Čeština",
}


PAGES: dict[str, dict[str, tuple[str, str]]] = {
    "en": {
        "waterfall.html": (
            "Waterfall and markers",
            """<h1>Waterfall and markers</h1>
<p>The waterfall shows the audio passband used by the selected mode. Left-click a signal to move the RX marker; in FT4/FT8, right-click moves the TX marker. Optional FT and RTTY labels can be shown over the spectrum.</p>
<h2>Zoom and movement</h2><p>Use the mouse wheel to zoom around the pointer. Shift+wheel, a horizontal wheel or middle-button drag pans the visible range. Left double-click restores the full passband. Markers and overlays remain tied to their real audio frequencies.</p>""",
        ),
        "text_modes.html": (
            "Text modes",
            """<h1>Text modes</h1>
<h2>RTTY</h2><p>The terminal follows the selected Mark/Space pair. The tuning scope is reserved for the live signal trace. <b>Show decoded text on waterfall</b> adds an optional vertical text trail between Mark and Space; turn it off for an unobstructed spectrum.</p>
<p>Contest multi-decode can find activity away from the selected signal and display CQ/callsign labels. The main terminal and QSO controls still belong to the selected signal only.</p>
<h2>PSK, MFSK and Feld Hell</h2><p>Select the mode, place the RX marker on the signal and start RX. Use the mode controls for baud rate, bandwidth or AFC where available; text macros use the same station identity configured in Settings.</p>""",
        ),
        "scheduler.html": (
            "Scheduler",
            """<h1>Scheduler</h1>
<p>The scheduler applies a daily UTC plan of band and dial-frequency changes through CAT. Each entry specifies a time, operating group and frequency; FT4/FT8 entries may use the standard frequency for the selected band.</p>
<h2>Safe QSY</h2><p>A scheduled change waits while TX or an FT QSO is active. It changes only the radio frequency/band and never starts a transmission. If CAT is disabled or disconnected, the entry is reported and the radio is not tuned.</p>
<p>Enable the plan separately for each supported mode group. The Mode tab shows whether the plan is enabled and whether a QSY is pending.</p>""",
        ),
        "troubleshooting.html": (
            "Troubleshooting",
            """<h1>Troubleshooting</h1>
<h2>No receive decodes</h2><p>Confirm that RX is running, the correct audio input is selected and the level is above the noise floor without clipping. Place the mode marker on the signal. FT4/FT8 also require an accurate UTC clock.</p>
<h2>CAT or band change does not work</h2><p>Check the Hamlib model, port or network address and use the CAT read test. Selecting an FT band tunes the standard dial frequency only while CAT is connected.</p>
<h2>TX does not start</h2><p>Configure a valid callsign and locator under Settings → User/QTH. Then verify the TX audio device, PTT method and radio data-mode routing. The Runtime log records the reason for a blocked FT transmission.</p>
<h2>Rotator does not move</h2><p>Use a rotator connection separate from radio CAT, confirm its mechanical limits and check the MOV/RDY indicators. Never bypass a reported limit or emergency stop.</p>""",
        ),
        "rotator.html": (
            "Rotator",
            """<h1>Rotator</h1>
<p>Rotator control is independent from radio CAT. Rotator 1/2/3 each has its own Hamlib connection, mechanical geometry, limits, park position and band assignments.</p>
<h2>Pointing</h2><p>Enter a Maidenhead locator, country/DXCC or prefix to calculate bearing and distance from your QTH. Manual azimuth/elevation, QSO tracking and Moon/EME tracking use the active profile.</p>
<h2>Geometry and calibration</h2><p>Choose the real azimuth scale, including 360° North/South stop, Yaesu 450° overlap or a custom range. Auto-calibration updates the measured motion rate. Automatic peak search stays inside the configured AZ/EL span and pauses during TX.</p>
<h2>Status and safety</h2><p>MOV means moving; RDY means connected and stopped. The navball shows current pointing, the TG target and mechanical overlap. Verify limits, cable wrap and an accessible stop before automatic movement.</p>""",
        ),
        "logbook_map.html": (
            "Logbook and map",
            """<h1>Logbook and map</h1>
<p>The ADIF logbook stores contacts from every QSO-capable mode. Search and filter by callsign, band, mode, locator or UTC interval, then export all results or only the selected records.</p>
<p><strong>UDP QSO logging:</strong> in Settings → Logbook, enable <em>Send logged QSOs to UDP server</em> and set the destination address and port (default 127.0.0.1:2237). After a QSO is successfully appended to the local ADIF log, MadModem sends a WSJT-X/JTDX-compatible Logged ADIF UDP message. UDP failure never cancels the local log entry.</p>
<p>The QSO map can show the online or offline background, Maidenhead grid, station markers and paths from your QTH. Use the single Layers button to choose what is visible. A selected locator can also become the active rotator target.</p>""",
        ),
    },
    "it": {
        "waterfall.html": (
            "Waterfall e marker",
            """<h1>Waterfall e marker</h1>
<p>Il waterfall mostra la banda audio usata dal modo selezionato. Un clic sinistro sul segnale sposta il marker RX; in FT4/FT8 il clic destro sposta il marker TX. Le etichette FT e RTTY sullo spettro sono opzionali.</p>
<h2>Zoom e spostamento</h2><p>La rotella ingrandisce attorno al puntatore. Maiusc+rotella, la rotella orizzontale o il trascinamento col tasto centrale spostano l'intervallo visibile. Un doppio clic sinistro ripristina l'intera banda. Marker e sovraimpressioni restano legati alla frequenza audio reale.</p>""",
        ),
        "text_modes.html": (
            "Modi testuali",
            """<h1>Modi testuali</h1>
<h2>RTTY</h2><p>Il terminale segue la coppia Mark/Space selezionata. L'oscilloscopio di sintonia mostra soltanto la traccia del segnale. <b>Mostra testo decodificato sul waterfall</b> aggiunge, se desiderato, una scia verticale fra Mark e Space; disattivala per lasciare libero lo spettro.</p>
<p>La decodifica multipla da contest individua attività lontano dal segnale selezionato e può mostrare etichette CQ/nominativo. Terminale e controlli QSO restano sempre associati al solo segnale selezionato.</p>
<h2>PSK, MFSK e Feld Hell</h2><p>Seleziona il modo, porta il marker RX sul segnale e avvia RX. Usa i controlli del modo per velocità, larghezza di banda o AFC; le macro impiegano l'identità di stazione configurata nelle Impostazioni.</p>""",
        ),
        "scheduler.html": (
            "Pianificatore",
            """<h1>Pianificatore</h1>
<p>Il pianificatore applica via CAT un programma giornaliero UTC di cambi banda e frequenza. Ogni riga indica ora, gruppo operativo e frequenza; per FT4/FT8 si può usare la frequenza standard della banda scelta.</p>
<h2>Cambio frequenza sicuro</h2><p>Un cambio programmato attende la fine del TX o del QSO FT in corso. Modifica soltanto frequenza e banda della radio e non avvia mai una trasmissione. Se la CAT è disattivata o scollegata, l'evento viene segnalato senza sintonizzare la radio.</p>
<p>Il programma si abilita separatamente per ciascun gruppo di modi. La scheda Modo mostra se è attivo e se esiste un cambio in attesa.</p>""",
        ),
        "troubleshooting.html": (
            "Risoluzione dei problemi",
            """<h1>Risoluzione dei problemi</h1>
<h2>Nessuna decodifica in ricezione</h2><p>Verifica che RX sia attivo, che l'ingresso audio sia quello corretto e che il livello superi il rumore senza saturare. Porta il marker del modo sul segnale. FT4/FT8 richiedono anche un orologio UTC preciso.</p>
<h2>CAT o cambio banda non funzionano</h2><p>Controlla modello Hamlib, porta o indirizzo di rete ed esegui il test di lettura CAT. La scelta della banda FT sintonizza la frequenza standard soltanto quando la CAT è connessa.</p>
<h2>Il TX non parte</h2><p>Configura nominativo e locator validi in Impostazioni → Utente/QTH. Controlla poi uscita audio TX, metodo PTT e instradamento dati della radio. Il Log runtime indica il motivo di un TX FT bloccato.</p>
<h2>Il rotore non si muove</h2><p>Usa una connessione distinta dalla CAT radio, verifica i limiti meccanici e controlla le spie MOV/RDY. Non aggirare mai un limite o un arresto di emergenza segnalato.</p>""",
        ),
        "rotator.html": (
            "Rotore",
            """<h1>Rotore</h1>
<p>Il controllo del rotore è indipendente dalla CAT radio. Rotore 1/2/3 hanno ciascuno connessione Hamlib, geometria meccanica, limiti, posizione di parcheggio e bande assegnate propri.</p>
<h2>Puntamento</h2><p>Inserisci locator Maidenhead, paese/DXCC o prefisso per calcolare direzione e distanza dal tuo QTH. Puntamento manuale azimut/elevazione, inseguimento QSO e inseguimento Luna/EME usano il profilo attivo.</p>
<h2>Geometria e calibrazione</h2><p>Scegli la scala azimutale reale: 360° con fermo a Nord o Sud, sovrapposizione Yaesu a 450° oppure intervallo personalizzato. L'autocalibrazione aggiorna la velocità misurata. La ricerca automatica del massimo resta entro l'apertura AZ/EL impostata e si sospende durante il TX.</p>
<h2>Stato e sicurezza</h2><p>MOV indica movimento; RDY indica rotore connesso e fermo. La sfera di navigazione mostra direzione attuale, bersaglio TG e sovrapposizione meccanica. Prima dei movimenti automatici verifica limiti, avvolgimento dei cavi e arresto accessibile.</p>""",
        ),
        "logbook_map.html": (
            "Registro QSO e mappa",
            """<h1>Registro QSO e mappa</h1>
<p>Il registro ADIF raccoglie i collegamenti di tutti i modi che gestiscono QSO. Puoi cercare e filtrare per nominativo, banda, modo, locator o intervallo UTC, quindi esportare tutti i risultati o solo le righe selezionate.</p>
<p><strong>Invio QSO via UDP:</strong> in Impostazioni → Registro QSO abilita <em>Invia i QSO registrati al server UDP</em> e imposta indirizzo e porta di destinazione (predefiniti 127.0.0.1:2237). Dopo che un QSO è stato aggiunto con successo al registro ADIF locale, MadModem invia un messaggio UDP Logged ADIF compatibile con WSJT-X/JTDX. Un errore UDP non annulla mai il QSO salvato localmente.</p>
<p>La mappa QSO può mostrare sfondo online o locale, griglia Maidenhead, stazioni e percorsi dal tuo QTH. Un unico pulsante Livelli sceglie gli elementi visibili. Il locator selezionato può diventare anche il bersaglio del rotore.</p>""",
        ),
    },
    "fr": {
        "waterfall.html": (
            "Waterfall et marqueurs",
            """<h1>Waterfall et marqueurs</h1>
<p>Le waterfall affiche la bande audio utilisée par le mode choisi. Un clic gauche sur un signal déplace le marqueur RX ; en FT4/FT8, un clic droit déplace le marqueur TX. Les étiquettes FT et RTTY sur le spectre sont facultatives.</p>
<h2>Zoom et déplacement</h2><p>La molette zoome autour du pointeur. Maj+molette, une molette horizontale ou le glisser avec le bouton central déplacent la plage visible. Un double clic gauche rétablit toute la bande. Marqueurs et surimpressions restent liés à leur fréquence audio réelle.</p>""",
        ),
        "text_modes.html": (
            "Modes texte",
            """<h1>Modes texte</h1>
<h2>RTTY</h2><p>Le terminal suit la paire Mark/Space sélectionnée. L'oscilloscope d'accord est réservé à la trace du signal. <b>Afficher le texte décodé sur la waterfall</b> ajoute, au choix, une traînée verticale entre Mark et Space ; désactivez-la pour libérer le spectre.</p>
<p>Le multidecodage de concours repère l'activité hors du signal choisi et peut afficher les CQ/indicatifs. Le terminal principal et les commandes QSO restent liés au seul signal sélectionné.</p>
<h2>PSK, MFSK et Feld Hell</h2><p>Choisissez le mode, placez le marqueur RX sur le signal et lancez RX. Réglez vitesse, largeur de bande ou AFC dans les commandes du mode ; les macros utilisent l'identité de station enregistrée dans les réglages.</p>""",
        ),
        "scheduler.html": (
            "Planificateur",
            """<h1>Planificateur</h1>
<p>Le planificateur applique par CAT un programme UTC quotidien de changements de bande et de fréquence. Chaque ligne indique l'heure, le groupe de modes et la fréquence ; une entrée FT4/FT8 peut utiliser la fréquence standard de la bande choisie.</p>
<h2>Changement sûr</h2><p>Un changement programmé attend la fin d'une TX ou d'un QSO FT actif. Il ne modifie que la fréquence/bande de la radio et ne lance jamais d'émission. Si la CAT est désactivée ou déconnectée, l'événement est signalé sans accorder la radio.</p>
<p>Activez le programme séparément pour chaque groupe de modes. L'onglet Mode indique son état et tout changement en attente.</p>""",
        ),
        "troubleshooting.html": (
            "Dépannage",
            """<h1>Dépannage</h1>
<h2>Aucun décodage en réception</h2><p>Vérifiez que RX est actif, que la bonne entrée audio est choisie et que le niveau dépasse le bruit sans saturer. Placez le marqueur du mode sur le signal. FT4/FT8 exigent aussi une horloge UTC précise.</p>
<h2>CAT ou changement de bande inactif</h2><p>Contrôlez le modèle Hamlib, le port ou l'adresse réseau, puis lancez le test de lecture CAT. Le choix d'une bande FT accorde la fréquence standard uniquement lorsque la CAT est connectée.</p>
<h2>La TX ne démarre pas</h2><p>Renseignez un indicatif et un locator valides dans Réglages → Utilisateur/QTH. Vérifiez ensuite la sortie audio TX, la méthode PTT et le routage du mode données de la radio. Le journal d'exécution indique pourquoi une TX FT est bloquée.</p>
<h2>Le rotateur ne bouge pas</h2><p>Utilisez une connexion distincte de la CAT radio, contrôlez les limites mécaniques et les voyants MOV/RDY. Ne contournez jamais une limite ou un arrêt d'urgence signalé.</p>""",
        ),
        "rotator.html": (
            "Rotateur",
            """<h1>Rotateur</h1>
<p>La commande du rotateur est indépendante de la CAT radio. Rotateur 1/2/3 possèdent chacun leur connexion Hamlib, géométrie mécanique, limites, position de parc et affectations de bandes.</p>
<h2>Pointage</h2><p>Saisissez un locator Maidenhead, un pays/DXCC ou un préfixe pour calculer l'azimut et la distance depuis votre QTH. Le pointage manuel azimut/élévation, le suivi QSO et le suivi Lune/EME utilisent le profil actif.</p>
<h2>Géométrie et étalonnage</h2><p>Choisissez l'échelle réelle : butée Nord ou Sud à 360°, recouvrement Yaesu à 450° ou plage personnalisée. L'étalonnage automatique actualise la vitesse mesurée. La recherche automatique du maximum reste dans la plage AZ/EL fixée et s'interrompt pendant la TX.</p>
<h2>État et sécurité</h2><p>MOV indique un mouvement ; RDY indique une connexion à l'arrêt. La sphère de navigation montre le pointage, la cible TG et le recouvrement mécanique. Vérifiez limites, torsion des câbles et arrêt accessible avant tout mouvement automatique.</p>""",
        ),
        "logbook_map.html": (
            "Journal et carte",
            """<h1>Journal et carte</h1>
<p>Le journal ADIF regroupe les contacts de tous les modes avec QSO. Recherchez et filtrez par indicatif, bande, mode, locator ou intervalle UTC, puis exportez tous les résultats ou seulement les lignes choisies.</p>
<p><strong>Envoi des QSO par UDP :</strong> dans Réglages → Journal, activez l’envoi vers le serveur UDP et indiquez l’adresse et le port de destination (127.0.0.1:2237 par défaut). Après l’ajout réussi du QSO au journal ADIF local, MadModem envoie un message UDP ADIF journalisé compatible WSJT-X/JTDX. Un échec UDP n’annule jamais l’entrée locale.</p>
<p>La carte QSO peut afficher un fond en ligne ou local, la grille Maidenhead, les stations et les trajets depuis votre QTH. Un seul bouton Calques choisit les éléments visibles. Le locator sélectionné peut aussi devenir la cible du rotateur.</p>""",
        ),
    },
    "de": {
        "waterfall.html": (
            "Wasserfall und Marker",
            """<h1>Wasserfall und Marker</h1>
<p>Der Wasserfall zeigt den vom gewählten Modus verwendeten Audiobereich. Ein Linksklick auf ein Signal setzt den RX-Marker; bei FT4/FT8 setzt ein Rechtsklick den TX-Marker. FT- und RTTY-Hinweise im Spektrum sind optional.</p>
<h2>Zoom und Verschieben</h2><p>Das Mausrad zoomt um den Zeiger. Umschalt+Rad, ein horizontales Rad oder Ziehen mit der mittleren Taste verschieben den Ausschnitt. Ein linker Doppelklick stellt den gesamten Bereich wieder her. Marker und Einblendungen bleiben an ihre tatsächliche Audiofrequenz gebunden.</p>""",
        ),
        "text_modes.html": (
            "Textmodi",
            """<h1>Textmodi</h1>
<h2>RTTY</h2><p>Das Terminal folgt dem gewählten Mark/Space-Paar. Das Abstimmdisplay zeigt nur die Signalspur. <b>Dekodierten Text im Wasserfall anzeigen</b> fügt wahlweise eine senkrechte Textspur zwischen Mark und Space ein; für ein freies Spektrum kann sie abgeschaltet werden.</p>
<p>Die Contest-Mehrfachdekodierung findet Aktivität außerhalb des gewählten Signals und kann CQ-/Rufzeichenhinweise anzeigen. Hauptterminal und QSO-Steuerung gehören weiterhin nur zum ausgewählten Signal.</p>
<h2>PSK, MFSK und Feld Hell</h2><p>Modus wählen, RX-Marker auf das Signal setzen und RX starten. Geschwindigkeit, Bandbreite oder AFC werden in den Modusreglern eingestellt; Makros verwenden die in den Einstellungen hinterlegte Stationsidentität.</p>""",
        ),
        "scheduler.html": (
            "Planer",
            """<h1>Planer</h1>
<p>Der Planer setzt über CAT einen täglichen UTC-Plan für Band- und Frequenzwechsel um. Jede Zeile enthält Uhrzeit, Modusgruppe und Frequenz; FT4/FT8-Einträge können die Standardfrequenz des gewählten Bandes verwenden.</p>
<h2>Sicherer Frequenzwechsel</h2><p>Ein geplanter Wechsel wartet, solange TX oder ein FT-QSO aktiv ist. Er ändert nur Funkfrequenz und Band und startet niemals eine Sendung. Ist CAT aus oder getrennt, wird das Ereignis gemeldet, ohne das Funkgerät abzustimmen.</p>
<p>Der Plan wird für jede unterstützte Modusgruppe einzeln aktiviert. Der Reiter Modus zeigt Aktivierung und anstehende Wechsel.</p>""",
        ),
        "troubleshooting.html": (
            "Fehlersuche",
            """<h1>Fehlersuche</h1>
<h2>Keine Empfangsdekodierung</h2><p>Prüfen Sie, ob RX läuft, der richtige Audioeingang gewählt ist und der Pegel über dem Rauschen liegt, ohne zu übersteuern. Setzen Sie den Modusmarker auf das Signal. FT4/FT8 benötigen zusätzlich eine genaue UTC-Uhr.</p>
<h2>CAT- oder Bandwechsel funktioniert nicht</h2><p>Prüfen Sie Hamlib-Modell, Port oder Netzwerkadresse mit dem CAT-Lesetest. Die FT-Bandwahl stimmt nur bei bestehender CAT-Verbindung auf die Standardfrequenz ab.</p>
<h2>TX startet nicht</h2><p>Tragen Sie unter Einstellungen → Benutzer/QTH ein gültiges Rufzeichen und einen Locator ein. Prüfen Sie danach TX-Audioausgang, PTT-Verfahren und Datenmodus-Routing des Funkgeräts. Das Laufzeitprotokoll nennt den Grund für eine blockierte FT-Sendung.</p>
<h2>Rotor bewegt sich nicht</h2><p>Verwenden Sie eine von der Funkgeräte-CAT getrennte Verbindung, prüfen Sie die mechanischen Grenzen und die Anzeigen MOV/RDY. Gemeldete Grenzen oder Not-Aus niemals umgehen.</p>""",
        ),
        "rotator.html": (
            "Rotor",
            """<h1>Rotor</h1>
<p>Die Rotorsteuerung ist von der Funkgeräte-CAT unabhängig. Rotor 1/2/3 besitzen jeweils eine eigene Hamlib-Verbindung, Mechanik, Grenzwerte, Parkposition und Bandzuordnung.</p>
<h2>Ausrichten</h2><p>Maidenhead-Locator, Land/DXCC oder Präfix eingeben, um Peilung und Entfernung vom eigenen QTH zu berechnen. Manuelle Azimut-/Elevationsvorgabe, QSO-Nachführung und Mond-/EME-Nachführung verwenden das aktive Profil.</p>
<h2>Geometrie und Kalibrierung</h2><p>Wählen Sie die reale Skala: 360° Nord- oder Südanschlag, Yaesu-Überlappung mit 450° oder eigener Bereich. Die automatische Kalibrierung aktualisiert die gemessene Bewegungsgeschwindigkeit. Die automatische Maximumsuche bleibt im eingestellten AZ/EL-Bereich und pausiert bei TX.</p>
<h2>Status und Sicherheit</h2><p>MOV bedeutet Bewegung; RDY bedeutet verbunden und angehalten. Die Navigationskugel zeigt aktuelle Richtung, TG-Ziel und mechanische Überlappung. Vor automatischer Bewegung Grenzen, Kabelreserve und erreichbaren Stopp prüfen.</p>""",
        ),
        "logbook_map.html": (
            "Logbuch und Karte",
            """<h1>Logbuch und Karte</h1>
<p>Das ADIF-Logbuch sammelt Kontakte aller QSO-fähigen Modi. Nach Rufzeichen, Band, Modus, Locator oder UTC-Zeitraum suchen und filtern; anschließend alle Ergebnisse oder nur ausgewählte Datensätze exportieren.</p>
<p><strong>QSO-Übertragung per UDP:</strong> unter Einstellungen → Logbuch das Senden an den UDP-Server aktivieren und Zieladresse sowie Port einstellen (Standard 127.0.0.1:2237). Nach dem erfolgreichen Eintrag eines QSO in das lokale ADIF-Logbuch sendet MadModem eine WSJT-X/JTDX-kompatible Logged-ADIF-UDP-Nachricht. Ein UDP-Fehler verwirft den lokalen Logbucheintrag nicht.</p>
<p>Die QSO-Karte zeigt wahlweise Online- oder Offline-Hintergrund, Maidenhead-Gitter, Stationen und Wege vom eigenen QTH. Eine einzige Schaltfläche Ebenen wählt die sichtbaren Elemente. Ein gewählter Locator kann auch Rotorziel werden.</p>""",
        ),
    },
    "no": {
        "waterfall.html": (
            "Vannfall og markører",
            """<h1>Vannfall og markører</h1>
<p>Vannfallet viser lydbåndet som brukes av valgt modus. Venstreklikk et signal for å flytte RX-markøren; i FT4/FT8 flytter høyreklikk TX-markøren. FT- og RTTY-etiketter over spekteret er valgfrie.</p>
<h2>Zoom og flytting</h2><p>Musehjulet zoomer rundt pekeren. Shift+hjulet, et horisontalt hjul eller dra med midtknappen flytter utsnittet. Venstre dobbeltklikk gjenoppretter hele båndet. Markører og overlegg forblir knyttet til den virkelige lydfrekvensen.</p>""",
        ),
        "text_modes.html": (
            "Tekstmoduser",
            """<h1>Tekstmoduser</h1>
<h2>RTTY</h2><p>Terminalen følger valgt Mark/Space-par. Innstillingsskopet viser bare signalsporet. <b>Vis dekodet tekst på vannfallet</b> legger valgfritt en loddrett tekststripe mellom Mark og Space; slå den av for et fritt spektrum.</p>
<p>Contest-multidekoding kan finne aktivitet utenfor valgt signal og vise CQ-/kallesignaletiketter. Hovedterminalen og QSO-kontrollene tilhører fortsatt bare det valgte signalet.</p>
<h2>PSK, MFSK og Feld Hell</h2><p>Velg modus, sett RX-markøren på signalet og start RX. Hastighet, båndbredde og AFC stilles i moduskontrollene; makroer bruker stasjonsidentiteten fra Innstillinger.</p>""",
        ),
        "scheduler.html": (
            "Planlegger",
            """<h1>Planlegger</h1>
<p>Planleggeren bruker CAT til å gjennomføre en daglig UTC-plan for bånd- og frekvensendringer. Hver rad angir tid, modusgruppe og frekvens; FT4/FT8 kan bruke standardfrekvensen for valgt bånd.</p>
<h2>Trygt frekvensskifte</h2><p>En planlagt endring venter mens TX eller et FT-QSO er aktivt. Den endrer bare radioens frekvens/bånd og starter aldri sending. Hvis CAT er avslått eller frakoblet, meldes hendelsen uten at radioen stilles om.</p>
<p>Planen aktiveres separat for hver støttet modusgruppe. Modusfanen viser om planen er aktiv og om en endring venter.</p>""",
        ),
        "troubleshooting.html": (
            "Feilsøking",
            """<h1>Feilsøking</h1>
<h2>Ingen dekoding i mottak</h2><p>Kontroller at RX kjører, riktig lydinngang er valgt og nivået ligger over støygulvet uten klipping. Sett modusmarkøren på signalet. FT4/FT8 krever også nøyaktig UTC-klokke.</p>
<h2>CAT eller båndskifte virker ikke</h2><p>Kontroller Hamlib-modell, port eller nettverksadresse med CAT-lesetesten. Valg av FT-bånd stiller bare standardfrekvensen når CAT er tilkoblet.</p>
<h2>TX starter ikke</h2><p>Angi gyldig kallesignal og lokator under Innstillinger → Bruker/QTH. Kontroller deretter TX-lydutgang, PTT-metode og radioens datamodusruting. Kjøretidsloggen viser hvorfor en FT-sending ble blokkert.</p>
<h2>Rotoren beveger seg ikke</h2><p>Bruk en forbindelse som er adskilt fra radio-CAT, kontroller mekaniske grenser og MOV/RDY-lampene. Omgå aldri en meldt grense eller nødstopp.</p>""",
        ),
        "rotator.html": (
            "Rotor",
            """<h1>Rotor</h1>
<p>Rotorstyring er uavhengig av radio-CAT. Rotor 1/2/3 har hver sin Hamlib-forbindelse, mekaniske geometri, grenser, parkposisjon og båndtilordning.</p>
<h2>Peiling</h2><p>Skriv inn Maidenhead-lokator, land/DXCC eller prefiks for å beregne retning og avstand fra eget QTH. Manuell asimut/elevasjon, QSO-sporing og Måne-/EME-sporing bruker aktiv profil.</p>
<h2>Geometri og kalibrering</h2><p>Velg virkelig skala: 360° stopp ved nord eller sør, Yaesu 450° overlapp eller egendefinert område. Automatisk kalibrering oppdaterer målt bevegelseshastighet. Automatisk makssøk holder seg innen valgt AZ/EL-område og pauser under TX.</p>
<h2>Status og sikkerhet</h2><p>MOV betyr bevegelse; RDY betyr tilkoblet og stanset. Navigasjonskulen viser gjeldende retning, TG-mål og mekanisk overlapp. Kontroller grenser, kabelvridning og tilgjengelig stopp før automatisk bevegelse.</p>""",
        ),
        "logbook_map.html": (
            "Loggbok og kart",
            """<h1>Loggbok og kart</h1>
<p>ADIF-loggboken samler kontakter fra alle QSO-moduser. Søk og filtrer etter kallesignal, bånd, modus, lokator eller UTC-intervall, og eksporter alle treff eller bare valgte poster.</p>
<p><strong>QSO-logging via UDP:</strong> under Innstillinger → Loggbok aktiver sending til UDP-server og angi måladresse og port (standard 127.0.0.1:2237). Etter at et QSO er lagt til i den lokale ADIF-loggen, sender MadModem en WSJT-X/JTDX-kompatibel Logged ADIF UDP-melding. En UDP-feil fjerner aldri den lokale loggposten.</p>
<p>QSO-kartet kan vise nett- eller lokal bakgrunn, Maidenhead-rutenett, stasjoner og linjer fra eget QTH. Én Lag-knapp velger synlige elementer. En valgt lokator kan også brukes som rotormål.</p>""",
        ),
    },
    "cs": {
        "waterfall.html": (
            "Vodopád a značky",
            """<h1>Vodopád a značky</h1>
<p>Vodopád zobrazuje zvukové pásmo používané zvoleným režimem. Levým kliknutím na signál přesunete značku RX; v FT4/FT8 pravým kliknutím značku TX. Popisky FT a RTTY nad spektrem jsou volitelné.</p>
<h2>Přiblížení a posun</h2><p>Kolečko myši přibližuje kolem ukazatele. Shift+kolečko, vodorovné kolečko nebo tažení prostředním tlačítkem posouvají výřez. Levý dvojklik obnoví celé pásmo. Značky i překryvy zůstávají svázány se skutečnou zvukovou frekvencí.</p>""",
        ),
        "text_modes.html": (
            "Textové režimy",
            """<h1>Textové režimy</h1>
<h2>RTTY</h2><p>Terminál sleduje zvolenou dvojici Mark/Space. Ladicí osciloskop zobrazuje pouze stopu signálu. <b>Zobrazit dekódovaný text na vodopádu</b> volitelně přidá svislou textovou stopu mezi Mark a Space; pro čisté spektrum ji vypněte.</p>
<p>Soutěžní vícenásobné dekódování vyhledá aktivitu mimo zvolený signál a může zobrazit CQ/volací značky. Hlavní terminál a ovládání QSO vždy patří pouze vybranému signálu.</p>
<h2>PSK, MFSK a Feld Hell</h2><p>Zvolte režim, umístěte značku RX na signál a spusťte RX. Rychlost, šířku pásma nebo AFC nastavte v ovládání režimu; makra používají identitu stanice z Nastavení.</p>""",
        ),
        "scheduler.html": (
            "Plánovač",
            """<h1>Plánovač</h1>
<p>Plánovač provádí přes CAT denní UTC program změn pásma a frekvence. Každý řádek určuje čas, skupinu režimů a frekvenci; FT4/FT8 mohou použít standardní frekvenci zvoleného pásma.</p>
<h2>Bezpečné přeladění</h2><p>Naplánovaná změna počká, dokud probíhá TX nebo FT QSO. Mění jen frekvenci/pásmo rádia a nikdy nespouští vysílání. Je-li CAT vypnutý nebo odpojený, událost se oznámí bez přeladění rádia.</p>
<p>Program se zapíná samostatně pro každou podporovanou skupinu režimů. Karta Režim ukazuje stav i čekající změnu.</p>""",
        ),
        "troubleshooting.html": (
            "Řešení potíží",
            """<h1>Řešení potíží</h1>
<h2>V příjmu se nic nedekóduje</h2><p>Ověřte, že RX běží, je zvolen správný zvukový vstup a úroveň je nad šumem bez přebuzení. Umístěte značku režimu na signál. FT4/FT8 navíc vyžadují přesné hodiny UTC.</p>
<h2>Nefunguje CAT nebo změna pásma</h2><p>Testem čtení CAT zkontrolujte model Hamlib, port nebo síťovou adresu. Volba pásma FT naladí standardní frekvenci jen při připojeném CAT.</p>
<h2>TX se nespustí</h2><p>V Nastavení → Uživatel/QTH zadejte platnou značku a lokátor. Potom ověřte zvukový výstup TX, metodu PTT a datové směrování rádia. Běhový protokol uvádí důvod blokování FT vysílání.</p>
<h2>Rotátor se nepohybuje</h2><p>Použijte připojení oddělené od CAT rádia, ověřte mechanické limity a kontrolky MOV/RDY. Nikdy neobcházejte hlášený limit ani nouzové zastavení.</p>""",
        ),
        "rotator.html": (
            "Rotátor",
            """<h1>Rotátor</h1>
<p>Ovládání rotátoru je nezávislé na CAT rádia. Rotátor 1/2/3 mají vlastní připojení Hamlib, mechanickou geometrii, limity, parkovací polohu a přiřazená pásma.</p>
<h2>Směrování</h2><p>Zadejte lokátor Maidenhead, zemi/DXCC nebo prefix pro výpočet azimutu a vzdálenosti z vašeho QTH. Ruční azimut/elevace, sledování QSO a Měsíce/EME používají aktivní profil.</p>
<h2>Geometrie a kalibrace</h2><p>Zvolte skutečnou stupnici: 360° doraz na severu nebo jihu, překryv Yaesu 450° nebo vlastní rozsah. Automatická kalibrace aktualizuje naměřenou rychlost. Automatické hledání maxima zůstává v nastaveném rozsahu AZ/EL a během TX se pozastaví.</p>
<h2>Stav a bezpečnost</h2><p>MOV znamená pohyb; RDY znamená připojeno a zastaveno. Navigační koule ukazuje aktuální směr, cíl TG a mechanický překryv. Před automatickým pohybem ověřte limity, kroucení kabelů a dostupné zastavení.</p>""",
        ),
        "logbook_map.html": (
            "Deník a mapa",
            """<h1>Deník a mapa</h1>
<p>Deník ADIF shromažďuje spojení ze všech režimů s QSO. Vyhledávejte a filtrujte podle značky, pásma, režimu, lokátoru nebo intervalu UTC a exportujte všechny výsledky či pouze vybrané záznamy.</p>
<p><strong>Odesílání QSO přes UDP:</strong> v Nastavení → Deník povolte odesílání na UDP server a nastavte cílovou adresu a port (výchozí 127.0.0.1:2237). Po úspěšném přidání QSO do místního deníku ADIF odešle MadModem UDP zprávu Logged ADIF kompatibilní s WSJT-X/JTDX. Chyba UDP nikdy nezruší místně uložený záznam.</p>
<p>Mapa QSO může zobrazit online nebo místní podklad, mřížku Maidenhead, stanice a trasy z vašeho QTH. Jediné tlačítko Vrstvy volí viditelné prvky. Vybraný lokátor lze použít i jako cíl rotátoru.</p>""",
        ),
    },
}


def main() -> int:
    for lang, pages in PAGES.items():
        for filename, (title, body) in pages.items():
            path = HELP / lang / filename
            old = path.read_text(encoding="utf-8")
            nav_match = re.search(r"<div class='nav'>.*?</div>", old, re.DOTALL)
            if nav_match is None:
                raise RuntimeError(f"missing navigation in {path}")
            nav = nav_match.group(0)
            page = (
                f"<!DOCTYPE html><html lang='{lang}'><head><meta charset='utf-8'>"
                f"<title>{title}</title><link rel='stylesheet' href='../style.css'></head><body>\n"
                f"{nav}\n{body}\n"
                f"<div class='footer'>MadModem {VERSION} — Qt Help — {LANGUAGE_NAMES[lang]}</div>\n"
                "</body></html>\n"
            )
            path.write_text(page, encoding="utf-8", newline="\n")
    print(f"Updated {sum(len(pages) for pages in PAGES.values())} reviewed help pages.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
