# Build dei sorgenti R31

Dipendenze e comandi di compilazione rimangono quelli di BUILD_R30.md. FFTW3 double precision era già richiesto dalla R30. Il nuovo codice è C++17 e non aggiunge un compilatore Fortran.

La revisione clean è compilata in una cartella nuova, senza riutilizzare oggetti della prima R31. Build eseguita in questa consegna: Linux x86_64, Qt 5.15.13, GCC 13, CMake 3.28, Hamlib 4.5.5, Release, BUILD_TESTING=ON. Qt Location non era disponibile nel runtime di verifica ed è stato disattivato tramite l'opzione di configurazione esistente. La verifica locale usava un prefisso temporaneo di dipendenze; non è incluso nello ZIP.

Comandi ordinari, dopo l'installazione delle dipendenze della piattaforma:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

Il workflow .github/workflows/source-regression.yml offre le build Qt5/Qt6 e le regressioni su GitHub, avviabili manualmente. Le prove non eseguite sulla revisione finale sono elencate nel rapporto R31. Questo pacchetto contiene sorgenti e documentazione, senza binari di distribuzione.
