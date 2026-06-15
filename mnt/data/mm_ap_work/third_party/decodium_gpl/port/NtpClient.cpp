// -*- Mode: C++ -*-
#include "NtpClient.hpp"

#include <QDateTime>
#include <QAbstractSocket>
#include <QNetworkDatagram>
#include <QNetworkRequest>
#include <QUrl>
#include <QtEndian>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <random>
#include <QLocale>

NtpClient::NtpClient(QObject *parent)
    : QObject(parent)
    , m_servers({
          QStringLiteral("0.pool.ntp.org"),
          QStringLiteral("1.pool.ntp.org"),
          QStringLiteral("2.pool.ntp.org"),
          QStringLiteral("3.pool.ntp.org"),
          QStringLiteral("0.europe.pool.ntp.org"),
          QStringLiteral("1.europe.pool.ntp.org"),
          QStringLiteral("2.europe.pool.ntp.org"),
          QStringLiteral("3.europe.pool.ntp.org"),
          QStringLiteral("time.google.com"),
          QStringLiteral("time1.google.com"),
          QStringLiteral("time.cloudflare.com"),
          QStringLiteral("time.windows.com"),
          // v1.18: numeric IPv4 fallbacks avoid broken IPv6 preference and
          // DNS-only failures on some consumer networks.
          QStringLiteral("162.159.200.123"),
          QStringLiteral("216.239.35.0"),
          QStringLiteral("216.239.35.4"),
          QStringLiteral("216.239.35.8"),
          QStringLiteral("216.239.35.12"),
          QStringLiteral("ntp1.inrim.it"),
          QStringLiteral("ntp2.inrim.it"),
          QStringLiteral("ptbtime1.ptb.de"),
          QStringLiteral("ptbtime2.ptb.de"),
          QStringLiteral("ntp.ubuntu.com")
      })
{
    m_refreshTimer.setInterval(REFRESH_INTERVAL_MS);
    connect(&m_refreshTimer, &QTimer::timeout, this, &NtpClient::onRefreshTimeout);

    m_queryTimeoutTimer.setInterval(QUERY_TIMEOUT_MS);
    m_queryTimeoutTimer.setSingleShot(true);
    connect(&m_queryTimeoutTimer, &QTimer::timeout, this, &NtpClient::onQueryTimeout);

    connect(&m_socket, &QUdpSocket::readyRead, this, &NtpClient::onReadyRead);

    m_nam = new QNetworkAccessManager(this);
    connect(m_nam, &QNetworkAccessManager::finished, this, &NtpClient::onHttpReply);
}

NtpClient::~NtpClient()
{
    m_refreshTimer.stop();
    m_queryTimeoutTimer.stop();
    m_socket.close();
}

void NtpClient::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;
    if (enabled) {
        if (m_socket.state() == QAbstractSocket::UnconnectedState) {
            // Prefer IPv4 for public NTP pools: many consumer networks expose
            // IPv6 DNS records without working IPv6 UDP/123 routing.  If IPv4
            // bind fails, fall back to Qt's generic dual-stack behaviour.
            if (!m_socket.bind(QHostAddress::AnyIPv4, 0, QAbstractSocket::ShareAddress)) {
                m_socket.bind(QHostAddress::Any, 0, QAbstractSocket::ShareAddress);
            }
        }
        syncNow();
        m_refreshTimer.start(m_refreshIntervalMs);
    } else {
        m_refreshTimer.stop();
        m_queryTimeoutTimer.stop();
        m_pendingQueries.clear();
        m_socket.close();
        setStatus(false, QStringLiteral("NTP: disabled"));
    }
}

void NtpClient::setInitialOffset(double offsetMs)
{
    m_offsetMs = offsetMs;
}

void NtpClient::setCustomServer(const QString &server)
{
    m_customServer = server.trimmed();
}

void NtpClient::setRefreshInterval(int ms)
{
    m_refreshIntervalMs = qBound(10000, ms, 300000);
    if (m_refreshTimer.isActive()) {
        m_refreshTimer.start(m_refreshIntervalMs);
    }
}

void NtpClient::setMaxRtt(double ms)
{
    m_maxRttMs = qBound(10.0, ms, 2500.0);
}

void NtpClient::syncNow()
{
    if (!m_enabled) {
        return;
    }

    m_queryTimeoutTimer.stop();
    m_pendingQueries.clear();
    m_collectedOffsets.clear();
    m_pendingDnsLookups = 0;
    m_nextServerIndex = 0;

    m_queryOrder = m_servers;
    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(m_queryOrder.begin(), m_queryOrder.end(), rng);

    if (!m_customServer.isEmpty()) {
        m_queryOrder.removeAll(m_customServer);
        m_queryOrder.prepend(m_customServer);
    }
    m_queryOrder.removeDuplicates();

    if (m_queryOrder.isEmpty()) {
        setStatus(false, QStringLiteral("NTP: no servers configured"));
        return;
    }

    launchNextServerBatch();
}

