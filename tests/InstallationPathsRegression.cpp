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
    return all ? 0 : 1;
}
