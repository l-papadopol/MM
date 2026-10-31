#include "LogbookIndexWorker.h"
#include "../dxcc/CtyCountryFile.h"

#include <QSet>
#include <QVariantMap>

LogbookIndexWorker::LogbookIndexWorker(QObject *parent)
    : QObject(parent)
{
}

QString LogbookIndexWorker::normalizedBand(const QString &band)
{
    return band.trimmed().toLower();
}

QString LogbookIndexWorker::normalizedMode(const QString &mode)
{
    return mode.trimmed().toUpper();
}

QString LogbookIndexWorker::grid4(const QString &grid)
{
    const QString g = grid.trimmed().toUpper();
    if (g.size() < 4) return QString();
    const QString four = g.left(4);
    if (four.at(0) < QLatin1Char('A') || four.at(0) > QLatin1Char('R') ||
        four.at(1) < QLatin1Char('A') || four.at(1) > QLatin1Char('R') ||
        !four.at(2).isDigit() || !four.at(3).isDigit()) return QString();
    return four;
}

QString LogbookIndexWorker::compoundKey(const QString &a, const QString &b)
{
    if (a.isEmpty() || b.isEmpty()) return QString();
    return a + QLatin1Char('|') + b;
}

QString LogbookIndexWorker::compoundKey(const QString &a, const QString &b, const QString &c)
{
    if (a.isEmpty() || b.isEmpty() || c.isEmpty()) return QString();
    return a + QLatin1Char('|') + b + QLatin1Char('|') + c;
}

QString LogbookIndexWorker::dxccForEntry(const LogbookEntry &entry) const
{
    QString dxcc = entry.adifFields.value(QStringLiteral("DXCC")).trimmed();
    if (!dxcc.isEmpty()) return dxcc;
    if (entry.callsign.trimmed().isEmpty()) return QString();
    const CtyCountryFile::LookupResult cty = CtyCountryFile::instance().lookupCallsign(entry.callsign);
    return cty.valid ? cty.entity.dxcc.trimmed() : QString();
}

void LogbookIndexWorker::indexEntry(const LogbookEntry &entry)
{
    const QString call = AdifLogbook::normalizeCallsign(entry.callsign);
    const QString band = normalizedBand(entry.band);
    const QString mode = normalizedMode(entry.mode);
    const QDateTime utc = entry.utc.isValid() ? entry.utc.toUTC() : QDateTime();
    increment(&m_byCall, call);
    if (!call.isEmpty() && utc.isValid()) {
        if (!m_latestByCall.contains(call) || m_latestByCall.value(call) < utc) m_latestByCall.insert(call, utc);
        const QString cbm = compoundKey(call, band, mode);
        if (!cbm.isEmpty() && (!m_latestByCallBandMode.contains(cbm) || m_latestByCallBandMode.value(cbm) < utc))
            m_latestByCallBandMode.insert(cbm, utc);
    }

    const QString dxcc = dxccForEntry(entry);
    if (!dxcc.isEmpty()) {
        increment(&m_byDxcc, dxcc);
        increment(&m_byDxccBand, compoundKey(dxcc, band));
        increment(&m_byDxccMode, compoundKey(dxcc, mode));
    }

    const QString g4 = grid4(entry.grid);
    if (!g4.isEmpty()) {
        increment(&m_byGrid4, g4);
        increment(&m_byGridBand, compoundKey(g4, band));
        increment(&m_byGridMode, compoundKey(g4, mode));
    }
}

void LogbookIndexWorker::unindexEntry(const LogbookEntry &entry)
{
    const QString call = AdifLogbook::normalizeCallsign(entry.callsign);
    const QString band = normalizedBand(entry.band);
    const QString mode = normalizedMode(entry.mode);
    decrement(&m_byCall, call);

    const QString dxcc = dxccForEntry(entry);
    if (!dxcc.isEmpty()) {
        decrement(&m_byDxcc, dxcc);
        decrement(&m_byDxccBand, compoundKey(dxcc, band));
        decrement(&m_byDxccMode, compoundKey(dxcc, mode));
    }
    const QString g4 = grid4(entry.grid);
    if (!g4.isEmpty()) {
        decrement(&m_byGrid4, g4);
        decrement(&m_byGridBand, compoundKey(g4, band));
        decrement(&m_byGridMode, compoundKey(g4, mode));
    }

    // Deletions/import rewrites are exceptional and MainWindow schedules a
    // background rebuild afterwards. Do not guess a second-latest timestamp.
    const QDateTime utc = entry.utc.isValid() ? entry.utc.toUTC() : QDateTime();
    if (!call.isEmpty() && utc.isValid() && m_latestByCall.value(call) == utc) m_latestByCall.remove(call);
    const QString cbm = compoundKey(call, band, mode);
    if (!cbm.isEmpty() && utc.isValid() && m_latestByCallBandMode.value(cbm) == utc) m_latestByCallBandMode.remove(cbm);
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
        m_latestByCall.clear();
        m_latestByCallBandMode.clear();
        m_byDxcc.clear();
        m_byDxccBand.clear();
        m_byDxccMode.clear();
        m_byGrid4.clear();
        m_byGridBand.clear();
        m_byGridMode.clear();
        emit indexError(error);
        emit indexReady(0, 0, 0);
        return;
    }

    rebuildFromRecords(logbook.records(), contest);
}

