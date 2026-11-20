#include "LocalDataMigration.h"
#include "InstallationPaths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>
#include <QVariant>

namespace MadModemInstallation {
namespace {

#ifdef Q_OS_WIN
constexpr Qt::CaseSensitivity kPathCase = Qt::CaseInsensitive;
#else
constexpr Qt::CaseSensitivity kPathCase = Qt::CaseSensitive;
#endif

// The original MadModem by IZ6NNH (its Inno Setup uninstall entry). When it is
// installed, its data in the per-user folders is copied, never moved.
bool originalMadModemInstalled()
{
#ifdef Q_OS_WIN
    const QString key = QStringLiteral("\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
                                       "{E9A289C9-3586-49BE-82A2-72594C56E2B8}_is1");
    for (const QString &hive : {QStringLiteral("HKEY_CURRENT_USER"), QStringLiteral("HKEY_LOCAL_MACHINE")}) {
        for (const QSettings::Format format : {QSettings::Registry64Format, QSettings::Registry32Format}) {
            QSettings registry(hive + key, format);
            if (!registry.value(QStringLiteral("UninstallString")).toString().isEmpty()) return true;
        }
    }
#endif
    return false;
}

QString cleanPath(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

bool copyRecursively(const QString &source, const QString &target)
{
    const QFileInfo info(source);
    if (info.isDir() && !info.isSymLink()) {
        if (!QDir().mkpath(target)) return false;
        const QFileInfoList entries = QDir(source).entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QFileInfo &entry : entries) {
            if (!copyRecursively(entry.absoluteFilePath(), QDir(target).filePath(entry.fileName()))) return false;
        }
        return true;
    }
    return QFile::copy(source, target);
}

void removeRecursively(const QString &path)
{
    const QFileInfo info(path);
    if (info.isDir() && !info.isSymLink()) QDir(path).removeRecursively();
    else QFile::remove(path);
}

} // namespace

QString migrateToLocalData()
{
    const QString root = localDataRoot();
    if (root.isEmpty()) return QString();
    const QDir data(root);
    if (!QDir().mkpath(root)) {
        return QStringLiteral("Data folder %1 cannot be created.").arg(QDir::toNativeSeparators(root));
    }
    const QString marker = data.filePath(QStringLiteral("migration.done"));
    if (QFileInfo::exists(marker)) return QString();

    // The per-user folders of the installed MadModem (the same folder can serve
    // several types).
    QStringList legacy;
    for (const QStandardPaths::StandardLocation type :
         {QStandardPaths::AppConfigLocation, QStandardPaths::AppLocalDataLocation, QStandardPaths::AppDataLocation}) {
        const QString dir = cleanPath(QStandardPaths::writableLocation(type));
        if (dir.isEmpty() || !QFileInfo(dir).isDir() || dir.compare(cleanPath(root), kPathCase) == 0) continue;
        bool known = false;
        for (const QString &seen : legacy) known = known || seen.compare(dir, kPathCase) == 0;
        if (!known) legacy << dir;
    }

    const bool move = !originalMadModemInstalled();
    QStringList done, kept;
    for (const QString &dir : legacy) {
        const QFileInfoList entries = QDir(dir).entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QFileInfo &entry : entries) {
            const QString name = entry.fileName();
            const QString source = entry.absoluteFilePath();
            if (name == QLatin1String("cache")) {
                // Downloads of the updater (the running setup may be among them).
                if (move) removeRecursively(source);
                continue;
            }
            const QString target = data.filePath(name);
            if (QFileInfo::exists(target)) {
                kept << name;
                continue;
            }
            bool ok = move && QDir().rename(source, target); // a rename on the same drive
            if (!ok) {
                ok = copyRecursively(source, target);
                if (ok && move) removeRecursively(source);
            }
            (ok ? done : kept) << name;
        }
        if (move) QDir().rmdir(dir); // only when empty
    }

    // Settings that point into the old folders (logbook, diagnostics, ...).
    const QString settingsFile = data.filePath(QStringLiteral("settings.mad"));
    if (!done.isEmpty() && QFileInfo::exists(settingsFile)) {
        QSettings settings(settingsFile, QSettings::IniFormat);
        for (const QString &key : settings.allKeys()) {
            const QVariant value = settings.value(key);
            if (value.userType() != QMetaType::QString) continue;
            const QString path = cleanPath(value.toString());
            for (const QString &dir : legacy) {
                if (!path.startsWith(dir + QLatin1Char('/'), kPathCase)) continue;
                settings.setValue(key, QDir::toNativeSeparators(data.filePath(path.mid(dir.length() + 1))));
                break;
            }
        }
        settings.sync();
    }

    QString report;
    if (!done.isEmpty()) {
        QStringList from;
        for (const QString &dir : legacy) from << QDir::toNativeSeparators(dir);
        report = QStringLiteral("Data folder %1: %2 from %3: %4.")
                     .arg(QDir::toNativeSeparators(root),
                          move ? QStringLiteral("moved") : QStringLiteral("copied"),
                          from.join(QStringLiteral(", ")), done.join(QStringLiteral(", ")));
    }
    if (!kept.isEmpty()) {
        report += (report.isEmpty() ? QString() : QStringLiteral(" "))
                  + QStringLiteral("Left in the old folders (already in %1 or not readable): %2.")
                        .arg(QDir::toNativeSeparators(root), kept.join(QStringLiteral(", ")));
    }

    QFile file(marker);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        // Not a UI text: QStringLiteral keeps it out of the translation harvest.
        out << QStringLiteral("MadModem IC8TEM: user data of the per-user folders brought here on %1\n%2\n")
                   .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate),
                        report.isEmpty() ? QStringLiteral("Nothing to bring.") : report);
    }
    return report;
}

} // namespace MadModemInstallation
