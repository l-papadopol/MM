#ifndef MADMODEM_RUNTIMEI18N_H
#define MADMODEM_RUNTIMEI18N_H

#include <QString>

namespace MadModemI18n {

void setLanguageCode(const QString &languageCode);
QString languageCode();
QString translate(const QString &key, const QString &fallback);
QString translateSource(const QString &prefix, const QString &source);
QString text(const QString &source);
QString placeholder(const QString &source);

} // namespace MadModemI18n

#endif // MADMODEM_RUNTIMEI18N_H