void LogbookIndexWorker::rebuildFromRecords(const QVector<LogbookEntry> &records, const ContestConfig &contest)
{
    m_contest = contest;
    m_byCall.clear();
    m_contestDupes.clear();
    m_latestByCall.clear();
    m_latestByCallBandMode.clear();
    m_byDxcc.clear();
    m_byDxccBand.clear();
    m_byDxccMode.clear();
    m_byGrid4.clear();
    m_byGridBand.clear();
    m_byGridMode.clear();
    m_byCall.reserve(records.size());
    if (m_contest.active) m_contestDupes.reserve(records.size());

    for (const LogbookEntry &entry : records) {
        indexEntry(entry);
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

    setContestConfigFromRecords(logbook.records(), contest);
}

void LogbookIndexWorker::setContestConfigFromRecords(const QVector<LogbookEntry> &records, const ContestConfig &contest)
{
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
    indexEntry(entry);
    if (contestEntryMatches(entry)) increment(&m_contestDupes, contestKeyForEntry(entry));
}

void LogbookIndexWorker::removeEntry(const LogbookEntry &entry)
{
    unindexEntry(entry);
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

void LogbookIndexWorker::lookupFt(quint64 requestId,
                                  const QString &consumerId,
                                  const QVariantList &queries)
{
    QVariantList results;
    results.reserve(queries.size());
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (const QVariant &item : queries) {
        const QVariantMap query = item.toMap();
        QVariantMap result;
        result.insert(QStringLiteral("cacheKey"), query.value(QStringLiteral("cacheKey")));
        result.insert(QStringLiteral("generation"), query.value(QStringLiteral("generation")));

        const QString call = AdifLogbook::normalizeCallsign(query.value(QStringLiteral("call")).toString());
        const QString band = normalizedBand(query.value(QStringLiteral("band")).toString());
        const QString mode = normalizedMode(query.value(QStringLiteral("mode")).toString());
        const QString dxcc = query.value(QStringLiteral("dxcc")).toString().trimmed();
        const QString g4 = grid4(query.value(QStringLiteral("grid4")).toString());
        const int recentHours = qBound(0, query.value(QStringLiteral("recentHours")).toInt(), 24 * 365);
        const int recentBandModeMinutes = qBound(0, query.value(QStringLiteral("recentBandModeMinutes")).toInt(), 24 * 60 * 30);
        const qint64 referenceUtcMs = query.value(QStringLiteral("referenceUtcMs")).toLongLong();
        const QDateTime referenceUtc = referenceUtcMs > 0
            ? QDateTime::fromMSecsSinceEpoch(referenceUtcMs, Qt::UTC)
            : now;

        result.insert(QStringLiteral("call"), call);
        result.insert(QStringLiteral("worked"), !call.isEmpty() && m_byCall.contains(call));

        bool recentWorked = false;
        if (!call.isEmpty() && recentHours > 0) {
            const QDateTime latest = m_latestByCall.value(call);
            recentWorked = latest.isValid() && qAbs(latest.secsTo(referenceUtc)) <= static_cast<qint64>(recentHours) * 3600;
        }
        result.insert(QStringLiteral("recentWorked"), recentWorked);
        result.insert(QStringLiteral("latestCallUtc"), m_latestByCall.value(call));
        result.insert(QStringLiteral("latestBandModeUtc"), m_latestByCallBandMode.value(compoundKey(call, band, mode)));

        bool recentBandMode = false;
        if (!call.isEmpty() && recentBandModeMinutes > 0) {
            const QDateTime latest = m_latestByCallBandMode.value(compoundKey(call, band, mode));
            recentBandMode = latest.isValid() && qAbs(latest.secsTo(referenceUtc)) <= static_cast<qint64>(recentBandModeMinutes) * 60;
        }
        result.insert(QStringLiteral("recentBandMode"), recentBandMode);

        result.insert(QStringLiteral("countryWorkedAny"), !dxcc.isEmpty() && m_byDxcc.contains(dxcc));
        result.insert(QStringLiteral("countryWorkedBand"), !dxcc.isEmpty() && m_byDxccBand.contains(compoundKey(dxcc, band)));
        result.insert(QStringLiteral("countryWorkedMode"), !dxcc.isEmpty() && m_byDxccMode.contains(compoundKey(dxcc, mode)));
        result.insert(QStringLiteral("gridWorkedAny"), !g4.isEmpty() && m_byGrid4.contains(g4));
        result.insert(QStringLiteral("gridWorkedBand"), !g4.isEmpty() && m_byGridBand.contains(compoundKey(g4, band)));
        result.insert(QStringLiteral("gridWorkedMode"), !g4.isEmpty() && m_byGridMode.contains(compoundKey(g4, mode)));
        results.push_back(result);
    }
    emit ftLookupReady(requestId, consumerId, results);
}

