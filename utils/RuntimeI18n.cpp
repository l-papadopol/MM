#include "RuntimeI18n.h"

#include <QFile>
#include <QHash>
#include <QTextStream>
#include <QtGlobal>

namespace {

QString g_languageCode = QStringLiteral("en");
QHash<QString, QString> g_translations;

QString normalizeSourceKey(const QString &source)
{
    QString normalized;
    normalized.reserve(source.size());

    for (const QChar ch : source) {
        if (ch.isLetterOrNumber()) {
            normalized.append(ch.toLower());
        } else if (!normalized.endsWith(QLatin1Char('_'))) {
            normalized.append(QLatin1Char('_'));
        }
    }

    while (normalized.startsWith(QLatin1Char('_'))) normalized.remove(0, 1);
    while (normalized.endsWith(QLatin1Char('_'))) normalized.chop(1);
    if (normalized.isEmpty()) normalized = QStringLiteral("empty");
    if (normalized.size() > 90) normalized = normalized.left(90);
    return normalized;
}

bool looksLikeBadGeneratedValue(const QString &value, const QString &normalized, const QString &source)
{
    const QString t = value.trimmed();
    if (t.isEmpty()) return true;
    if (t == normalized) return true;
    if (t == source.trimmed() && t.contains(QLatin1Char('_'))) return true;
    if (t.contains(QLatin1Char('_')) && !t.contains(QLatin1Char(' ')) && t.toLower() == t) return true;
    if (t.endsWith(QStringLiteral(".h"), Qt::CaseInsensitive) ||
        t.endsWith(QStringLiteral(".cpp"), Qt::CaseInsensitive) ||
        t.endsWith(QStringLiteral(".moc"), Qt::CaseInsensitive) ||
        t.contains(QStringLiteral("../")) ||
        t.contains(QLatin1Char('/'))) {
        return true;
    }
    return false;
}

} // namespace

namespace MadModemI18n {

void setLanguageCode(const QString &languageCode)
{
    QString code = languageCode.trimmed().toLower();
    if (code.isEmpty()) {
        code = QStringLiteral("en");
    }
    if (code.startsWith(QStringLiteral("it"))) code = QStringLiteral("it");
    else if (code.startsWith(QStringLiteral("fr"))) code = QStringLiteral("fr");
    else if (code.startsWith(QStringLiteral("de"))) code = QStringLiteral("de");
    else if (code.startsWith(QStringLiteral("no")) || code.startsWith(QStringLiteral("nb")) || code.startsWith(QStringLiteral("nn"))) code = QStringLiteral("no");
    else if (code.startsWith(QStringLiteral("cs")) || code.startsWith(QStringLiteral("cz"))) code = QStringLiteral("cs");
    else code = QStringLiteral("en");

    g_languageCode = code;
    g_translations.clear();

    QFile file(QStringLiteral(":/translations/ui_%1.ini").arg(code));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#endif
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char('['))) {
            continue;
        }
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            continue;
        }
        const QString key = line.left(eq).trimmed();
        QString value = line.mid(eq + 1).trimmed();
        value.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
        g_translations.insert(key, value);
    }
}

QString languageCode()
{
    return g_languageCode;
}

QString translate(const QString &key, const QString &fallback)
{
    return g_translations.value(key, fallback);
}

QString translateSource(const QString &prefix, const QString &source)
{
    const QString trimmed = source.trimmed();
    if (trimmed.isEmpty()) {
        return source;
    }

    const QString normalized = normalizeSourceKey(source);
    if (g_translations.contains(trimmed)) {
        return g_translations.value(trimmed);
    }
    if (g_translations.contains(normalized)) {
        return g_translations.value(normalized);
    }

    const QString generatedKey = prefix + QStringLiteral(".") + normalized;
    const QString translated = g_translations.value(generatedKey, QString());
    if (!translated.isEmpty() && !looksLikeBadGeneratedValue(translated, normalized, source)) {
        return translated;
    }

    return source;
}

QString text(const QString &source)
{
    return translateSource(QStringLiteral("text"), source);
}

QString placeholder(const QString &source)
{
    return translateSource(QStringLiteral("placeholder"), source);
}

} // namespace MadModemI18n
