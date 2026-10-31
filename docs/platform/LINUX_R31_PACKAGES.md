# R31 LINUXPKG1 — Linux Mint, pacchetti e repository

## Aggiornamento su Mint

Gli asset pubblicati della build 130 includono gli installer `.run` per
Ubuntu 24.04 e Fedora 43 e i relativi portable. Non includono un binario distinto
chiamato Linux Mint. La base Ubuntu di Mint 22, 22.1, 22.2 e 22.3 è Noble/24.04:
il download da scegliere è quindi quello Ubuntu24.04, su processori x86_64.

LINUXPKG1 conserva prima di tutto l'identità del pacchetto installato, letta da
`bin/distribution-platform.txt`, oppure dal precedente `BUILD_INFO.txt`.
Se queste informazioni mancano, riconosce anche Mint 22.x senza
`UBUNTU_CODENAME` e le derivate che dichiarano la base Ubuntu Noble.
Il nome proprio della distribuzione e il suo codename Mint non devono diventare
il nome del download. Sono ammessi i vecchi nomi degli installer della stessa
build come seconda scelta; il nome attuale ha precedenza.

L'aggiornamento di un'installazione gestita continua a utilizzare `.run`.
Un portable continua a utilizzare TAR.GZ/ZIP. Il programma non passa una DEB o
una RPM a Bash e non sceglie un'altra famiglia Linux per trovare un asset.
Verifica SHA-256, consenso e attesa dello stato inattivo restano invariati.

Non abbiamo il numero di versione Mint né il `--build-info` dell'eseguibile
che ha segnalato l'errore: non è dimostrato quale ramo del vecchio updater abbia
fallito. Questa correzione copre la selezione e il fallback descritti sopra.
Mint 21.x usa Ubuntu Jammy; LMDE usa Debian. Non vengono dichiarati compatibili
con Ubuntu24.04 solo perché utilizzano APT. Una versione vecchia dell'updater
deve ricevere questa correzione con un'installazione manuale iniziale.

## Download prodotti da GitHub

| Base e architettura | Installer | Pacchetto nativo | Portable |
| --- | --- | --- | --- |
| Ubuntu 24.04 / Mint 22.x, x86_64 | `.run` | `.deb` | `.tar.gz`, `.zip` |
| Fedora 43, x86_64 | `.run` | `.rpm` | `.tar.gz`, `.zip` |

Per la versione 0.6.4 i nuovi asset pubblici sono:

- `MadModem-0.6.4-Linux-Ubuntu24.04-64bit-Package.deb`
- `MadModem-0.6.4-Linux-Fedora43-64bit-Package.rpm`

Lo script costruiva già questi pacchetti per inserirli negli installer `.run`.
Ora Actions conserva anche i file nativi e il publisher li richiede insieme a
tutti i download esistenti prima di pubblicare la release. Anche i due nuovi
file entrano in `SHA256SUMS.txt`. Il nome pubblico resta stabile; la versione
interna DEB/RPM include il numero della build per gli aggiornamenti 0.6.4→0.6.4.
I pacchetti nativi usano le librerie di sistema, con dipendenze ELF rilevate dai
tool DEB/RPM e dipendenze aggiuntive per i moduli Qt caricati dinamicamente.

Dopo aver copiato **tutti** i sorgenti, inclusa `.github`, nel repository ed
eseguito un nuovo workflow **Build installers**, questi file compariranno nella
nuova Release. Rieseguire una vecchia build usa i suoi vecchi sorgenti.
Questa consegna non pubblica modifiche sul repository remoto.

Installazione manuale dalla cartella del download, sulla base corrispondente:

```bash
# Ubuntu 24.04 / Mint 22.x
sudo apt install ./MadModem-0.6.4-Linux-Ubuntu24.04-64bit-Package.deb
# Fedora 43
sudo dnf install ./MadModem-0.6.4-Linux-Fedora43-64bit-Package.rpm
```

Chiudere MM prima dell'installazione. `.run` e pacchetto nativo installano lo
stesso pacchetto `madmodem`; impostazioni e logbook rimangono nelle directory
utente documentate in [INSTALLERS.md](INSTALLERS.md).

## Arrivare nei repository delle distribuzioni

Pubblicare una DEB/RPM su GitHub permette l'installazione diretta; non aggiunge
automaticamente MM ai cataloghi ufficiali e non crea un repository APT/DNF.
Il percorso consigliato procede su due livelli:

1. **Distribuzione rapida:** PPA Launchpad per Ubuntu e la corrispondente base
   Mint; Copr per Fedora. Richiedono account del manutentore e ricette che
   ricompilino i sorgenti per ciascuna distribuzione. Offrono repository per gli
   aggiornamenti tramite il sistema, previa aggiunta da parte dell'utente.
   Un repository APT firmato separato può servire Debian, con build Debian
   dedicate. Non utilizzare la DEB Ubuntu come pacchetto Debian universale.
2. **Inclusione ufficiale:** pacchetto sorgente Debian con directory `debian/`,
   dipendenze di build, copyright delle singole componenti, regole di build e
   controlli Lintian/sbuild; ITP, pubblicazione su mentors e sponsor per l'upload.
   Il Debian Hamradio Maintainers Team è un interlocutore pertinente. Per Fedora
   servono una SPEC che compili dal sorgente, un SRPM, verifica con mock/rpmlint,
   package review e il percorso di sponsorship/maintainership previsto dal
   progetto. Occorre poi mantenere aggiornamenti e bug nelle release supportate.

Il generatore attuale produce pacchetti binari dalla build CMake esistente:
**non costituisce ancora il pacchetto sorgente Debian o SRPM da presentare per
la revisione ufficiale**. Il prossimo lavoro concreto è preparare quelle due
ricette, verificare attribuzioni e licenze delle componenti, usare le librerie
di distribuzione ove richiesto e aggiungere i metadati desktop/AppStream.
Non sono stati creati account, repository pubblici né inviate candidature.
L'accettazione e la propagazione tra Debian, Ubuntu e Mint seguono i rispettivi
processi e calendari; non sono immediate né garantite.

Riferimenti ufficiali consultati il 31 ottobre 2026:

- [Basi delle versioni Linux Mint](https://www.linuxmint.com/download_all.php)
- [Debian Policy: source packages](https://www.debian.org/doc/debian-policy/ch-source.html)
- [Debian: ITP e sponsorship](https://mentors.debian.net/intro-maintainers/)
- [Debian Hamradio Maintainers](https://wiki.debian.org/Teams/Hams)
- [Launchpad: Personal Package Archives](https://ubuntu.com/docs/launchpad/user/explanation/feature-highlights/personal-package-archives/)
- [Copr: repository di terze parti e build](https://docs.copr.fedorainfracloud.org/user_documentation.html)
- [Fedora: Package Lifecycle](https://fedoraproject.org/wiki/Fedora_Package_Lifecycle_notes)
- [Fedora: package review](https://docs.fedoraproject.org/en-US/package-maintainers/Package_Review_Process/)

## Verifiche

I risultati della compilazione e delle regressioni mirate di questa patch sono
in `LINUX_R31_PACKAGES_RESULTS.json`. Le verifiche native Ubuntu/Fedora di
creazione, installazione e reinstallazione sono previste nel workflow GitHub;
la disponibilità dei nuovi asset richiede l'esecuzione di quel workflow.
RX, decoder, Doppler, sequencer, coda Evil, audio e compressione LOGZIP1 non
sono modificati da questa patch.
