#include "QsoUdpBroadcaster.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QHostAddress>
#include <QUdpSocket>

namespace {
constexpr quint32 kWsjtMagic = 0xadbccbdaU;
constexpr quint32 kWsjtSchema = 3U;
constexpr quint32 kWsjtLoggedAdifType = 12U;
}

QString QsoUdpBroadcaster::buildAdifFile(const LogbookEntry &entry, const QString &programVersion)
{
    QString version = programVersion.trimmed();
    if (version.isEmpty()) {
        version = QCoreApplication::applicationVersion().trimmed();
    }

    QString adif = QStringLiteral("<ADIF_VER:5>3.1.4 <PROGRAMID:8>MadModem ");
    if (!version.isEmpty()) {
        const int utf8Bytes = version.toUtf8().size();
        adif += QStringLiteral("<PROGRAMVERSION:%1>%2 ").arg(utf8Bytes).arg(version);
    }
    adif += QStringLiteral("<EOH>\n");
    adif += AdifLogbook::entryToAdif(entry);
    return adif;
}

QByteArray QsoUdpBroadcaster::buildLoggedAdifDatagram(const LogbookEntry &entry,
                                                       const QString &programVersion)
{
    QByteArray datagram;
    QDataStream stream(&datagram, QIODevice::WriteOnly);
    // WSJT-X network protocol schema 3 uses Qt 5.4 QDataStream encoding and
    // network (big-endian) byte order.
    stream.setVersion(QDataStream::Qt_5_4);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << kWsjtMagic
           << kWsjtSchema
           << kWsjtLoggedAdifType
           << QByteArrayLiteral("MadModem")
           << buildAdifFile(entry, programVersion).toUtf8();
    return datagram;
}

QsoUdpBroadcaster::SendResult QsoUdpBroadcaster::sendLoggedAdif(const LogbookEntry &entry,
                                                                 const QString &serverAddress,
                                                                 quint16 port,
                                                                 const QString &programVersion)
{
    SendResult result;
    if (port == 0) {
        result.error = QStringLiteral("invalid UDP port");
        return result;
    }

    const QString destination = serverAddress.trimmed();
    QHostAddress address;
    if (destination.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0) {
        address = QHostAddress::LocalHost;
    } else if (!address.setAddress(destination)) {
        result.error = QStringLiteral("invalid IP address: %1").arg(destination);
        return result;
    }

    const QByteArray datagram = buildLoggedAdifDatagram(entry, programVersion);
    QUdpSocket socket;
    result.bytesWritten = socket.writeDatagram(datagram, address, port);
    if (result.bytesWritten != datagram.size()) {
        result.error = socket.errorString();
        if (result.error.trimmed().isEmpty()) {
            result.error = QStringLiteral("UDP datagram was not fully written");
        }
        return result;
    }

    result.ok = true;
    return result;
}
