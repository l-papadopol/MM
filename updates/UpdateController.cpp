#include "UpdateController.h"
#include "../MadModemVersion.h"
#include "../settings/AppSettings.h"
#include "../utils/InstallationPaths.h"
#include "../utils/RuntimeI18n.h"
#include <QAction>
#include <QApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <QUuid>
#include <QVersionNumber>
#include <memory>
namespace {
QString uiText(const char* key, const char* fallback) { return MadModemI18n::translate(QString::fromUtf8(key), QString::fromUtf8(fallback)); }
const QString repository = QStringLiteral("https://github.com/l-papadopol/MM");
bool assetUrl(const QUrl& url) {
    return url.scheme() == "https" && url.host() == "github.com" && url.userInfo().isEmpty() &&
           url.path().startsWith("/l-papadopol/MM/releases/download/");
}
}
UpdateController::UpdateController(QWidget* window, QMenu* help, std::function<bool()> idle)
    : QObject(window), m_window(window), m_idle(std::move(idle)), m_network(this) {
    window->installEventFilter(this);
    auto* action = help->addAction(uiText("text.check_for_updates", "Check for updates…"));
    action->setObjectName("checkForUpdatesAction");
    action->setProperty("i18nSourceText", "Check for updates…");
    connect(action, &QAction::triggered, this, [this] { check(true); });
    auto* automatic = help->addAction(uiText("text.check_for_updates_at_startup", "Check for updates at startup"));
    automatic->setObjectName("automaticUpdatesAction");
    automatic->setProperty("i18nSourceText", "Check for updates at startup");
    automatic->setCheckable(true);
    QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
    automatic->setChecked(settings.value("Updates/checkAtStartup", true).toBool());
    connect(automatic, &QAction::toggled, this, [](bool enabled) {
        QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
        settings.setValue("Updates/checkAtStartup", enabled);
    });
    if (automatic->isChecked() && QCoreApplication::arguments().size() == 1) QTimer::singleShot(8000, this, [this, automatic] { if (automatic->isChecked()) check(); });
}
bool UpdateController::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_window && event->type() == QEvent::Close) {
        // Shutdown may pump events while audio/logbook workers finish. Do not
        // offer an update or display a download error during that interval.
        m_cancelled = true;
        for (auto* reply : m_network.findChildren<QNetworkReply*>()) reply->abort();
        if (m_progress) {
            disconnect(m_progress, nullptr, this, nullptr);
            m_progress->hide(); m_progress->deleteLater(); m_progress = nullptr;
        }
    }
    return QObject::eventFilter(watched, event);
}
QString UpdateController::newerStableVersion(const QJsonObject& release, const QString& current) {
    if (release.value("draft").toBool() || release.value("prerelease").toBool()) return {};
    // /releases/latest selects the release promoted by the publisher. Branch
    // builds may be promoted without renaming their generated build tag.
    // Read only our two tag formats; titles are freely editable display text.
    static const QRegularExpression versionTag(QStringLiteral("\\Av?([0-9]+\\.[0-9]+\\.[0-9]+)\\z"));
    static const QRegularExpression buildTag(QStringLiteral("\\Abuild-([0-9]+\\.[0-9]+\\.[0-9]+)-[0-9]+\\z"));
    const QString tag = release.value("tag_name").toString();
    auto match = versionTag.match(tag);
    if (!match.hasMatch()) match = buildTag.match(tag);
    if (!match.hasMatch()) return {};
    const QString version = match.captured(1);
    return QVersionNumber::compare(QVersionNumber::fromString(version), QVersionNumber::fromString(current)) > 0 ? version : QString();
}
QString UpdateController::assetName(const QString& version, bool managed) {
    const QString base = "MadModem-" + version;
#ifdef Q_OS_WIN
    if (QSysInfo::buildCpuArchitecture() != "x86_64") return {};
    if (managed) return base + "-Windows-64bit-Installer-AutoCPU.exe";
#if defined(__AVX2__)
    return base + "-Windows-64bit-Portable-AVX2-FMA.zip";
#else
    return base + "-Windows-64bit-Portable-Compatible.zip";
#endif
#elif defined(Q_OS_MACOS)
    const QString arch = QSysInfo::buildCpuArchitecture();
    if (arch != "arm64" && arch != "x86_64") return {};
    return base + "-macOS-" + (arch == "arm64" ? "AppleSilicon" : "Intel") + (managed ? "-Installer.pkg" : "-Portable.zip");
#else
    if (QSysInfo::buildCpuArchitecture() != "x86_64") return {};
    QFile file("/etc/os-release"); if (!file.open(QIODevice::ReadOnly)) return {};
    const QString os = QString::fromUtf8(file.readAll());
    auto value = [&](const QString& key) { return QRegularExpression("(?:^|\\n)" + key + "=\"?([^\"\\n]+)").match(os).captured(1); };
    QString platform;
    if (value("ID") == "ubuntu" && value("VERSION_ID") == "24.04") platform = "Ubuntu24.04";
    if (value("ID") == "fedora" && value("VERSION_ID") == "43") platform = "Fedora43";
    if (platform.isEmpty()) return {};
    return base + "-Linux-" + platform + (managed ? "-64bit-Installer.run" : "-64bit-Portable.tar.gz");
#endif
}
void UpdateController::check(bool manual) {
    if (m_busy) return;
    m_manual = manual; m_cancelled = false;
    if (!m_idle()) { if (manual) fail(uiText("update_idle", "Stop transmission, automatic TX and active operations before updating.")); return; }
    m_busy = true; m_cancelled = false;
    fetch(QUrl("https://api.github.com/repos/l-papadopol/MM/releases/latest"), 2*1024*1024, [this](QByteArray bytes) {
        const auto document = QJsonDocument::fromJson(bytes);
        if (!document.isObject()) { fail(uiText("update_invalid", "Invalid release information.")); return; }
        const auto release = document.object();
        m_version = newerStableVersion(release, QStringLiteral(MADMODEM_VERSION_STRING));
        if (m_version.isEmpty()) {
            m_busy = false;
            if (m_manual) QMessageBox::information(m_window, "MadModem", uiText("update_current", "No newer stable release is available."));
            return;
        }
        offer(release);
    });
}
void UpdateController::fetch(const QUrl& url, qint64 maximum, std::function<void(QByteArray)> done) {
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "MadModem/" MADMODEM_VERSION_STRING);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = m_network.get(request);
    auto data = std::make_shared<QByteArray>();
    auto* timer = new QTimer(reply); timer->setSingleShot(true); timer->start(20000);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::readyRead, this, [reply, data, maximum] {
        data->append(reply->readAll()); if (data->size() > maximum) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, data, maximum, done] {
        const bool ok = reply->error() == QNetworkReply::NoError && data->size() <= maximum &&
                        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
        reply->deleteLater();
        if (m_cancelled) { m_busy = false; return; }
        if (!ok) { fail(uiText("update_network", "Could not retrieve the update. Check the connection and try again from Help.")); return; }
        done(*data);
    });
}
void UpdateController::offer(const QJsonObject& release) {
    if (!m_idle()) { m_busy = false; return; }
    m_managed = MadModemInstallation::isManaged();
    const QString name = assetName(m_version, m_managed);
    QJsonObject asset, sums;
    for (const auto& value : release.value("assets").toArray()) {
        const auto item = value.toObject();
        if (item.value("name").toString() == name) asset = item;
        if (item.value("name").toString() == "SHA256SUMS.txt") sums = item;
    }
    if (name.isEmpty() || asset.isEmpty() || sums.isEmpty()) {
        m_busy = false;
        if (m_manual) {
            QMessageBox::information(m_window, "MadModem", uiText("update_no_package", "A newer release exists, but no complete update package matches this installation. Opening the releases page."));
            QDesktopServices::openUrl(QUrl(repository + "/releases"));
        }
        return;
    }
    const QUrl url(asset.value("browser_download_url").toString()), sumsUrl(sums.value("browser_download_url").toString());
    const qint64 size = static_cast<qint64>(asset.value("size").toDouble());
    if (!assetUrl(url) || !assetUrl(sumsUrl) || size <= 0 || size > qint64{2}*1024*1024*1024) { fail(uiText("update_invalid", "Invalid release information.")); return; }
    QString message = uiText("update_offer", "MadModem %1 is available. Download the update?\n\nCurrent version: %2\nPackage: %3").arg(m_version, QStringLiteral(MADMODEM_VERSION_STRING), name);
    message += "\n\n" + (m_managed ? uiText("update_installer_info", "After verification, you can close MM and start the installer. Settings and logbooks are preserved; the installer may request license acceptance or system permission.") : uiText("update_portable_info", "The portable copy will remain portable. After confirmation MM closes, a full backup is kept beside the program folder, the package is replaced and MM restarts."));
    if (QMessageBox::question(m_window, "MadModem", message, QMessageBox::Yes|QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) { m_busy = false; return; }
    if (m_cancelled || !m_idle()) { m_busy = false; return; }
    m_manual = true;
    fetch(sumsUrl, 1024*1024, [this, url, name, size](const QByteArray& bytes) {
        QByteArray sha;
        for (const auto& line : bytes.split('\n')) {
            const auto match = QRegularExpression("^([a-fA-F0-9]{64}) [ *](.+)$").match(QString::fromUtf8(line).trimmed());
            if (match.hasMatch() && match.captured(2) == name) {
                if (!sha.isEmpty()) { fail(uiText("update_invalid", "Invalid release information.")); return; }
                sha = match.captured(1).toLatin1().toLower();
            }
        }
        if (sha.isEmpty()) { fail(uiText("update_checksum_missing", "The release has no checksum for this package. Update cancelled.")); return; }
        download(url, name, sha, size);
    });
}
void UpdateController::download(const QUrl& url, const QString& name, const QByteArray& sha, qint64 size) {
    m_directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/updates/" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QDir().mkpath(m_directory)) { fail(uiText("update_storage", "Cannot create the update download folder.")); return; }
    const QString path = m_directory + '/' + name;
    auto file = std::make_shared<QSaveFile>(path);
    if (!file->open(QIODevice::WriteOnly)) { fail(file->errorString()); return; }
    auto hash = std::make_shared<QCryptographicHash>(QCryptographicHash::Sha256);
    auto received = std::make_shared<qint64>(0);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "MadModem/" MADMODEM_VERSION_STRING);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = m_network.get(request);
    auto* timer = new QTimer(reply); timer->setSingleShot(true); timer->start(60000);
    auto* totalTimer = new QTimer(reply); totalTimer->setSingleShot(true); totalTimer->start(30*60*1000);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    connect(totalTimer, &QTimer::timeout, reply, &QNetworkReply::abort);
    m_progress = new QProgressDialog(uiText("update_downloading", "Downloading update…"), uiText("update_cancel", "Cancel"), 0, 100, m_window);
    m_progress->setWindowTitle("MadModem"); m_progress->setAutoClose(false); m_progress->setMinimumDuration(0);
    connect(m_progress, &QProgressDialog::canceled, this, [this, reply] { m_cancelled = true; reply->abort(); });
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, file, hash, received, size, timer] {
        timer->start(60000);
        while (reply->bytesAvailable()) {
            const auto chunk = reply->read(256*1024); *received += chunk.size();
            if (*received > size || file->write(chunk) != chunk.size()) { reply->abort(); return; }
            hash->addData(chunk);
        }
        if (m_progress) m_progress->setValue(static_cast<int>(*received * 100 / size));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, hash, received, size, sha, path] {
        if (m_progress) { disconnect(m_progress, nullptr, this, nullptr); m_progress->hide(); m_progress->deleteLater(); m_progress = nullptr; }
        const bool ok = reply->error() == QNetworkReply::NoError && *received == size && hash->result().toHex() == sha;
        reply->deleteLater();
        if (m_cancelled) { m_busy = false; return; }
        if (!ok || !file->commit()) { fail(uiText("update_corrupt", "Download failed or checksum mismatch. No program files were changed.")); return; }
        if (!m_idle()) { m_busy = false; QMessageBox::information(m_window, "MadModem", uiText("update_wait", "Update downloaded. Stop active operations before installing it from the download folder.") + "\n" + m_directory); return; }
        if (QMessageBox::question(m_window, "MadModem", uiText("update_install", "Download verified. Close MadModem and apply the update now?"), QMessageBox::Yes|QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) { m_busy = false; return; }
        if (!m_idle()) { m_busy = false; return; }
        if (launch(path, sha)) { m_window->close(); m_busy = false; }
        else fail(uiText("update_launch_failed", "Could not start the updater. The current installation is unchanged.") + "\n" + path);
    });
}
void UpdateController::fail(const QString& message) {
    m_busy = false;
    if (m_manual && !m_cancelled) QMessageBox::warning(m_window, "MadModem", message);
}
bool UpdateController::launch(const QString& path, const QByteArray& sha) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString target = QDir(appDir + "/..").absolutePath();
    if (m_managed) {
#ifdef Q_OS_WIN
        return QProcess::startDetached(path, {"/DIR=" + QDir::toNativeSeparators(target)});
#elif defined(Q_OS_MACOS)
        return QProcess::startDetached("/usr/bin/open", {path});
#else
        if (!QStandardPaths::findExecutable("zenity").isEmpty()) return QProcess::startDetached("/bin/bash", {path});
        for (const auto& terminal : {QString("x-terminal-emulator"), QString("xterm")})
            if (!QStandardPaths::findExecutable(terminal).isEmpty()) return QProcess::startDetached(terminal, {"-e", "/bin/bash", path});
        return false;
#endif
    }
