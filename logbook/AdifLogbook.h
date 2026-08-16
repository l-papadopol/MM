#ifndef ADIFLOGBOOK_H
#define ADIFLOGBOOK_H

#include <QDate>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief One QSO log entry stored in ADIF form.
 *
 * MadModem exposes the common fields directly, but keeps every ADIF tag found
 * in the original record.  This is important for real logbooks produced by
 * WSJT-X, MSHV, Logger32, N1MM, DXKeeper, Log4OM, etc.: unsupported fields must
 * not be destroyed just because MM does not show them in a primary column.
 */
struct LogbookEntry
{
    QString callsign;
    QString rstSent;
    QString rstReceived;
    QString band;
    QString mode;
    QString grid;
    QString comment;
    QString freq;
    QString name;
    QString qth;
    QString country;
    QString operatorCall;
    QString stationCallsign;
    QDateTime utc;
    QDateTime utcEnd;

    // Complete ADIF payload for preservation/export. Keys are upper-case ADIF
    // field names. fieldOrder preserves the original order when possible.
    QMap<QString, QString> adifFields;
    QStringList adifFieldOrder;
};

/**
 * @brief Structured search filters for the ADIF-backed logbook.
 *
 * Empty string fields are ignored.  Valid from/to dates are inclusive and are
 * evaluated against the QSO UTC date.
 */
struct LogbookSearchCriteria
{
    QString anyText;
    QString callsign;
    QString rstSent;
    QString rstReceived;
    QString band;
    QString mode;
    QString grid;
    QDate fromDateUtc;
    QDate toDateUtc;
};

/**
 * @brief ADIF-backed logbook store.
 *
 * Load/save is conservative and ADIF-preserving: the common columns are parsed
 * for UI/filtering, while all tags are kept for round-trip export.  MadModem no
 * longer rewrites a user-provided 60+ MB ADIF file on shutdown just to normalize
 * the small subset of fields it understands.
 */
class AdifLogbook
{
public:
    static QString defaultPath();

    explicit AdifLogbook(const QString &fileName = defaultPath());

    QString fileName() const;
    void setFileName(const QString &fileName);

    bool load(QString *errorMessage = nullptr);
    bool save(QString *errorMessage = nullptr) const;

    bool append(const LogbookEntry &entry, QString *errorMessage = nullptr);
    int removeEntries(const QVector<LogbookEntry> &entries, QString *errorMessage = nullptr);
    bool importAdif(const QString &fileName, int *importedCount = nullptr, QString *errorMessage = nullptr);
    bool exportAdif(const QString &fileName, QString *errorMessage = nullptr) const;
    bool exportRecordsAdif(const QString &fileName,
                           const QVector<LogbookEntry> &records,
                           QString *errorMessage = nullptr) const;

    QVector<LogbookEntry> records() const;
    QStringList allAdifFieldNames() const;
    QVector<LogbookEntry> filteredRecords(const QString &needle) const;
    QVector<LogbookEntry> filteredRecords(const LogbookSearchCriteria &criteria) const;

    bool containsCallsign(const QString &callsign) const;
    int count() const;

    static QString normalizeCallsign(const QString &callsign);
    static QString entryToAdif(const LogbookEntry &entry);
    static QVector<LogbookEntry> parseAdif(const QString &text);
    static QString recordsToAdif(const QVector<LogbookEntry> &records, const QString &headerText = QString());
    static QString extraFieldSummary(const LogbookEntry &entry, int maxFields = 12);

private:
    void rebuildCallsignIndex();

    QString m_fileName;
    QString m_headerText;
    QVector<LogbookEntry> m_records;
    QSet<QString> m_callsignIndex;
};

#endif // ADIFLOGBOOK_H
