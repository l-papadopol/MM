#pragma once
#include <QObject>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QProgressDialog>
#include <functional>
class QMenu;
class QWidget;

class UpdateController : public QObject {
public:
    UpdateController(QWidget* window, QMenu* help, std::function<bool()> idle);
    void check(bool manual = false);
    static QString newerStableVersion(const QJsonObject& release, const QString& current);
    static QString assetName(const QString& version, bool managed);
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    void fetch(const QUrl& url, qint64 maximum, std::function<void(QByteArray)> done);
    void offer(const QJsonObject& release);
    void download(const QUrl& url, const QString& name, const QByteArray& sha, qint64 size);
    bool launch(const QString& path, const QByteArray& sha);
    void fail(const QString& message);
    QWidget* m_window;
    std::function<bool()> m_idle;
    QNetworkAccessManager m_network;
    QPointer<QProgressDialog> m_progress;
    QString m_version, m_directory;
    bool m_busy = false, m_manual = false, m_cancelled = false, m_managed = false;
};
