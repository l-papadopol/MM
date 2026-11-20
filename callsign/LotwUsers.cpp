#include "LotwUsers.h"

/*
 * LoTW users (IC8TEM, from MSHV-IC8TEM). See LotwUsers.h.
 * Copyright (C) 2026 Costantino Cerrotta, IC8TEM. GNU GPL v3 or later.
 */

#include "../MadModemVersion.h"
// Application identity comes from MadModemVersion.h; defaults support standalone builds.
#ifndef MADMODEM_EDITION
#define MADMODEM_EDITION ""
#endif
#ifndef MADMODEM_UPDATE_REPOSITORY
#define MADMODEM_UPDATE_REPOSITORY "l-papadopol/MM"
#endif
#include "../settings/AppSettings.h"
#include "../utils/InstallationPaths.h"
#include "../utils/RuntimeI18n.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDateEdit>
#include <QDateTime>
#include <QSharedPointer>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QSslSocket>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QHash<QString, int> g_calls; // call -> julian day of the last LoTW upload
QDate g_lastDate;            // newest upload in the list (age of the list)
int g_minJulianDay = 0;
bool g_showMarker = true;
constexpr int kRefreshDays = 14;

QString uiText(const char *key, const char *fallback)
{
    return MadModemI18n::translate(QString::fromUtf8(key), QString::fromUtf8(fallback));
}

// "EA8/IK2ABC/P" -> "IK2ABC": the longest part with a digit and a letter
QString baseCall(const QString &c)
{
    QString result;
    for (const QString &part : c.split(QLatin1Char('/'))) {
        bool digit = false, letter = false;
        for (QChar ch : part) {
            if (ch.isDigit()) digit = true;
            else if (ch.isLetter()) letter = true;
        }
        if (digit && letter && part.size() > result.size()) result = part;
    }
    return result;
}

int parseCsv(const QString &path, QHash<QString, int> &out, QDate &last)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return -1;
    out.clear();
    out.reserve(200000);
    last = QDate();
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        const QStringList l = line.split(QLatin1Char(','));
        if (l.count() < 2) continue;
        QString call = l.at(0).trimmed().toUpper();
        call.remove(QLatin1Char('"'));
        QString day = l.at(1).trimmed();
        day.remove(QLatin1Char('"'));
        const QDate d = QDate::fromString(day, QStringLiteral("yyyy-MM-dd"));
        if (call.isEmpty() || !d.isValid()) continue; // header or bad line
        const int jd = static_cast<int>(d.toJulianDay());
        auto it = out.find(call);
        if (it == out.end()) out.insert(call, jd);
        else if (jd > it.value()) it.value() = jd;
        if (!last.isValid() || d > last) last = d;
    }
    return out.count();
}

QString dataFolder()
{
    const QString exe = QCoreApplication::applicationDirPath();
    return MadModemInstallation::isManaged(exe) ? MadModemInstallation::writableLocation(QStandardPaths::AppDataLocation) : exe;
}

} // namespace

bool LotwUsers::hasList()
{
    return !g_calls.isEmpty();
}

bool LotwUsers::isUser(const QString &call)
{
    return g_showMarker && isValid(call);
}

bool LotwUsers::isValid(const QString &call0)
{
    if (g_calls.isEmpty() || call0.size() < 3) return false;
    const QString call = call0.trimmed().toUpper();
    auto it = g_calls.constFind(call);
    if (it == g_calls.constEnd() && call.contains(QLatin1Char('/'))) it = g_calls.constFind(baseCall(call));
    if (it == g_calls.constEnd()) return false;
    return it.value() >= g_minJulianDay;
}

QDate LotwUsers::lastUpload(const QString &call0)
{
    const QString call = call0.trimmed().toUpper();
    auto it = g_calls.constFind(call);
    if (it == g_calls.constEnd() && call.contains(QLatin1Char('/'))) it = g_calls.constFind(baseCall(call));
    return it == g_calls.constEnd() ? QDate() : QDate::fromJulianDay(it.value());
}

