#include "QrzLookupPanel.h"
#include "../MadModemVersion.h"
#include "../settings/AppSettings.h"
#include "../utils/RuntimeI18n.h"
#include <QNetworkReply>
#include <QTextBrowser>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QtMath>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
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
    m_details->setOpenLinks(false);
    m_details->setMinimumHeight(100);
    m_details->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_details, 1);
    auto *buttons = new QHBoxLayout;
    m_open = new QPushButton(uiText("text.open_qrz_page", "Open QRZ page"), this);
    m_open->setProperty("i18nSourceText", "Open QRZ page");
    buttons->addWidget(m_open); layout->addLayout(buttons);
    connect(m_open, &QPushButton::clicked, this, [this] {
        if (!m_call.isEmpty()) QDesktopServices::openUrl(QUrl(QStringLiteral("https://www.qrz.com/db/") + QString::fromLatin1(QUrl::toPercentEncoding(m_call))));
    });
    m_delay.setSingleShot(true); m_delay.setInterval(450);
    connect(&m_delay, &QTimer::timeout, this, [this] { lookup(); });
    QSettings settings(AppSettings::settingsFilePath(), QSettings::IniFormat);
    m_username = settings.value("QRZ/username").toString().trimmed();
    m_password = settings.value("QRZ/password").toString();
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
    if (m_username.isEmpty() || m_password.isEmpty()) { showStatus(uiText("qrz_settings_help", "Configure your QRZ account in Settings → QRZ.com. Complete data requires XML access.")); return; }
    showStatus(uiText("qrz_loading", "Looking up station…")); m_delay.start();
}
void QrzLookupPanel::showStatus(const QString& status) {
    m_details->setHtml((m_call.isEmpty() ? QString() : "<b>"+m_call.toHtmlEscaped()+"</b><br>") + status.toHtmlEscaped().replace("\n", "<br>"));
}
void QrzLookupPanel::setAccountCredentials(const QString& username, const QString& password) {
    const QString normalized = username.trimmed();
    if (normalized == m_username && password == m_password) return;
    cancel(); m_username = normalized; m_password = password; m_key.clear(); m_cache.clear();
    const QString selected = m_call;
    m_call.clear();
    setCallsign(selected);
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
    if (m_call.isEmpty() || m_username.isEmpty() || m_password.isEmpty()) return;
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
