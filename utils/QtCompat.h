#pragma once

#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QTimeZone>
#endif

namespace mmqt {

inline QDateTime fromMSecsSinceEpochUtc(qint64 msecs)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QDateTime::fromMSecsSinceEpoch(msecs, QTimeZone(QTimeZone::UTC));
#else
    return QDateTime::fromMSecsSinceEpoch(msecs, Qt::UTC);
#endif
}

inline QDateTime makeUtcDateTime(const QDate &date, const QTime &time)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QDateTime(date, time, QTimeZone(QTimeZone::UTC));
#else
    return QDateTime(date, time, Qt::UTC);
#endif
}

inline void setDateTimeUtc(QDateTime &dateTime)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    dateTime.setTimeZone(QTimeZone(QTimeZone::UTC));
#else
    dateTime.setTimeSpec(Qt::UTC);
#endif
}

} // namespace mmqt