LotwUsers::LotwUsers(QObject *parent)
    : QObject(parent), m_csvPath(QDir(dataFolder()).filePath(QStringLiteral("lotw-user-activity.csv")))
{
    m_network = new QNetworkAccessManager(this);
    readSettings();
    if (QFile::exists(m_csvPath)) loadInBackground(m_csvPath, false);
    QTimer::singleShot(45000, this, &LotwUsers::checkAge); // not during the start of MadModem
    auto *daily = new QTimer(this);
    connect(daily, &QTimer::timeout, this, &LotwUsers::checkAge);
    daily->start(6 * 3600 * 1000);
}

LotwUsers::~LotwUsers() = default;

void LotwUsers::readSettings()
{
    QSettings s(AppSettings::settingsFilePath(), QSettings::IniFormat);
    s.beginGroup(QStringLiteral("Lotw"));
    m_showMarker = s.value(QStringLiteral("showMarker"), true).toBool();
    m_filterFt = s.value(QStringLiteral("filterFt"), false).toBool();
    m_autoUpdate = s.value(QStringLiteral("autoUpdate"), true).toBool();
    m_minDate = QDate::fromString(s.value(QStringLiteral("minDate")).toString(), QStringLiteral("yyyy-MM-dd"));
    s.endGroup();
    if (!m_minDate.isValid()) m_minDate = QDate::currentDate().addYears(-1);
    g_showMarker = m_showMarker;
    g_minJulianDay = static_cast<int>(m_minDate.toJulianDay());
}

void LotwUsers::saveSettings() const
{
    QSettings s(AppSettings::settingsFilePath(), QSettings::IniFormat);
    s.beginGroup(QStringLiteral("Lotw"));
    s.setValue(QStringLiteral("showMarker"), m_showMarker);
    s.setValue(QStringLiteral("filterFt"), m_filterFt);
    s.setValue(QStringLiteral("autoUpdate"), m_autoUpdate);
    s.setValue(QStringLiteral("minDate"), m_minDate.toString(QStringLiteral("yyyy-MM-dd")));
    s.endGroup();
}

void LotwUsers::setShowMarker(bool on)
{
    if (m_showMarker == on) return;
    m_showMarker = on;
    g_showMarker = on;
    saveSettings();
    emit changed();
}

void LotwUsers::setFilterFt(bool on)
{
    if (m_filterFt == on) return;
    m_filterFt = on;
    saveSettings();
    emit changed();
    emit listChanged();
}

void LotwUsers::setMinDate(const QDate &date)
{
    if (!date.isValid() || m_minDate == date) return;
    m_minDate = date;
    g_minJulianDay = static_cast<int>(date.toJulianDay());
    saveSettings();
    emit changed();
    emit listChanged();
}

void LotwUsers::setAutoUpdate(bool on)
{
    if (m_autoUpdate == on) return;
    m_autoUpdate = on;
    saveSettings();
    if (on) QTimer::singleShot(500, this, &LotwUsers::checkAge);
    emit changed();
}

void LotwUsers::checkAge()
{
    if (!m_autoUpdate || m_downloading) return;
    const QFileInfo fi(m_csvPath);
    if (!fi.exists() || fi.lastModified().daysTo(QDateTime::currentDateTime()) >= kRefreshDays) download();
}

void LotwUsers::loadInBackground(const QString &path, bool replaceSaved)
{
    struct Result { QHash<QString, int> calls; QDate last; int count = -1; };
    auto result = QSharedPointer<Result>::create();
    QThread *thread = QThread::create([path, result]() { result->count = parseCsv(path, result->calls, result->last); });
    connect(thread, &QThread::finished, this, [this, thread, result, path, replaceSaved]() {
        thread->deleteLater();
        if (result->count > 1000) {
            g_calls.swap(result->calls);
            g_lastDate = result->last;
            m_error.clear();
            if (replaceSaved) {
                QFile::remove(m_csvPath);
                if (!QFile::rename(path, m_csvPath)) {
                    QFile::copy(path, m_csvPath);
                    QFile::remove(path);
                }
            }
            emit listChanged();
        } else {
            if (replaceSaved) QFile::remove(path);
            m_error = uiText("lotw_bad_file", "No valid lines found (expected CALL,YYYY-MM-DD,HH:MM:SS).");
        }
        emit changed();
    });
    thread->start(QThread::LowPriority);
}

