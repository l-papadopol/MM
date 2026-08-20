#ifndef QSOUDPBROADCASTER_H
#define QSOUDPBROADCASTER_H

#include "../logbook/AdifLogbook.h"

#include <QByteArray>
#include <QString>
#include <QtGlobal>

/**
 * @brief Sends one completed QSO to an external logger using the WSJT-X/JTDX
 *        UDP Logged ADIF message (message type 12, schema 3).
 *
 * This class is deliberately stateless.  The ADIF logbook remains the owner of
 * QSO persistence; UDP is a best-effort notification performed only after the
 * local append succeeds.
 */
class QsoUdpBroadcaster
{
public:
    struct SendResult
    {
        bool ok = false;
        qint64 bytesWritten = -1;
        QString error;
    };

    static QByteArray buildLoggedAdifDatagram(const LogbookEntry &entry,
                                               const QString &programVersion = QString());

    static SendResult sendLoggedAdif(const LogbookEntry &entry,
                                     const QString &serverAddress,
                                     quint16 port,
                                     const QString &programVersion = QString());

private:
    static QString buildAdifFile(const LogbookEntry &entry, const QString &programVersion);
};

#endif // QSOUDPBROADCASTER_H