void NtpClient::launchNextServerBatch()
{
    if (!m_enabled) {
        return;
    }

    m_pendingQueries.clear();
    m_pendingDnsLookups = 0;

    const int start = m_nextServerIndex;
    const int end = qMin(m_queryOrder.size(), start + SERVERS_PER_QUERY);
    if (start >= end) {
        finalizeNtpSamples();
        return;
    }

    for (int i = start; i < end; ++i) {
        ++m_pendingDnsLookups;
        QHostInfo::lookupHost(m_queryOrder.at(i), this, SLOT(onDnsLookupResult(QHostInfo)));
    }
    m_nextServerIndex = end;

    setStatus(m_synced, QStringLiteral("NTP: querying IPv4 servers %1-%2 of %3")
                         .arg(start + 1)
                         .arg(end)
                         .arg(m_queryOrder.size()));
}

void NtpClient::onDnsLookupResult(QHostInfo hostInfo)
{
    --m_pendingDnsLookups;

    if (hostInfo.error() != QHostInfo::NoError) {
        emit errorOccurred(QStringLiteral("DNS lookup failed for %1: %2")
                               .arg(hostInfo.hostName(), hostInfo.errorString()));
        if (m_pendingDnsLookups <= 0 && m_pendingQueries.isEmpty() && !m_queryTimeoutTimer.isActive()) {
            onQueryTimeout();
        }
        return;
    }

    QList<QHostAddress> addresses = hostInfo.addresses();
    if (!addresses.isEmpty()) {
        // v1.18: force IPv4 for UDP NTP.  Several desktop networks return
        // IPv6 records even when outbound IPv6/UDP 123 is not actually usable.
        // Do not send IPv6 NTP packets here; if a hostname has no IPv4 result,
        // let the next server in the fallback list try.
        std::sort(addresses.begin(), addresses.end(), [](const QHostAddress &a, const QHostAddress &b) {
            const bool a4 = (a.protocol() == QAbstractSocket::IPv4Protocol);
            const bool b4 = (b.protocol() == QAbstractSocket::IPv4Protocol);
            return a4 && !b4;
        });
        int sent = 0;
        for (const QHostAddress &address : addresses) {
            if (address.isNull() || address.protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }
            if (sent >= 4) {
                break;
            }
            sendQuery(address, hostInfo.hostName());
            ++sent;
        }
        if (sent > 0 && !m_queryTimeoutTimer.isActive()) {
            m_queryTimeoutTimer.start();
        } else if (sent == 0 && m_pendingDnsLookups <= 0 && m_pendingQueries.isEmpty() && !m_queryTimeoutTimer.isActive()) {
            onQueryTimeout();
        }
    } else if (m_pendingDnsLookups <= 0 && m_pendingQueries.isEmpty() && !m_queryTimeoutTimer.isActive()) {
        onQueryTimeout();
    }
}

void NtpClient::sendQuery(const QHostAddress &address, const QString &serverName)
{
    QByteArray packet(NTP_PACKET_SIZE, '\0');
    packet[0] = 0x23; // LI=0, VN=4, Mode=3 client

    const qint64 t1Ms = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    quint32 t1Sec = 0;
    quint32 t1Frac = 0;
    msToNtpTimestamp(t1Ms, t1Sec, t1Frac);
    // Client transmit timestamp lives at bytes 40..47.  Some legacy code wrote
    // this into Originate; for a client query, Transmit is the correct field.
    qToBigEndian(t1Sec, reinterpret_cast<uchar *>(packet.data() + 40));
    qToBigEndian(t1Frac, reinterpret_cast<uchar *>(packet.data() + 44));

    PendingQuery pq;
    pq.t1Ms = t1Ms;
    pq.serverName = serverName;
    m_pendingQueries.insert(address.toString(), pq);

    if (m_socket.writeDatagram(packet, address, NTP_PORT) < 0) {
        m_pendingQueries.remove(address.toString());
    }
}