bool LotwUsers::loadFile(const QString &path, QString *error)
{
    QHash<QString, int> calls;
    QDate last;
    if (parseCsv(path, calls, last) <= 0) {
        if (error) *error = uiText("lotw_bad_file", "No valid lines found (expected CALL,YYYY-MM-DD,HH:MM:SS).");
        return false;
    }
    if (QFileInfo(path).absoluteFilePath() != QFileInfo(m_csvPath).absoluteFilePath()) {
        QDir().mkpath(QFileInfo(m_csvPath).absolutePath());
        QFile::remove(m_csvPath);
        QFile::copy(path, m_csvPath);
    }
    g_calls.swap(calls);
    g_lastDate = last;
    m_error.clear();
    emit listChanged();
    emit changed();
    return true;
}

void LotwUsers::download()
{
    if (m_downloading) return;
    m_downloading = true;
    m_progress = 0;
    m_error.clear();
    emit changed();
    QDir().mkpath(QFileInfo(m_csvPath).absolutePath());
    const QString userAgent = QStringLiteral("MadModem/" MADMODEM_VERSION_STRING " " MADMODEM_EDITION);
    if (QSslSocket::supportsSsl()) {
        QNetworkRequest request{QUrl(url())};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, userAgent);
        QNetworkReply *reply = m_network->get(request);
        connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64) {
            m_progress = got;
            emit changed();
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) downloadDone(false, QByteArray(), reply->errorString());
            else downloadDone(true, reply->readAll(), QString());
        });
        return;
    }
    const QString curl = QStandardPaths::findExecutable(QStringLiteral("curl")); // Qt without OpenSSL
    if (curl.isEmpty()) {
        downloadDone(false, QByteArray(), uiText("loc_no_https", "HTTPS is not available (no OpenSSL and no curl)"));
        return;
    }
    const QString tmp = m_csvPath + QStringLiteral(".dl");
    m_curl = new QProcess(this);
    connect(m_curl, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, tmp](int code, QProcess::ExitStatus status) {
                QProcess *p = m_curl;
                m_curl = nullptr;
                p->deleteLater();
                if (status != QProcess::NormalExit || code != 0) {
                    const QString err = QString::fromLocal8Bit(p->readAllStandardError()).trimmed();
                    QFile::remove(tmp);
                    downloadDone(false, QByteArray(), err.isEmpty() ? QStringLiteral("curl %1").arg(code) : err);
                    return;
                }
                QFile f(tmp);
                QByteArray data;
                if (f.open(QIODevice::ReadOnly)) data = f.readAll();
                f.close();
                QFile::remove(tmp);
                downloadDone(true, data, QString());
            });
    m_curl->start(curl, {QStringLiteral("-L"), QStringLiteral("-s"), QStringLiteral("-S"), QStringLiteral("-f"),
                         QStringLiteral("--max-time"), QStringLiteral("300"), QStringLiteral("-A"), userAgent,
                         QStringLiteral("-o"), tmp, url()});
}

void LotwUsers::downloadDone(bool ok, const QByteArray &data, const QString &error)
{
    m_downloading = false;
    if (!ok || data.size() < 1000) {
        m_error = uiText("loc_download_failed", "Download failed: %1").arg(error.isEmpty() ? uiText("loc_empty_answer", "empty answer") : error);
        emit changed();
        return;
    }
    const QString tmp = m_csvPath + QStringLiteral(".tmp");
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size()) {
        f.close();
        QFile::remove(tmp);
        m_error = uiText("loc_cannot_write", "Cannot write %1").arg(tmp);
        emit changed();
        return;
    }
    f.close();
    loadInBackground(tmp, true); // replaces the saved list only when valid
    emit changed();
}

QString LotwUsers::status() const
{
    QString t;
    if (m_downloading) {
        t = uiText("lotw_downloading", "Downloading... %1 MB").arg(m_progress / 1048576.0, 0, 'f', 1);
    } else if (g_calls.isEmpty()) {
        t = uiText("lotw_no_list", "No list loaded.");
    } else {
        int valid = 0;
        for (auto it = g_calls.constBegin(); it != g_calls.constEnd(); ++it) {
            if (it.value() >= g_minJulianDay) ++valid;
        }
        t = uiText("lotw_status", "%1 calls in the list, %2 with an upload on or after %3. Newest upload in the list: %4.")
                .arg(g_calls.size()).arg(valid).arg(m_minDate.toString(QStringLiteral("yyyy-MM-dd")))
                .arg(g_lastDate.toString(QStringLiteral("yyyy-MM-dd")));
    }
    if (!m_error.isEmpty()) t += QStringLiteral("\n") + m_error;
    return t;
}

