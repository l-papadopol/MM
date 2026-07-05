// -*- Mode: C++ -*-
#ifndef MADMODEM_DECODIUM_NTP_CLIENT_HPP__
#define MADMODEM_DECODIUM_NTP_CLIENT_HPP__

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QHostInfo>
#include <QVector>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDateTime>

/**
 * @brief Small Decodium/Raptor-derived asynchronous NTP client for FT8 timing.
 *
 * This is an adapted MadModem port of Decodium's Network/NtpClient.  It queries
 * several NTP servers over UDP, filters slow replies, computes a robust median
 * offset, and falls back to HTTP Date headers when UDP/123 is blocked.  It does
 * not change the operating-system clock; it only reports timing diagnostics.
 */
class NtpClient : public QObject
{
    Q_OBJECT

public:
    explicit NtpClient(QObject *parent = nullptr);
    ~NtpClient() override;

    void setEnabled(bool enabled);
    void syncNow();
    void setCustomServer(const QString &server);
    void setRefreshInterval(int ms);
    void setMaxRtt(double ms);
    void setInitialOffset(double offsetMs);

    double offsetMs() const { return m_offsetMs; }
    bool isSynced() const { return m_synced; }
    int lastServerCount() const { return m_lastServerCount; }
    double lastRttMs() const { return m_lastRttMs; }
    QDateTime lastSyncUtc() const { return m_lastSyncUtc; }
    QString lastStatusText() const { return m_lastStatusText; }

signals:
    void offsetUpdated(double offsetMs);
    void syncStatusChanged(bool synced, const QString &statusText);
    void errorOccurred(const QString &errorMsg);

private slots:
    void onRefreshTimeout();
    void onQueryTimeout();
    void onReadyRead();
    void onDnsLookupResult(QHostInfo hostInfo);
    void onHttpReply(QNetworkReply *reply);

private:
    static constexpr quint64 NTP_EPOCH_OFFSET = 2208988800ULL;
    static constexpr int NTP_PACKET_SIZE = 48;
    static constexpr int NTP_PORT = 123;
    static constexpr int REFRESH_INTERVAL_MS = 60000;
    static constexpr int REFRESH_RETRY_MS = 10000;
    static constexpr int QUERY_TIMEOUT_MS = 2500;
    static constexpr double MAX_OFFSET_MS = 3600000.0;
    static constexpr int SERVERS_PER_QUERY = 4;

    struct PendingQuery
    {
        qint64 t1Ms = 0;
        QString serverName;
    };

    struct OffsetSample
    {
        double offsetMs = 0.0;
        double rttMs = 0.0;
        QString source;
    };

    void launchNextServerBatch();
    void sendQuery(const QHostAddress &address, const QString &serverName);
    void finalizeNtpSamples();
    void httpTimeFallback();
    void setStatus(bool synced, const QString &text);
    static double ntpTimestampToMs(quint32 seconds, quint32 fraction);
    static void msToNtpTimestamp(qint64 msEpoch, quint32 &seconds, quint32 &fraction);

    QUdpSocket m_socket;
    QTimer m_refreshTimer;
    QTimer m_queryTimeoutTimer;

    QStringList m_servers;
    QStringList m_queryOrder;
    int m_nextServerIndex = 0;
    QHash<QString, PendingQuery> m_pendingQueries;
    QVector<OffsetSample> m_collectedOffsets;

    QNetworkAccessManager *m_nam = nullptr;
    QVector<OffsetSample> m_httpOffsets;
    QHash<QNetworkReply *, qint64> m_httpSendTimes;
    int m_httpPendingCount = 0;
    int m_ntpConsecutiveFailures = 0;

    double m_offsetMs = 0.0;
    bool m_synced = false;
    int m_lastServerCount = 0;
    double m_lastRttMs = 0.0;
    QDateTime m_lastSyncUtc;
    QString m_lastStatusText = QStringLiteral("NTP: idle");

    bool m_enabled = false;
    int m_pendingDnsLookups = 0;
    QString m_customServer;
    int m_refreshIntervalMs = REFRESH_INTERVAL_MS;
    double m_maxRttMs = 1000.0;
};

#endif // MADMODEM_DECODIUM_NTP_CLIENT_HPP__
