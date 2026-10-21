LANGS=('en','it','fr','de','no','cs')
VALUES={
 'ft_cq_queue_enable': ('Manage CQ queue','Gestisci coda CQ','Gérer la file CQ','CQ-Warteschlange verwalten','Administrer CQ-kø','Spravovat frontu CQ'),
 'ft_cq_queue_tooltip': (
 'When enabled, double-clicks add stations to the CQ list without interrupting the current QSO. Press Start list to begin.',
 'Quando attivo, i doppi clic aggiungono stazioni alla lista CQ senza interrompere il QSO corrente. Premi Avvia lista per iniziare.',
 'Si activé, les doubles-clics ajoutent les stations à la liste CQ sans interrompre le QSO en cours. Appuyez sur Démarrer la liste.',
 'Wenn aktiv, fügen Doppelklicks Stationen zur CQ-Liste hinzu, ohne das laufende QSO zu unterbrechen. Zum Beginnen Liste starten drücken.',
 'Når aktivert legger dobbeltklikk stasjoner i CQ-listen uten å avbryte aktivt QSO. Trykk Start listen for å begynne.',
 'Při zapnutí dvojklik přidá stanici do seznamu CQ bez přerušení aktuálního QSO. Začněte tlačítkem Spustit seznam.'),
 'tab_qrz': ('QRZ',)*6,
 'ft8_queue_callers': ('Queue stations calling me','Accoda chi mi chiama','Mettre mes appelants en file','Meine Anrufer einreihen','Sett mine anropere i kø','Řadit volající stanice do fronty'),
 'ft_cq_list_title': ('CQ stations to work','CQ da lavorare','Stations CQ à contacter','CQ-Stationen abarbeiten','CQ-stasjoner å kontakte','Stanice CQ ke spojení'),
 'ft_cq_list_add': ('Add selected CQs','Accoda CQ selezionati','Ajouter les CQ sélectionnés','Gewählte CQs hinzufügen','Legg til valgte CQ','Přidat vybraná CQ'),
 'ft_cq_list_button': ('CQ list (%1)','Lista CQ (%1)','Liste CQ (%1)','CQ-Liste (%1)','CQ-liste (%1)','Seznam CQ (%1)'),
 'ft_cq_list_selection_help': (
 'Select several CQ rows with Ctrl/Shift, then add them. Open the CQ list and press Start to work them one at a time.',
 'Seleziona più righe CQ con Ctrl/Maiusc e accodale. Apri Lista CQ e premi Avvia per lavorarle una alla volta.',
 'Sélectionnez plusieurs lignes CQ avec Ctrl/Maj et ajoutez-les. Ouvrez la liste et appuyez sur Démarrer pour les contacter une à une.',
 'Mehrere CQ-Zeilen mit Strg/Umschalt auswählen und hinzufügen. Die CQ-Liste öffnen und mit Start nacheinander abarbeiten.',
 'Velg flere CQ-rader med Ctrl/Shift og legg dem til. Åpne CQ-listen og trykk Start for å kontakte én om gangen.',
 'Vyberte více řádků CQ pomocí Ctrl/Shift a přidejte je. Otevřete seznam CQ a spusťte postupné navazování spojení.'),
 'ft_cq_list_help': (
 'Select CQ rows with Ctrl/Shift and add them to this list. Start enables one QSO at a time. Before each call MM waits for a new CQ; stations working someone else or no longer heard are skipped. Pause takes effect after the current QSO. STOP stops TX and pauses the list.',
 'Seleziona le righe CQ con Ctrl/Maiusc e accodale. Avvia esegue un QSO alla volta. Prima di ogni chiamata MM attende un nuovo CQ: salta le stazioni impegnate con altri o non più ricevute. Pausa lascia terminare il QSO corrente. STOP ferma la TX e mette in pausa la lista.',
 'Sélectionnez les lignes CQ avec Ctrl/Maj et ajoutez-les. Démarrer active un QSO à la fois. Avant chaque appel, MM attend un nouveau CQ ; les stations occupées ou non reçues sont ignorées. Pause laisse finir le QSO en cours. STOP arrête la TX et suspend la liste.',
 'CQ-Zeilen mit Strg/Umschalt auswählen und hinzufügen. Start arbeitet ein QSO nach dem anderen ab. Vor jedem Anruf wartet MM auf ein neues CQ; belegte oder nicht mehr empfangene Stationen werden übersprungen. Pause lässt das laufende QSO enden. STOP beendet TX und pausiert die Liste.',
 'Velg CQ-rader med Ctrl/Shift og legg dem til. Start aktiverer ett QSO om gangen. Før hvert anrop venter MM på et nytt CQ; opptatte stasjoner og stasjoner som ikke lenger høres hoppes over. Pause lar aktivt QSO fullføres. STOP stopper TX og setter listen på pause.',
 'Vyberte řádky CQ pomocí Ctrl/Shift a přidejte je. Spuštění provádí jedno QSO po druhém. Před každým voláním MM čeká na nové CQ; obsazené nebo již neslyšené stanice přeskočí. Pauza nechá aktuální QSO dokončit. STOP zastaví TX a pozastaví seznam.'),
 'ft_cq_list_pause': ('Pause after QSO','Pausa dopo QSO','Pause après le QSO','Pause nach QSO','Pause etter QSO','Pauza po QSO'),
 'ft_cq_list_start': ('Start list','Avvia lista','Démarrer la liste','Liste starten','Start listen','Spustit seznam'),
 'ft_cq_list_remove': ('Remove selected','Rimuovi selezionati','Retirer la sélection','Auswahl entfernen','Fjern valgte','Odebrat vybrané'),
 'ft_cq_list_clear': ('Clear list','Svuota lista','Vider la liste','Liste leeren','Tøm listen','Vyprázdnit seznam'),
 'ft_cq_list_heard': ('CQ heard','CQ ricevuto','CQ reçu','CQ empfangen','CQ mottatt','CQ přijato'),
 'ft_cq_list_busy': ('Working another station','Impegnata con altra stazione','Occupée avec une autre station','Mit anderer Station beschäftigt','Opptatt med en annen stasjon','Spojení s jinou stanicí'),
 'ft_cq_list_wait': ('Waiting for new CQ','Attesa di nuovo CQ','Attente d’un nouveau CQ','Warten auf neues CQ','Venter på nytt CQ','Čekání na nové CQ'),
 'ft_cq_list_priority': ('Operator CQ list','Lista CQ operatore','Liste CQ opérateur','CQ-Liste des Bedieners','Operatørens CQ-liste','Seznam CQ operátora'),
}
for _key in ('ft_cq_list_help', 'ft_cq_list_selection_help'):
    VALUES[_key] = tuple(text + suffix for text,suffix in zip(VALUES[_key],(
      ' Enable Manage CQ queue to add stations by repeated double-clicks. The numbered table shows their order; duplicates are ignored.',
      ' Attiva Gestisci coda CQ per aggiungere stazioni con doppi clic successivi. La tabella numerata mostra l’ordine; i duplicati sono ignorati.',
      ' Activez Gérer la file CQ pour ajouter les stations par doubles-clics successifs. Le tableau numéroté montre leur ordre ; les doublons sont ignorés.',
      ' CQ-Warteschlange verwalten aktivieren, um Stationen durch mehrere Doppelklicks hinzuzufügen. Die nummerierte Tabelle zeigt die Reihenfolge; Duplikate werden ignoriert.',
      ' Aktiver Administrer CQ-kø for å legge til stasjoner med flere dobbeltklikk. Den nummererte tabellen viser rekkefølgen; duplikater ignoreres.',
      ' Zapněte Spravovat frontu CQ pro přidávání stanic opakovanými dvojkliky. Číslovaná tabulka ukazuje pořadí; duplicity se ignorují.')))
KEYS={key:dict(zip(LANGS,value)) for key,value in VALUES.items()}