// ------------------------------------------------------------------ dialog

LotwUsersDialog::LotwUsersDialog(LotwUsers *users, QWidget *parent)
    : QDialog(parent), m_users(users)
{
    setWindowTitle(uiText("lotw_title", "LoTW users"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    auto *box = new QGroupBox(uiText("lotw_group", "LoTW users (Logbook of The World)"), this);
    auto *info = new QLabel(uiText("lotw_info", "List of the LoTW users from ARRL:") + QStringLiteral("<br><a href=\"%1\">%1</a><br>").arg(LotwUsers::url())
                            + uiText("lotw_info2", "Download it here, or open the link, save the file and load it."), box);
    info->setTextFormat(Qt::RichText);
    info->setTextInteractionFlags(Qt::TextBrowserInteraction);
    info->setOpenExternalLinks(true);
    info->setWordWrap(true);
    m_download = new QPushButton(uiText("lotw_download", "Download from ARRL and load"), box);
    auto *load = new QPushButton(uiText("lotw_load", "Load a LoTW CSV file..."), box);
    auto *buttons = new QHBoxLayout();
    buttons->addWidget(m_download);
    buttons->addWidget(load);
    buttons->addStretch(1);
    auto *dateLabel = new QLabel(uiText("lotw_min_date", "Valid if the last LoTW upload is on or after:"), box);
    auto *date = new QDateEdit(users->minDate(), box);
    date->setCalendarPopup(true);
    date->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    auto *dateRow = new QHBoxLayout();
    dateRow->addWidget(dateLabel);
    dateRow->addWidget(date);
    dateRow->addStretch(1);
    auto *marker = new QCheckBox(uiText("lotw_marker", "Mark the decodes of LoTW users with a dot after the message"), box);
    marker->setChecked(users->showMarker());
    auto *filter = new QCheckBox(uiText("lotw_filter", "FT decode list: show LoTW users only (messages to you are always shown)"), box);
    filter->setChecked(users->filterFt());
    auto *autoUpdate = new QCheckBox(uiText("lotw_auto", "Download the list again every 14 days"), box);
    autoUpdate->setChecked(users->autoUpdate());
    m_status = new QLabel(box);
    m_status->setWordWrap(true);
    auto *layout = new QVBoxLayout(box);
    layout->addWidget(info);
    layout->addLayout(buttons);
    layout->addLayout(dateRow);
    layout->addWidget(marker);
    layout->addWidget(filter);
    layout->addWidget(autoUpdate);
    layout->addWidget(m_status);
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::close);
    auto *outer = new QVBoxLayout(this);
    outer->addWidget(box);
    outer->addWidget(close);
    resize(600, 380);

    connect(m_download, &QPushButton::clicked, users, &LotwUsers::download);
    connect(load, &QPushButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(this, uiText("lotw_open", "Open a LoTW CSV file"), QDir::homePath(),
                                                          QStringLiteral("LoTW CSV (*.csv);;All files (*)"));
        if (file.isEmpty()) return;
        QString error;
        if (!m_users->loadFile(file, &error)) QMessageBox::warning(this, QStringLiteral("MadModem"), error);
    });
    connect(date, &QDateEdit::dateChanged, users, &LotwUsers::setMinDate);
    connect(marker, &QCheckBox::toggled, users, &LotwUsers::setShowMarker);
    connect(filter, &QCheckBox::toggled, users, &LotwUsers::setFilterFt);
    connect(autoUpdate, &QCheckBox::toggled, users, &LotwUsers::setAutoUpdate);
    connect(users, &LotwUsers::changed, this, [this, filter, marker]() {
        const QSignalBlocker b1(filter), b2(marker);
        filter->setChecked(m_users->filterFt());
        marker->setChecked(m_users->showMarker());
        refresh();
    });
    refresh();
}

void LotwUsersDialog::refresh()
{
    m_status->setText(m_users->status());
    m_download->setEnabled(!m_users->downloading());
}