void NtpClient::onReadyRead()
{
    while (m_socket.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket.receiveDatagram();
        const QByteArray data = datagram.data();
        if (data.size() < NTP_PACKET_SIZE) {
            continue;
        }

        const QString senderKey = datagram.senderAddress().toString();
        if (!m_pendingQueries.contains(senderKey)) {
            continue;
        }

        const PendingQuery pq = m_pendingQueries.value(senderKey);
        m_pendingQueries.remove(senderKey);

        const qint64 t4Ms = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
        const quint8 stratum = static_cast<quint8>(data.at(1));
        if (stratum == 0 || stratum > 15) {
            continue;
        }

        const quint32 t2Sec = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + 32));
        const quint32 t2Frac = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + 36));
        const quint32 t3Sec = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + 40));
        const quint32 t3Frac = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + 44));

        const double t2Ms = ntpTimestampToMs(t2Sec, t2Frac);
        const double t3Ms = ntpTimestampToMs(t3Sec, t3Frac);
        const double offset = ((t2Ms - static_cast<double>(pq.t1Ms)) + (t3Ms - static_cast<double>(t4Ms))) / 2.0;
        const double rtt = (static_cast<double>(t4Ms - pq.t1Ms)) - (t3Ms - t2Ms);

        if (rtt < 0.0 || rtt > m_maxRttMs) {
            continue;
        }
        if (std::abs(offset) > MAX_OFFSET_MS) {
            continue;
        }

        OffsetSample sample;
        sample.offsetMs = offset;
        sample.rttMs = rtt;
        sample.source = pq.serverName.isEmpty() ? senderKey : pq.serverName;
        m_collectedOffsets.append(sample);

        if (m_pendingQueries.isEmpty()) {
            m_queryTimeoutTimer.stop();
            finalizeNtpSamples();
        }
    }
}

void NtpClient::onRefreshTimeout()
{
    syncNow();
}

void NtpClient::onQueryTimeout()
{
    m_queryTimeoutTimer.stop();
    m_pendingQueries.clear();

    // True fallback over the configured NTP list: if the current UDP batch
    // times out, immediately try the next servers instead of giving up after
    // the first random group. Only after the full list fails do we go to HTTP.
    if (m_collectedOffsets.isEmpty() && m_nextServerIndex < m_queryOrder.size()) {
        setStatus(m_synced, QStringLiteral("NTP: no IPv4 reply from this batch; trying next servers"));
        launchNextServerBatch();
        return;
    }

    finalizeNtpSamples();
}

void NtpClient::finalizeNtpSamples()
{
    if (m_collectedOffsets.isEmpty()) {
        ++m_ntpConsecutiveFailures;
        if (!m_synced) {
            setStatus(false, QStringLiteral("NTP UDP IPv4: no accepted reply after all configured servers"));
        }
        httpTimeFallback();
        m_refreshTimer.setInterval(REFRESH_RETRY_MS);
        return;
    }

    m_ntpConsecutiveFailures = 0;
    std::sort(m_collectedOffsets.begin(), m_collectedOffsets.end(), [](const OffsetSample &a, const OffsetSample &b) {
        return a.offsetMs < b.offsetMs;
    });

    QVector<OffsetSample> filtered = m_collectedOffsets;
    int n = filtered.size();
    if (n >= 4) {
        const double q1 = filtered.at(n / 4).offsetMs;
        const double q3 = filtered.at((3 * n) / 4).offsetMs;
        const double iqr = q3 - q1;
        const double lo = q1 - 1.5 * iqr;
        const double hi = q3 + 1.5 * iqr;
        QVector<OffsetSample> keep;
        for (const OffsetSample &sample : filtered) {
            if (sample.offsetMs >= lo && sample.offsetMs <= hi) {
                keep.append(sample);
            }
        }
        if (!keep.isEmpty()) {
            filtered = keep;
        }
        n = filtered.size();
    }

    std::sort(filtered.begin(), filtered.end(), [](const OffsetSample &a, const OffsetSample &b) {
        return a.offsetMs < b.offsetMs;
    });

    const double median = (n % 2 == 0)
        ? ((filtered.at(n / 2 - 1).offsetMs + filtered.at(n / 2).offsetMs) / 2.0)
        : filtered.at(n / 2).offsetMs;

    OffsetSample best = filtered.first();
    for (const OffsetSample &sample : filtered) {
        if (std::abs(sample.offsetMs - median) < std::abs(best.offsetMs - median)) {
            best = sample;
        }
    }

    m_lastServerCount = n;
    m_lastRttMs = best.rttMs;
    m_lastSyncUtc = QDateTime::currentDateTimeUtc();
    if (m_synced) {
        m_offsetMs = (m_offsetMs * 0.5) + (median * 0.5);
    } else {
        m_offsetMs = median;
    }
    m_synced = true;
    m_refreshTimer.setInterval(m_refreshIntervalMs);

    emit offsetUpdated(m_offsetMs);
    setStatus(true, QStringLiteral("NTP locked: %1 servers, offset %2 ms, RTT %3 ms")
                    .arg(n)
                    .arg(median, 0, 'f', 1)
                    .arg(best.rttMs, 0, 'f', 0));
}

