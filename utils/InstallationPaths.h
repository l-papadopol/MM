#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace MadModemInstallation {
inline bool isManaged(const QString &executableDir = QCoreApplication::applicationDirPath())
{
    return QFileInfo::exists(QDir(executableDir).filePath(QStringLiteral("managed-install.flag")));
}

// IC8TEM edition: with "local-data.flag" next to the executable (made by the Windows
// installer of the edition) the user files live in "<installation>/data" instead of
// the per-user folders of the system. Without the flag nothing changes.
inline QString localDataRoot(const QString &executableDir = QCoreApplication::applicationDirPath())
{
    if (!QFileInfo::exists(QDir(executableDir).filePath(QStringLiteral("local-data.flag")))) return QString();
    return QDir::cleanPath(QDir(executableDir).absoluteFilePath(QStringLiteral("../data")));
}

// QStandardPaths::writableLocation() for the folders of MadModem's own files.
inline QString writableLocation(QStandardPaths::StandardLocation type)
{
    const QString local = localDataRoot();
    if (local.isEmpty()) return QStandardPaths::writableLocation(type);
    return type == QStandardPaths::CacheLocation ? QDir(local).filePath(QStringLiteral("cache")) : local;
}

// Never replace an existing user file or remove the original portable data.
// If migration fails, retain access to the original rather than silently loading
// defaults/an empty log. The existing save-error handling reports write failures.
inline QString userFile(const QString &executableDir, const QString &userDir, const QString &name,
                        QString installerBackupRoot = QString())
{
    const QString target = QDir(userDir).filePath(name);
    QString legacy = QDir(executableDir).filePath(name);
#ifdef Q_OS_MACOS
    if (installerBackupRoot.isEmpty())
        installerBackupRoot = QStringLiteral("/Library/Application Support/MadModem/Portable Backups");
#endif
    // macOS replaces the signed bundle atomically. The installer preserves
    // portable data outside it, so migration must also look in that snapshot.
    if (!QFileInfo::exists(legacy) && isManaged(executableDir) && !installerBackupRoot.isEmpty()) {
        const QDir backups(installerBackupRoot);
        const QStringList snapshots = backups.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
        if (!snapshots.isEmpty()) {
            const QString saved = backups.filePath(snapshots.first() + QLatin1Char('/') + name);
            if (QFileInfo::exists(saved)) legacy = saved;
        }
    }
    if (userDir.isEmpty()) return legacy;
    if (!QFileInfo::exists(target) && QFileInfo::exists(legacy)) {
        if (!QDir().mkpath(userDir) || !QFile::copy(legacy, target)) {
            if (!QFileInfo::exists(target)) return legacy;
        }
    }
    QDir().mkpath(userDir);
    return target;
}
inline QString settingsFilePath()
{
    const QString executableDir = QCoreApplication::applicationDirPath();
    if (!isManaged(executableDir)) return QDir(executableDir).filePath(QStringLiteral("settings.mad"));
    return userFile(executableDir, writableLocation(QStandardPaths::AppConfigLocation),
                    QStringLiteral("settings.mad"));
}
}
