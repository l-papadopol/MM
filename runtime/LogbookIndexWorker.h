#ifndef LOGBOOKINDEXWORKER_H
#define LOGBOOKINDEXWORKER_H

#include "../logbook/AdifLogbook.h"
#include "../modems/rtty/contest/RttyContestRules.h"

#include <QObject>
#include <QHash>
#include <QStringList>

/**
 * @brief Background owner of fast logbook membership/contest-dupe indexes.
 *
 * The GUI and modem threads never scan the ADIF record vector for live text
 * assistance. Exceptional rebuilds receive only an ADIF file path and parse it
 * inside this worker thread; the GUI never copies the full record vector. Normal
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

signals:
    void lookupReady(quint64 requestId,
                     const QString &consumerId,
                     const QStringList &workedCalls);
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
    static void increment(QHash<QString, int> *index, const QString &key);
    static void decrement(QHash<QString, int> *index, const QString &key);

    ContestConfig m_contest;
    QHash<QString, int> m_byCall;
    QHash<QString, int> m_contestDupes;
};

#endif // LOGBOOKINDEXWORKER_H
