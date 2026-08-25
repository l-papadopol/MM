#ifndef QSOUDPBROADCASTER_H
#define QSOUDPBROADCASTER_H

#include "../logbook/AdifLogbook.h"

#include <QByteArray>
#include <QString>
#include <QtGlobal>

/**
 * @brief Sends completed QSOs to external loggers using the WSJT-X/JTDX
 *        schema-3 UDP protocol. The established Logged ADIF message remains
 *        available; CW/RTTY can additionally send QSO Logged plus heartbeat.
 *
 * This class is deliberately stateless.  The ADIF logbook remains the owner of
 * QSO persistence; UDP is a best-effort notification performed only after the
 * local append succeeds.
 */
class QsoUdpBroadcaster
{
public:
    struct SendContext
    {
        QString myCall;
        QString myGrid;
        quint64 txFrequencyHz = 0;
        QString operatorCall;
        QString revision;
    };

    struct SendResult
    {
        bool ok = false;
        qint64 bytesWritten = -1;
        QString error;
    };

    static QByteArray buildLoggedAdifDatagram(const LogbookEntry &entry,
                                               const QString &programVersion = QString());
    static QByteArray buildHeartbeatDatagram(const QString &programVersion,
                                             const QString &revision = QString());
    static QByteArray buildQsoLoggedDatagram(const LogbookEntry &entry,
                                             const SendContext &context);

    static SendResult sendLoggedAdif(const LogbookEntry &entry,
                                     const QString &serverAddress,
                                     quint16 port,
                                     const QString &programVersion = QString());

    // Contest/text logger compatibility bundle. WSJT-X/JTDX send both the
    // structured QSO Logged (type 5) and Logged ADIF (type 12) notifications;
    // many contest loggers listen primarily for type 5 while general ADIF
    // consumers often prefer type 12. A heartbeat precedes the pair so servers
    // that require client/schema discovery accept the event immediately.
    static SendResult sendQsoLoggedBundle(const LogbookEntry &entry,
                                          const SendContext &context,
                                          const QString &serverAddress,
                                          quint16 port,
                                          const QString &programVersion = QString());

private:
    static QString buildAdifFile(const LogbookEntry &entry, const QString &programVersion);
};

#endif // QSOUDPBROADCASTER_H
