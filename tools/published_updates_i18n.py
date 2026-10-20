KEYS = {
 'update_current_published': {
  'en':'No newer published release is available.\n\nInstalled: %1\nNewest published: %2',
  'it':'Non è disponibile una release pubblicata più recente.\n\nInstallata: %1\nUltima pubblicata: %2',
  'fr':'Aucune version publiée plus récente n’est disponible.\n\nInstallée : %1\nDernière publiée : %2',
  'de':'Keine neuere veröffentlichte Version verfügbar.\n\nInstalliert: %1\nNeueste Veröffentlichung: %2',
  'no':'Ingen nyere publisert versjon er tilgjengelig.\n\nInstallert: %1\nNyeste publiserte: %2',
  'cs':'Není dostupné novější publikované vydání.\n\nNainstalováno: %1\nNejnovější publikované: %2'},
 'update_prerelease': {'en':'This is a pre-release.', 'it':'Questa è una pre-release.', 'fr':'Il s’agit d’une préversion.', 'de':'Dies ist eine Vorabversion.', 'no':'Dette er en forhåndsversjon.', 'cs':'Jde o předběžné vydání.'},
 'update_history_limit': {
  'en':'The release history is too large to finish checking. Open the releases page.',
  'it':'La cronologia delle release è troppo ampia per completare il controllo. Apri la pagina delle release.',
  'fr':'L’historique est trop volumineux pour terminer la vérification. Ouvrez la page des versions.',
  'de':'Der Veröffentlichungsverlauf ist zu groß für eine vollständige Prüfung. Öffnen Sie die Release-Seite.',
  'no':'Versjonshistorikken er for stor til å fullføre kontrollen. Åpne utgivelsessiden.',
  'cs':'Historie vydání je příliš rozsáhlá pro dokončení kontroly. Otevřete stránku vydání.'}
}

KEYS.update({
 'tab_dx': {l:'DX' for l in ('en','it','fr','de','no','cs')},
 'update_missing_checksums': dict(zip(('en','it','fr','de','no','cs'), (
 'The release has no SHA256SUMS.txt file to verify downloads.',
 'Nella release manca il file SHA256SUMS.txt necessario per verificare il download.',
 'Le fichier SHA256SUMS.txt nécessaire pour vérifier le téléchargement est absent de cette version.',
 'Der Release fehlt die Datei SHA256SUMS.txt zur Prüfung des Downloads.',
 'Utgivelsen mangler SHA256SUMS.txt for å kontrollere nedlastingen.',
 'Ve vydání chybí soubor SHA256SUMS.txt pro ověření stažených dat.'))),
 'update_unknown_bundle': dict(zip(('en','it','fr','de','no','cs'), (
 'The Linux package type could not be identified. Download the package for your distribution.',
 'Non riesco a identificare il pacchetto Linux in uso. Scarica il pacchetto per la tua distribuzione.',
 'Le type de paquet Linux utilisé est inconnu. Téléchargez le paquet pour votre distribution.',
 'Das verwendete Linux-Paket wurde nicht erkannt. Laden Sie das Paket für Ihre Distribution herunter.',
 'Linux-pakken kunne ikke identifiseres. Last ned pakken for distribusjonen din.',
 'Použitý balíček Linuxu nebyl rozpoznán. Stáhněte balíček pro svou distribuci.'))),
 'update_missing_compatible_package': dict(zip(('en','it','fr','de','no','cs'), (
 'The release has no compatible package. Expected: %1',
 'Nella release manca un pacchetto compatibile. Nomi cercati: %1',
 'Cette version ne contient aucun paquet compatible. Noms recherchés : %1',
 'Die Veröffentlichung enthält kein passendes Paket. Gesuchte Namen: %1',
 'Utgivelsen har ingen kompatibel pakke. Forventede navn: %1',
 'Vydání neobsahuje kompatibilní balíček. Hledané názvy: %1'))),
 'update_manual_download_reason': dict(zip(('en','it','fr','de','no','cs'), (
 'A newer release is available. %1\n\nOpening the release page.',
 'È disponibile una release più recente. %1\n\nApro la pagina della release.',
 'Une version plus récente est disponible. %1\n\nOuverture de la page de la version.',
 'Eine neuere Version ist verfügbar. %1\n\nDie Release-Seite wird geöffnet.',
 'En nyere utgivelse er tilgjengelig. %1\n\nÅpner utgivelsessiden.',
 'Je dostupné novější vydání. %1\n\nOtevírám stránku vydání.'))),
})
