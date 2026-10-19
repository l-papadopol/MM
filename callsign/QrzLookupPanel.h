#pragma once
#include <QGroupBox>
#include <QMap>
#include <QHash>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QDateTime>
#include <QPair>
#include <QList>
#include <functional>
class QNetworkReply;
class QTextBrowser;
class QPushButton;
struct QrzResponse {
    QMap<QString, QString> fields;
    QString key, error, message;
    bool valid = false;
    static QrzResponse parse(const QByteArray& xml);
};
// Explicitly selected stations only; never used as a decoder validation gate.
class QrzLookupPanel : public QGroupBox {
public:
    explicit QrzLookupPanel(QWidget* parent = nullptr);
    void setCallsign(const QString& call);
    void setAccountCredentials(const QString& username, const QString& password);
private:
    void lookup(bool relogin = false);
    void login(bool retry);
    void request(const QList<QPair<QString,QString>>& query, std::function<void(QrzResponse)> done);
    void showResult(const QrzResponse& result);
    void showStatus(const QString& status);
    void cancel();
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_delay;
    QTextBrowser* m_details;
    QPushButton* m_open;
    QString m_call, m_username, m_password, m_key;
    quint64 m_generation = 0;
    struct Cached { QrzResponse response; QDateTime time; };
    QHash<QString, Cached> m_cache;
};
