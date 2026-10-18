#include "QrzLookupPanel.h"
#include "../MadModemVersion.h"
#include "../settings/AppSettings.h"
#include "../utils/RuntimeI18n.h"
#include <QNetworkReply>
#include <QTextBrowser>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QLabel>
#include <QSettings>
#include <QDesktopServices>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <memory>
namespace {
QString uiText(const char* key, const char* fallback) { return MadModemI18n::translate(QString::fromUtf8(key), QString::fromUtf8(fallback)); }
}
QrzResponse QrzResponse::parse(const QByteArray& xml) {
    QrzResponse result;
    if (xml.size() > 1024*1024 || xml.contains("<!DOCTYPE") || xml.contains("<!ENTITY")) return result;
    QXmlStreamReader reader(xml);
    if (!reader.readNextStartElement() || reader.name() != QLatin1String("QRZDatabase")) return result;
    while (reader.readNextStartElement()) {
        const bool session = reader.name() == QLatin1String("Session");
        const bool callsign = reader.name() == QLatin1String("Callsign");
        if (!session && !callsign) { reader.skipCurrentElement(); continue; }
        while (reader.readNextStartElement()) {
            const QString name = reader.name().toString();
            const QString value = reader.readElementText(QXmlStreamReader::SkipChildElements).left(2048);
            if (callsign) result.fields.insert(name, value);
            else if (name == "Key") result.key = value;
            else if (name == "Error") result.error = value;
            else if (name == "Message") result.message = value;
        }
    }
    while (!reader.atEnd()) reader.readNext();
    result.valid = !reader.hasError();
    return result;
}
QrzLookupPanel::QrzLookupPanel(QWidget* parent) : QGroupBox(QStringLiteral("QRZ.COM"), parent), m_network(this) {
    setObjectName(QStringLiteral("qrzLookupPanel"));
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(6, 4, 6, 6); layout->setSpacing(4);
    m_details = new QTextBrowser(this); m_details->setObjectName(QStringLiteral("qrzStationDetails"));
    m_details->setOpenLinks(false); m_details->setMinimumHeight(58); m_details->setMaximumHeight(100);
    m_details->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(m_details);
    auto *buttons = new QHBoxLayout;
    auto *connectButton = new QPushButton(uiText("text.qrz_account", "QRZ account…"), this);
    connectButton->setObjectName(QStringLiteral("qrzAccountButton"));
    connectButton->setProperty("i18nSourceText", "QRZ account…");
    m_open = new QPushButton(uiText("text.open_qrz_page", "Open QRZ page"), this);
    m_open->setProperty("i18nSourceText", "Open QRZ page");
    buttons->addWidget(connectButton); buttons->addWidget(m_open); layout->addLayout(buttons);
    connect(connectButton, &QPushButton::clicked, this, [this] { account(); });
    connect(m_open, &QPushButton::clicked, this, [this] {
        if (!m_call.isEmpty()) QDesktopServices::openUrl(QUrl(QStringLiteral("https://www.qrz.com/db/") + QString::fromLatin1(QUrl::toPercentEncoding(m_call))));
    });
    m_delay.setSingleShot(true); m_delay.setInterval(450);
    connect(&m_delay, &QTimer::timeout, this, [this] { lookup(); });
    QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
    m_username = settings.value("QRZ/username").toString();
    setCallsign({});
}
void QrzLookupPanel::cancel() {
    ++m_generation; m_delay.stop();
    if (m_reply) m_reply->abort();
    m_reply = nullptr;
}
void QrzLookupPanel::setCallsign(const QString& raw) {
    const QString call = raw.trimmed().toUpper();
    const bool valid = QRegularExpression("^(?:[A-Z0-9]{1,4}/)?[A-Z0-9]{1,3}[0-9][A-Z]{1,4}(?:/[A-Z0-9]{1,4})?$").match(call).hasMatch();
    const QString selected = valid ? call : QString();
    if (!selected.isEmpty() && selected == m_call) return;
    cancel(); m_call = selected; m_open->setEnabled(!m_call.isEmpty());
    if (m_call.isEmpty()) { showStatus(uiText("qrz_select", "Select a callsign in the RX text or QSO form.")); return; }
    auto cached = m_cache.constFind(m_call);
    if (cached != m_cache.constEnd() && cached->time.secsTo(QDateTime::currentDateTimeUtc()) < 600) { showResult(cached->response); return; }
    if (m_password.isEmpty()) { showStatus(uiText("qrz_connect_help", "Connect your QRZ account for station details. Complete data requires XML access.")); return; }
    showStatus(uiText("qrz_loading", "Looking up station…")); m_delay.start();
}
void QrzLookupPanel::showStatus(const QString& status) {
    m_details->setHtml((m_call.isEmpty() ? QString() : "<b>"+m_call.toHtmlEscaped()+"</b><br>") + status.toHtmlEscaped().replace("\n", "<br>"));
}
void QrzLookupPanel::account() {
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("QRZ.COM")); dialog.resize(430, 240);
    auto *layout = new QVBoxLayout(&dialog);
    auto *info = new QLabel(uiText("qrz_credentials_help", "QRZ XML access is required for complete station data. The username is remembered; the password stays in memory only until MM closes."), &dialog); info->setWordWrap(true); layout->addWidget(info);
    auto *form = new QFormLayout;
    auto *username = new QLineEdit(m_username, &dialog); auto *password = new QLineEdit(&dialog); password->setEchoMode(QLineEdit::Password);
    form->addRow(uiText("qrz_username", "Username"), username); form->addRow(uiText("qrz_password", "Password"), password); layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, &dialog); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    cancel(); m_username = username->text().trimmed(); m_password = password->text(); m_key.clear(); m_cache.clear();
    QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat); settings.setValue("QRZ/username", m_username);
    if (m_username.isEmpty() || m_password.isEmpty()) { showStatus(uiText("qrz_connect_help", "Connect your QRZ account for station details. Complete data requires XML access.")); return; }
    login(false);
}
void QrzLookupPanel::request(const QList<QPair<QString,QString>>& query, std::function<void(QrzResponse)> done) {
    const quint64 generation = m_generation;
    QNetworkRequest request(QUrl(QStringLiteral("https://xmldata.qrz.com/xml/current/")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setRawHeader("User-Agent", "MadModem/" MADMODEM_VERSION_STRING);
    // Credentials/session keys stay in the TLS POST body, never in URLs/logs.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QByteArray body;
    for (const auto& item : query) {
        if (!body.isEmpty()) body += '&';
        body += QUrl::toPercentEncoding(item.first) + '=' + QUrl::toPercentEncoding(item.second);
    }
    auto *reply = m_network.post(request, body); m_reply = reply;
    reply->setReadBufferSize(1024*1024);
    auto data = std::make_shared<QByteArray>();
    auto *timeout = new QTimer(reply); timeout->setSingleShot(true); timeout->start(15000);
    connect(timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::readyRead, this, [reply, data] { data->append(reply->readAll()); if (data->size()>1024*1024) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, data, generation, done] {
        const bool ok = reply->error()==QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200;
        reply->deleteLater(); if (generation != m_generation) return; m_reply = nullptr;
        if (!ok) { showStatus(uiText("qrz_network_error", "QRZ request failed. Check the connection and account, then try again.")); return; }
        data->append(reply->readAll());
        const auto result = QrzResponse::parse(*data);
        if (!result.valid) { showStatus(uiText("qrz_invalid", "QRZ returned an invalid response.")); return; }
        done(result);
    });
}
void QrzLookupPanel::login(bool retry) {
    showStatus(uiText("qrz_connecting", "Connecting to QRZ…"));
    const QList<QPair<QString,QString>> query{{"username",m_username},{"password",m_password},{"agent","MadModem-" MADMODEM_VERSION_STRING}};
    request(query, [this, retry](const QrzResponse& result) {
        m_key = result.key;
        if (m_key.isEmpty() || !result.error.isEmpty()) { showStatus(result.error.isEmpty() ? uiText("qrz_login_failed", "QRZ login failed. Check your account details.") : result.error); return; }
        if (m_call.isEmpty()) { showStatus(uiText("qrz_select", "Select a callsign in the RX text or QSO form.")+"\n"+result.message); return; }
        lookup(retry);
    });
}
void QrzLookupPanel::lookup(bool relogin) {
    if (m_call.isEmpty() || m_password.isEmpty()) return;
    if (m_key.isEmpty()) { login(relogin); return; }
    const QList<QPair<QString,QString>> query{{"s",m_key},{"callsign",m_call}};
    request(query, [this, relogin](const QrzResponse& result) {
        if (result.key.isEmpty() && !relogin) { m_key.clear(); login(true); return; }
        if (!result.key.isEmpty()) m_key = result.key;
        if (!result.error.isEmpty() || result.fields.value("call").isEmpty()) { showStatus(result.error.isEmpty() ? uiText("qrz_no_data", "No station data returned by QRZ.") : result.error); return; }
        if (m_cache.size()>=256) m_cache.clear();
        m_cache.insert(m_call, {result,QDateTime::currentDateTimeUtc()}); showResult(result);
    });
}
void QrzLookupPanel::showResult(const QrzResponse& result) {
    const auto& f = result.fields;
    QStringList lines;
    const QString registered = f.value("call").trimmed().toUpper();
    lines << "<b>" + registered.toHtmlEscaped() + "</b> · QRZ.COM";
    if (registered != m_call) lines << uiText("qrz_alias", "Requested: %1").arg(m_call).toHtmlEscaped();
    QString name = f.value("name_fmt"); if (name.isEmpty()) name=(f.value("fname")+" "+f.value("name")).trimmed();
    for (const QString& value : {name, (f.value("addr2")+" "+f.value("state")).trimmed(), f.value("country")}) if (!value.isEmpty()) lines<<value.toHtmlEscaped();
    if (!f.value("grid").isEmpty()) lines<<"Locator: "+f.value("grid").toHtmlEscaped();
    if (!f.value("qslmgr").isEmpty()) lines<<"QSL: "+f.value("qslmgr").toHtmlEscaped();
    if (!result.message.isEmpty()) lines<<result.message.toHtmlEscaped();
    m_details->setHtml(lines.join("<br>"));
    m_details->setToolTip(uiText("qrz_record_help", "QRZ database record for the selected text. This does not confirm the received decoding or the station's current operating location."));
}
