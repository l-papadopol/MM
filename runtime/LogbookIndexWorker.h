#ifndef LOGBOOKINDEXWORKER_H
#define LOGBOOKINDEXWORKER_H

#include "../logbook/AdifLogbook.h"
#include "../modems/rtty/contest/RttyContestRules.h"

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QStringList>
#include <QVariantList>

/**
 * @brief Background owner of fast logbook membership/contest-dupe indexes.
 *
 * The GUI and modem threads never scan the ADIF record vector for live text
 * assistance. Rebuilds receive committed, implicitly shared record snapshots;
 * subsequent incremental updates cannot race a newer file revision. Normal
 * QSO updates are incremental. All hot-path lookups are O(1) hash operations.
 */
class LogbookIndexWorker final : public QObject
{
    Q_OBJECT
public:
    struct ContestConfig {
        bool active = false;
        QString mode;
        QString sessionId;
        QString ruleId;
        QString cabrilloId;
        QString sessionAdifKey;
        QString ruleAdifKey;
        QString dupeScope;
        QList<RttyContestPeriodRule> periods;
    };

    explicit LogbookIndexWorker(QObject *parent = nullptr);

    void rebuildFromRecords(const QVector<LogbookEntry> &records, const ContestConfig &contest);
    void setContestConfigFromRecords(const QVector<LogbookEntry> &records, const ContestConfig &contest);
    void rebuildFromFile(const QString &fileName, const ContestConfig &contest);
    void setContestConfigFromFile(const QString &fileName, const ContestConfig &contest);
    void addEntry(const LogbookEntry &entry);
    void removeEntry(const LogbookEntry &entry);
    void lookup(quint64 requestId,
                const QString &consumerId,
                const QStringList &calls,
                bool contestScoped,
                const QString &band,
                const QString &periodId);

    /**
     * FT worked/needed lookup. Each query is a QVariantMap containing cacheKey,
     * call, band, mode, dxcc, grid4 and optional recentHours /
     * recentBandModeMinutes. Results preserve cacheKey and are served only from
     * worker-owned hash indexes; no ADIF scan occurs here.
     */
    void lookupFt(quint64 requestId,
                  const QString &consumerId,
                  const QVariantList &queries);

signals:
    void lookupReady(quint64 requestId,
                     const QString &consumerId,
                     const QStringList &workedCalls);
    void ftLookupReady(quint64 requestId,
                       const QString &consumerId,
                       const QVariantList &results);
    void indexReady(int recordCount, int uniqueCalls, int contestKeys);
    void indexError(const QString &message);

private:
    QString contestPeriodId(const QDateTime &utc) const;
    bool contestEntryMatches(const LogbookEntry &entry) const;
    QString contestKey(const QString &call,
                       const QString &band,
                       const QString &periodId) const;
    QString contestKeyForEntry(const LogbookEntry &entry) const;
    static QString normalizedBand(const QString &band);
    static QString normalizedMode(const QString &mode);
    static QString grid4(const QString &grid);
    static QString compoundKey(const QString &a, const QString &b);
    static QString compoundKey(const QString &a, const QString &b, const QString &c);
    QString dxccForEntry(const LogbookEntry &entry) const;
    void indexEntry(const LogbookEntry &entry);
    void unindexEntry(const LogbookEntry &entry);
    static void increment(QHash<QString, int> *index, const QString &key);
    static void decrement(QHash<QString, int> *index, const QString &key);

    ContestConfig m_contest;
    QHash<QString, int> m_byCall;
    QHash<QString, int> m_contestDupes;

    // FT lookup indexes. These live only in the worker thread.
    QHash<QString, QDateTime> m_latestByCall;
    QHash<QString, QDateTime> m_latestByCallBandMode;
    QHash<QString, int> m_byDxcc;
    QHash<QString, int> m_byDxccBand;
    QHash<QString, int> m_byDxccMode;
    QHash<QString, int> m_byGrid4;
    QHash<QString, int> m_byGridBand;
    QHash<QString, int> m_byGridMode;
};

#endif // LOGBOOKINDEXWORKER_H
