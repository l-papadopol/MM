# R21.1 — compatibilità qint64 / Qt6 macOS

Corrette le tre chiamate segnalate in `mainwindow.cpp` e `Ft8Transmitter.cpp`. La revisione estesa ha uniformato anche un limite `1LL`, uno zero su più righe e gli argomenti interi `profile.slotMs` e `frameSamples` (MSK144 TX). Limiti, ritardi e calcoli restano invariati.

Il preflight ora controlla tutti gli argomenti delle chiamate qMin/qMax/qBound<qint64>, anche su più righe. È incluso nel release audit CTest sui sistemi Unix, quindi viene eseguito anche su Linux prima della consegna. Verificata la rilevazione dei casi che sfuggivano al vecchio controllo.

Risultati in `verification-r21_1`: preflight macOS, build Linux Qt5 e CTest. Compilazione nativa macOS Intel/Apple Silicon da rieseguire su GitHub: questo ambiente non dispone di AppleClang/macOS.
