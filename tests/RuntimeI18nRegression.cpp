#include "utils/RuntimeI18n.h"
#include <QCoreApplication>
#include <iostream>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    MadModemI18n::setLanguageCode("it");
    bool ok = MadModemI18n::text("Track Moon / EME") == QStringLiteral("Segui Luna / EME");
    ok &= MadModemI18n::text("User / QTH / Macros") == QStringLiteral("Utente / QTH / Macro");
    ok &= MadModemI18n::text("Logbook / FT colours") == QStringLiteral("Registro QSO / colori FT");
    std::cout << (ok ? "PASS" : "FAIL") << " translated labels containing slash\n";
    return ok ? 0 : 1;
}
