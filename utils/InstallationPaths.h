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

// Never replace an existing user file or remove the original portable data.
// If migration fails, retain access to the original rather than silently loading
// defaults/an empty log. The existing save-error handling reports write failures.
inline QString userFile(const QString &executableDir, const QString &userDir, const QString &name)
{
    const QString target = QDir(userDir).filePath(name);
    const QString legacy = QDir(executableDir).filePath(name);
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
    return userFile(executableDir, QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation),
                    QStringLiteral("settings.mad"));
}
}