#ifdef Q_OS_MACOS
    target = QDir(appDir + "/../..").absolutePath();
#endif
    QFileInfo targetInfo(target);
    if (!targetInfo.isWritable() || targetInfo.isSymLink()) return false;
#ifdef Q_OS_WIN
    const QString script = m_directory + "/apply.ps1";
    if (!QFile::copy(":/updates/apply_portable.ps1", script)) return false;
    const QString powershell = QDir::toNativeSeparators(qEnvironmentVariable("SystemRoot") + "/System32/WindowsPowerShell/v1.0/powershell.exe");
    return QProcess::startDetached(powershell, {"-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", script,
        "-ParentPid", QString::number(QCoreApplication::applicationPid()), "-TargetPath", target, "-Archive", path,
        "-ExpectedHash", QString::fromLatin1(sha), "-Version", m_version});
#else
    const QString script = m_directory + "/apply.sh";
    if (!QFile::copy(":/updates/apply_portable.sh", script)) return false;
#ifdef Q_OS_MACOS
    const QString platform = "mac";
#else
    const QString platform = "linux";
#endif
    return QProcess::startDetached("/bin/bash", {script, QString::number(QCoreApplication::applicationPid()), target, path, QString::fromLatin1(sha), m_version, platform}, m_directory);
#endif
}
