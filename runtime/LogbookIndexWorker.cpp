#include "LogbookIndexWorker.h"

#include <QSet>

LogbookIndexWorker::LogbookIndexWorker(QObject *parent)
    : QObject(parent)
{
}

QString LogbookIndexWorker::normalizedBand(const QString &band)
{
    return band.trimmed().toLower();
}

void LogbookIndexWorker::increment(QHash<QString, int> *index, const QString &key)
{
    if (index == nullptr || key.isEmpty()) return;
    (*index)[key] = index->value(key, 0) + 1;
}

void LogbookIndexWorker::decrement(QHash<QString, int> *index, const QString &key)
{
    if (index == nullptr || key.isEmpty()) return;
    const int next = index->value(key, 0) - 1;
    if (next <= 0) index->remove(key);
    else (*index)[key] = next;
}

QString LogbookIndexWorker::contestPeriodId(const QDateTime &utc) const
{
    if (!utc.isValid()) return QString();
    const QDateTime instant = utc.toUTC();
    for (const RttyContestPeriodRule &period : m_contest.periods) {
        QDateTime start = QDateTime::fromString(period.startUtc, Qt::ISODate);
        QDateTime end = QDateTime::fromString(period.endUtc, Qt::ISODate);
        if (!start.isValid() || !end.isValid()) continue;
        start = start.toUTC();
        end = end.toUTC();
        if (instant >= start && instant <= end) return period.id;
    }
    return QString();
}

bool LogbookIndexWorker::contestEntryMatches(const LogbookEntry &entry) const
{
    if (!m_contest.active || m_contest.sessionId.isEmpty()) return false;
    if (!m_contest.mode.isEmpty() && entry.mode.compare(m_contest.mode, Qt::CaseInsensitive) != 0) return false;
    if (!m_contest.sessionAdifKey.isEmpty() &&
        entry.adifFields.value(m_contest.sessionAdifKey).trimmed() != m_contest.sessionId) return false;

    const QString rule = m_contest.ruleAdifKey.isEmpty()
        ? QString()
        : entry.adifFields.value(m_contest.ruleAdifKey).trimmed().toLower();
    const QString cabrillo = entry.adifFields.value(QStringLiteral("CONTEST_ID")).trimmed().toUpper();
    const bool ruleMatches = !m_contest.ruleId.isEmpty() && rule == m_contest.ruleId.toLower();
    const bool cabrilloMatches = !m_contest.cabrilloId.isEmpty() && cabrillo == m_contest.cabrilloId.toUpper();
    return ruleMatches || cabrilloMatches;
}

QString LogbookIndexWorker::contestKey(const QString &call,
                                       const QString &band,
                                       const QString &periodId) const
{
    QString key = AdifLogbook::normalizeCallsign(call);
    if (key.isEmpty()) return QString();
    const QString scope = m_contest.dupeScope.trimmed().toLower();
    if (scope == QStringLiteral("band")) {
        const QString b = normalizedBand(band);
        if (b.isEmpty()) return QString();
        key += QStringLiteral("|") + b;
    } else if (scope == QStringLiteral("period")) {
        if (periodId.isEmpty()) return QString();
        key += QStringLiteral("|") + periodId;
    } else if (scope == QStringLiteral("band_period")) {
        const QString b = normalizedBand(band);
        if (b.isEmpty() || periodId.isEmpty()) return QString();
        key += QStringLiteral("|") + b + QStringLiteral("|") + periodId;
    }
    return key;
}

QString LogbookIndexWorker::contestKeyForEntry(const LogbookEntry &entry) const
{
    return contestKey(entry.callsign, entry.band, contestPeriodId(entry.utc));
}

void LogbookIndexWorker::rebuildFromFile(const QString &fileName,
                                             const ContestConfig &contest)
{
    AdifLogbook logbook(fileName);
    QString error;
    if (!logbook.load(&error)) {
        m_contest = contest;
        m_byCall.clear();
        m_contestDupes.clear();
        emit indexError(error);
        emit indexReady(0, 0, 0);
        return;
    }

    const QVector<LogbookEntry> records = logbook.records();
    m_contest = contest;
    m_byCall.clear();
    m_contestDupes.clear();
    m_byCall.reserve(records.size());
    if (m_contest.active) m_contestDupes.reserve(records.size());

    for (const LogbookEntry &entry : records) {
        increment(&m_byCall, AdifLogbook::normalizeCallsign(entry.callsign));
        if (contestEntryMatches(entry)) increment(&m_contestDupes, contestKeyForEntry(entry));
    }
    emit indexReady(records.size(), m_byCall.size(), m_contestDupes.size());
}

void LogbookIndexWorker::setContestConfigFromFile(const QString &fileName,
                                                   const ContestConfig &contest)
{
    // Contest/rule/session changes are exceptional. Re-read the ADIF in this
    // worker thread and rebuild only the contest index; the GUI never copies
    // the logbook record vector.
    AdifLogbook logbook(fileName);
    QString error;
    if (!logbook.load(&error)) {
        m_contest = contest;
        m_contestDupes.clear();
        emit indexError(error);
        emit indexReady(0, m_byCall.size(), 0);
        return;
    }

    const QVector<LogbookEntry> records = logbook.records();
    m_contest = contest;
    m_contestDupes.clear();
    if (m_contest.active) {
        m_contestDupes.reserve(records.size());
        for (const LogbookEntry &entry : records) {
            if (contestEntryMatches(entry)) increment(&m_contestDupes, contestKeyForEntry(entry));
        }
    }
    emit indexReady(records.size(), m_byCall.size(), m_contestDupes.size());
}

void LogbookIndexWorker::addEntry(const LogbookEntry &entry)
{
    increment(&m_byCall, AdifLogbook::normalizeCallsign(entry.callsign));
    if (contestEntryMatches(entry)) increment(&m_contestDupes, contestKeyForEntry(entry));
}

void LogbookIndexWorker::removeEntry(const LogbookEntry &entry)
{
    decrement(&m_byCall, AdifLogbook::normalizeCallsign(entry.callsign));
    if (contestEntryMatches(entry)) decrement(&m_contestDupes, contestKeyForEntry(entry));
}

void LogbookIndexWorker::lookup(quint64 requestId,
                                const QString &consumerId,
                                const QStringList &calls,
                                bool contestScoped,
                                const QString &band,
                                const QString &periodId)
{
    QStringList worked;
    worked.reserve(calls.size());
    QSet<QString> emitted;
    for (const QString &rawCall : calls) {
        const QString call = AdifLogbook::normalizeCallsign(rawCall);
        if (call.isEmpty() || emitted.contains(call)) continue;
        const bool present = contestScoped && m_contest.active
            ? m_contestDupes.contains(contestKey(call, band, periodId))
            : m_byCall.contains(call);
        if (present) {
            worked.push_back(call);
            emitted.insert(call);
        }
    }
    emit lookupReady(requestId, consumerId, worked);
}
