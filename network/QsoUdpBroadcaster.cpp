#include "QsoUdpBroadcaster.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QHostAddress>
#include <QUdpSocket>

namespace {
constexpr quint32 kWsjtMagic = 0xadbccbdaU;
constexpr quint32 kWsjtSchema = 3U;
constexpr quint32 kWsjtHeartbeatType = 0U;
constexpr quint32 kWsjtQsoLoggedType = 5U;
constexpr quint32 kWsjtLoggedAdifType = 12U;

void configureStream(QDataStream &stream)
{
    stream.setVersion(QDataStream::Qt_5_4);
    stream.setByteOrder(QDataStream::BigEndian);
}

void writeHeader(QDataStream &stream, quint32 type)
{
    stream << kWsjtMagic
           << kWsjtSchema
           << type
           << QByteArrayLiteral("MadModem");
}

bool resolveAddress(const QString &serverAddress, QHostAddress *address, QString *error)
{
    if (address == nullptr) return false;
    const QString destination = serverAddress.trimmed();
    if (destination.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0) {
        *address = QHostAddress::LocalHost;
        return true;
    }
    if (!address->setAddress(destination)) {
        if (error != nullptr) *error = QStringLiteral("invalid IP address: %1").arg(destination);
        return false;
    }
    return true;
}

QString exchangeField(const LogbookEntry &entry, const QString &key)
{
    return entry.adifFields.value(key).trimmed().toUpper();
}
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
    configureStream(stream);
    writeHeader(stream, kWsjtLoggedAdifType);
    stream << buildAdifFile(entry, programVersion).toUtf8();
    return datagram;
}

QByteArray QsoUdpBroadcaster::buildHeartbeatDatagram(const QString &programVersion,
                                                      const QString &revision)
{
    QByteArray datagram;
    QDataStream stream(&datagram, QIODevice::WriteOnly);
    configureStream(stream);
    writeHeader(stream, kWsjtHeartbeatType);
    stream << kWsjtSchema
           << programVersion.trimmed().toUtf8()
           << revision.trimmed().toUtf8();
    return datagram;
}

QByteArray QsoUdpBroadcaster::buildQsoLoggedDatagram(const LogbookEntry &entry,
                                                      const SendContext &context)
{
    QByteArray datagram;
    QDataStream stream(&datagram, QIODevice::WriteOnly);
    configureStream(stream);
    writeHeader(stream, kWsjtQsoLoggedType);

    const QDateTime on = entry.utc.isValid() ? entry.utc.toUTC() : QDateTime::currentDateTimeUtc();
    const QDateTime off = entry.utcEnd.isValid() ? entry.utcEnd.toUTC() : on;
    const QString operatorCall = !entry.operatorCall.trimmed().isEmpty()
        ? entry.operatorCall.trimmed().toUpper()
        : context.operatorCall.trimmed().toUpper();
    const QString myCall = !entry.stationCallsign.trimmed().isEmpty()
        ? entry.stationCallsign.trimmed().toUpper()
        : context.myCall.trimmed().toUpper();

    // WSJT-X/JTDX schema-3 QSO Logged (type 5). Empty values are legitimate
    // protocol values when MadModem does not own that item (power/name/etc.).
    stream << off
           << entry.callsign.trimmed().toUpper().toUtf8()
           << entry.grid.trimmed().toUpper().toUtf8()
           << context.txFrequencyHz
           << entry.mode.trimmed().toUpper().toUtf8()
           << entry.rstSent.trimmed().toUpper().toUtf8()
           << entry.rstReceived.trimmed().toUpper().toUtf8()
           << QByteArray()                         // Tx power
           << entry.comment.trimmed().toUtf8()
           << entry.name.trimmed().toUtf8()
           << on
           << operatorCall.toUtf8()
           << myCall.toUtf8()
           << context.myGrid.trimmed().toUpper().toUtf8()
           << exchangeField(entry, QStringLiteral("STX_STRING")).toUtf8()
           << exchangeField(entry, QStringLiteral("SRX_STRING")).toUtf8()
           << exchangeField(entry, QStringLiteral("PROP_MODE")).toUtf8();
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

    QHostAddress address;
    if (!resolveAddress(serverAddress, &address, &result.error)) return result;

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

QsoUdpBroadcaster::SendResult QsoUdpBroadcaster::sendQsoLoggedBundle(
    const LogbookEntry &entry,
    const SendContext &context,
    const QString &serverAddress,
    quint16 port,
    const QString &programVersion)
{
    SendResult result;
    if (port == 0) {
        result.error = QStringLiteral("invalid UDP port");
        return result;
    }

    QHostAddress address;
    if (!resolveAddress(serverAddress, &address, &result.error)) return result;

    const QList<QByteArray> datagrams = {
        buildHeartbeatDatagram(programVersion, context.revision),
        buildQsoLoggedDatagram(entry, context),
        buildLoggedAdifDatagram(entry, programVersion)
    };

    QUdpSocket socket;
    qint64 total = 0;
    for (const QByteArray &datagram : datagrams) {
        const qint64 written = socket.writeDatagram(datagram, address, port);
        if (written != datagram.size()) {
            result.bytesWritten = total + qMax<qint64>(qint64{0}, written);
            result.error = socket.errorString();
            if (result.error.trimmed().isEmpty()) {
                result.error = QStringLiteral("UDP datagram was not fully written");
            }
            return result;
        }
        total += written;
    }

    result.ok = true;
    result.bytesWritten = total;
    return result;
}
