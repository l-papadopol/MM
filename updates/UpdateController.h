#pragma once
#include <QObject>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QProgressDialog>
#include <functional>
class QMenu;
class QWidget;
class QTimer;

class UpdateController : public QObject {
public:
    UpdateController(QWidget* window, QMenu* help, std::function<bool()> idle);
    void check(bool manual = false);
    static QString newerPublishedVersion(const QJsonObject& release, const QString& current, quint64 currentBuild = 0);
    static quint64 releaseBuildNumber(const QJsonObject& release);
    static QJsonObject newestPublishedRelease(const QJsonArray& releases);
    static QString assetName(const QString& version, bool managed);
    static QStringList assetNames(const QString& version, bool managed);
    static QString linuxPlatform(const QString& marker, const QString& bundleInfo, const QString& osRelease);
    static QJsonObject selectAsset(const QJsonObject& release, const QStringList& names);
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    virtual void fetch(const QUrl& url, qint64 maximum, std::function<void(QByteArray)> done);
    void fetchReleasePage(int page);
    virtual void offer(const QJsonObject& release);
    void download(const QUrl& url, const QString& name, const QByteArray& sha, qint64 size);
    bool launch(const QString& path, const QByteArray& sha);
    void fail(const QString& message);
    QWidget* m_window;
    std::function<bool()> m_idle;
    QNetworkAccessManager m_network;
    QPointer<QProgressDialog> m_progress;
    QTimer* m_automaticCheck = nullptr;
    QString m_version, m_directory;
    quint64 m_releaseBuild = 0;
    QJsonObject m_newestRelease;
    bool m_busy = false, m_manual = false, m_cancelled = false, m_managed = false;
};
