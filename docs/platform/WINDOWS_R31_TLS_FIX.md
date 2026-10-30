# R31 clean — Windows link fix 1

Il log fornito dall'utente, commit 4829056bb0cafa155b8b47056429da832c3a9d6f, compila gli oggetti della build MSYS2 UCRT64 ma fallisce nel collegamento di MadModem.exe. Il linker MinGW GCC 16.2.0 segnala più definizioni della funzione di inizializzazione TLS di MshvReference::stopRequested, provenienti da FtReferenceRecovery, decoderft8, decoderft8var, decoderft4 e decoderpom.

La callback era un oggetto std::function non banale, definito inline thread_local in budget.h. L'header ora contiene una dichiarazione extern thread_local; la sola definizione è in budget.cpp, aggiunto al target statico madmodem_ft_reference. La callback rimane distinta per ciascun thread e resta condivisa fra le unità di compilazione dello stesso thread. Algoritmi, deadline, Doppler e opzioni di compilazione non cambiano.

Il nuovo controllo madmodem_ft_reference_budget collega tre unità di compilazione e verifica propagazione della cancellazione, azzeramento e isolamento dei thread. La build completa Linux Qt5 passa; passano anche questo controllo e la regressione del recupero nativo FT8/FT4. L'ispezione della libreria conferma una sola definizione della funzione di inizializzazione TLS.

La build Windows completa con la toolchain del log non è stata eseguita in questo runtime Linux: va rilanciata tramite il workflow già presente. La versione pubblica e il marcatore R31 clean rimangono invariati; il pacchetto WINFIX1 identifica questa correzione.
