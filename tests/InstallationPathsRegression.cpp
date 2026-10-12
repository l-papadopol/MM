#include "../utils/InstallationPaths.h"
#include <QTemporaryDir>
#include <QTextStream>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir storage;
    if (!storage.isValid()) return 1;
    bool all = true;
    auto check = [&](bool ok, const char *name) {
        all &= ok; QTextStream(stdout) << (ok ? "PASS " : "FAIL ") << name << '\n';
    };
    auto write = [&](const QString &path, const QByteArray &data) {
        QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    };
    auto read = [](const QString &path) { QFile file(path); file.open(QIODevice::ReadOnly); return file.readAll(); };
    const QString install = storage.filePath("application"), user = storage.filePath("user/data");
    QDir().mkpath(install);
    check(!MadModemInstallation::isManaged(install), "portable builds retain portable behavior");
    check(write(install + "/managed-install.flag", "managed"), "managed marker created");
    check(MadModemInstallation::isManaged(install), "managed installation recognized");
    for (const QString &name : {QStringLiteral("settings.mad"), QStringLiteral("logbook.adi")}) {
        const QString legacy = install + '/' + name, target = user + '/' + name;
        check(write(legacy, "original user data"), "legacy data available");
        check(MadModemInstallation::userFile(install, user, name) == target, "migrate to writable user folder");
        check(read(target) == "original user data" && read(legacy) == "original user data", "migration preserves original bytes and source");
        check(write(target, "newer user data"), "user data changed after installation");
        check(MadModemInstallation::userFile(install, user, name) == target && read(target) == "newer user data", "reinstall never overwrites newer user data");
        check(MadModemInstallation::userFile(install, QString(), name) == legacy, "missing platform data path preserves legacy access");
        const QString blocker = storage.filePath("blocked");
        check(write(blocker, "not a directory"), "migration failure fixture");
        check(MadModemInstallation::userFile(install, blocker + "/child", name) == legacy, "failed migration retains original data access");
    }
    const QString empty = storage.filePath("fresh/user");
    check(MadModemInstallation::userFile(install, empty, "new-log.adi") == empty + "/new-log.adi" && QDir(empty).exists(), "new install creates user directory without inventing a log");
    const QString backups = storage.filePath("Portable Backups");
    QDir().mkpath(backups + "/20261011T100000Z-old");
    QDir().mkpath(backups + "/20261011T110000Z-new");
    const QString saved = backups + "/20261011T110000Z-new/settings.mad";
    check(write(backups + "/20261011T100000Z-old/settings.mad", "older") && write(saved, "latest portable settings"), "installer snapshots available");
    QFile::remove(install + "/settings.mad");
    const QString migrated = storage.filePath("mac-user");
    check(MadModemInstallation::userFile(install, migrated, "settings.mad", backups) == migrated + "/settings.mad" &&
          read(migrated + "/settings.mad") == "latest portable settings" && read(saved) == "latest portable settings",
          "atomic Mac upgrade migrates preserved portable data without changing backup");
    check(write(migrated + "/settings.mad", "current user settings") &&
          MadModemInstallation::userFile(install, migrated, "settings.mad", backups) == migrated + "/settings.mad" &&
          read(migrated + "/settings.mad") == "current user settings", "Mac reinstall keeps newer user settings");
    check(MadModemInstallation::userFile(install, storage.filePath("blocked/child"), "settings.mad", backups) == saved,
          "failed Mac migration retains access to preserved data");
    return all ? 0 : 1;
}