void NtpClient::httpTimeFallback()
{
    if (!m_enabled || m_nam == nullptr) {
        return;
    }

    static const QStringList httpServers = {
        QStringLiteral("http://www.google.com/generate_204"),
        QStringLiteral("http://www.cloudflare.com"),
        QStringLiteral("http://www.microsoft.com"),
        QStringLiteral("http://www.apple.com")
    };

    m_httpOffsets.clear();
    m_httpPendingCount = httpServers.size();

    for (const QString &url : httpServers) {
        QNetworkRequest req{QUrl(url)};
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
#endif
        QNetworkReply *reply = m_nam->head(req);
        m_httpSendTimes.insert(reply, QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
        QTimer::singleShot(5000, reply, [reply]() {
            if (reply->isRunning()) {
                reply->abort();
            }
        });
    }
}

void NtpClient::onHttpReply(QNetworkReply *reply)
{
    reply->deleteLater();
    --m_httpPendingCount;

    const qint64 t4Ms = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    if (reply->error() == QNetworkReply::NoError && m_httpSendTimes.contains(reply)) {
        const qint64 t1Ms = m_httpSendTimes.value(reply);
        m_httpSendTimes.remove(reply);
        const QByteArray dateHeader = reply->rawHeader("Date");
        if (!dateHeader.isEmpty()) {
            QDateTime serverTime = QLocale::c().toDateTime(QString::fromLatin1(dateHeader).trimmed(),
                                                          QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'"));
            serverTime.setTimeSpec(Qt::UTC);
            if (serverTime.isValid()) {
                const qint64 serverMs = serverTime.toMSecsSinceEpoch();
                const qint64 rtt = t4Ms - t1Ms;
                const double offset = static_cast<double>(serverMs) - static_cast<double>(t1Ms + rtt / 2);
                if (std::abs(offset) < MAX_OFFSET_MS) {
                    OffsetSample sample;
                    sample.offsetMs = offset;
                    sample.rttMs = static_cast<double>(rtt);
                    sample.source = reply->url().host();
                    m_httpOffsets.append(sample);
                }
            }
        }
    } else {
        m_httpSendTimes.remove(reply);
    }

    if (m_httpPendingCount <= 0) {
        if (!m_httpOffsets.isEmpty()) {
            std::sort(m_httpOffsets.begin(), m_httpOffsets.end(), [](const OffsetSample &a, const OffsetSample &b) {
                return a.offsetMs < b.offsetMs;
            });
            const int n = m_httpOffsets.size();
            const double median = (n % 2 == 0)
                ? ((m_httpOffsets.at(n / 2 - 1).offsetMs + m_httpOffsets.at(n / 2).offsetMs) / 2.0)
                : m_httpOffsets.at(n / 2).offsetMs;
            m_offsetMs = median;
            m_synced = true;
            m_lastServerCount = n;
            m_lastRttMs = m_httpOffsets.at(n / 2).rttMs;
            m_lastSyncUtc = QDateTime::currentDateTimeUtc();
            emit offsetUpdated(m_offsetMs);
            setStatus(true, QStringLiteral("HTTP time fallback: %1 servers, offset %2 ms")
                            .arg(n)
                            .arg(median, 0, 'f', 0));
        } else if (!m_synced) {
            setStatus(false, QStringLiteral("NTP/HTTP time: no response"));
        }
        m_httpSendTimes.clear();
    }
}

void NtpClient::setStatus(bool synced, const QString &text)
{
    m_synced = synced;
    m_lastStatusText = text;
    emit syncStatusChanged(synced, text);
}

double NtpClient::ntpTimestampToMs(quint32 seconds, quint32 fraction)
{
    const qint64 unixSeconds = static_cast<qint64>(seconds) - static_cast<qint64>(NTP_EPOCH_OFFSET);
    const double fracMs = static_cast<double>(fraction) * 1000.0 / 4294967296.0;
    return static_cast<double>(unixSeconds) * 1000.0 + fracMs;
}

void NtpClient::msToNtpTimestamp(qint64 msEpoch, quint32 &seconds, quint32 &fraction)
{
    const qint64 sec = msEpoch / 1000;
    const qint64 ms = msEpoch % 1000;
    seconds = static_cast<quint32>(sec + static_cast<qint64>(NTP_EPOCH_OFFSET));
    fraction = static_cast<quint32>(static_cast<double>(ms) / 1000.0 * 4294967296.0);
}
