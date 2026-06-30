#include "AdifLogbook.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>
#include <QTime>
#include <QTimeZone>
#include "utils/QtCompat.h"

namespace {

QString cleanAdifValue(QString value)
{
    value.replace('\r', ' ');
    value.replace('\n', ' ');
    return value.simplified();
}

QString fieldKey(QString name)
{
    return name.trimmed().toUpper();
}

void putField(LogbookEntry *entry, const QString &name, const QString &value)
{
    if (entry == nullptr) {
        return;
    }
    const QString key = fieldKey(name);
    if (key.isEmpty()) {
        return;
    }
    if (!entry->adifFields.contains(key)) {
        entry->adifFieldOrder.append(key);
    }
    entry->adifFields.insert(key, cleanAdifValue(value));
}

QString adifTag(const QString &name, const QString &value)
{
    const QString cleaned = cleanAdifValue(value);
    if (cleaned.isEmpty()) {
        return QString();
    }

    // ADIF field lengths are byte counts, not QString character counts.  Use
    // UTF-8 byte size so non-ASCII names/comments round-trip correctly.
    return QString("<%1:%2>%3 ")
        .arg(fieldKey(name))
        .arg(cleaned.toUtf8().size())
        .arg(cleaned);
}

QString valueFromMap(const LogbookEntry &entry, const QString &name)
{
    return entry.adifFields.value(fieldKey(name)).trimmed();
}

QString firstValue(const LogbookEntry &entry, std::initializer_list<const char *> names)
{
    for (const char *name : names) {
        const QString v = valueFromMap(entry, QString::fromLatin1(name));
        if (!v.isEmpty()) {
            return v;
        }
    }
    return QString();
}

QDateTime utcFromAdif(const QString &date, const QString &time)
{
    if (date.size() != 8) {
        return QDateTime();
    }

    QString paddedTime = time;
    while (paddedTime.size() < 6) {
        paddedTime.append('0');
    }
    paddedTime = paddedTime.left(6);

    const QDate qdate = QDate::fromString(date, "yyyyMMdd");
    const QTime qtime = QTime::fromString(paddedTime, "hhmmss");
    if (!qdate.isValid() || !qtime.isValid()) {
        return QDateTime();
    }

    return mmqt::makeUtcDateTime(qdate, qtime);
}

int indexOfTagAscii(const QByteArray &bytes, const QByteArray &tag, int from = 0)
{
    return bytes.toLower().indexOf(tag.toLower(), from);
}

int indexOfEohBytes(const QByteArray &bytes)
{
    return indexOfTagAscii(bytes, QByteArrayLiteral("<eoh>"));
}

struct ParsedRecord
{
    QMap<QString, QString> fields;
    QStringList order;
};

ParsedRecord parseRecordFieldsBytes(const QByteArray &record)
{
    ParsedRecord parsed;
    int i = 0;
    const int n = record.size();
    while (i < n) {
        const int lt = record.indexOf('<', i);
        if (lt < 0 || lt + 1 >= n) {
            break;
        }
        const int gt = record.indexOf('>', lt + 1);
        if (gt < 0) {
            break;
        }

        const QByteArray descriptorBytes = record.mid(lt + 1, gt - lt - 1).trimmed();
        const QList<QByteArray> parts = descriptorBytes.split(':');
        if (parts.isEmpty()) {
            i = gt + 1;
            continue;
        }

        const QString name = fieldKey(QString::fromLatin1(parts.at(0)));
        if (name == QStringLiteral("EOR") || name == QStringLiteral("EOH")) {
            i = gt + 1;
            continue;
        }
        if (parts.size() < 2) {
            i = gt + 1;
            continue;
        }

        bool ok = false;
        const int byteLength = QString::fromLatin1(parts.at(1)).toInt(&ok);
        if (!ok || byteLength < 0) {
            i = gt + 1;
            continue;
        }

        const int valueStart = gt + 1;
        if (valueStart > n) {
            break;
        }
        const QByteArray valueBytes = record.mid(valueStart, qMin(byteLength, n - valueStart));
        const QString value = QString::fromUtf8(valueBytes).trimmed();
        if (!parsed.fields.contains(name)) {
            parsed.order.append(name);
        }
        parsed.fields.insert(name, value);
        i = valueStart + byteLength;
    }
    return parsed;
}

QVector<QByteArray> splitAdifRecordsBytes(const QByteArray &body)
{
    QVector<QByteArray> result;
    const QByteArray lower = body.toLower();
    int start = 0;
    while (true) {
        const int eor = lower.indexOf(QByteArrayLiteral("<eor>"), start);
        if (eor < 0) {
            break;
        }
        const QByteArray rec = body.mid(start, eor - start);
        if (!rec.trimmed().isEmpty()) {
            result.append(rec);
        }
        start = eor + 5;
    }
    const QByteArray tail = body.mid(start);
    if (!tail.trimmed().isEmpty()) {
        result.append(tail);
    }
    return result;
}

void syncCommonFieldsToAdif(LogbookEntry *entry)
{
    if (entry == nullptr) {
        return;
    }

    if (!entry->callsign.trimmed().isEmpty()) putField(entry, "CALL", AdifLogbook::normalizeCallsign(entry->callsign));
    if (!entry->rstSent.trimmed().isEmpty()) putField(entry, "RST_SENT", entry->rstSent.trimmed().toUpper());
    if (!entry->rstReceived.trimmed().isEmpty()) putField(entry, "RST_RCVD", entry->rstReceived.trimmed().toUpper());
    if (!entry->band.trimmed().isEmpty()) putField(entry, "BAND", entry->band.trimmed().toLower());
    if (!entry->mode.trimmed().isEmpty()) putField(entry, "MODE", entry->mode.trimmed().toUpper());
    if (!entry->grid.trimmed().isEmpty()) putField(entry, "GRIDSQUARE", entry->grid.trimmed().toUpper());
    if (!entry->comment.trimmed().isEmpty()) putField(entry, "COMMENT", entry->comment.trimmed());
    if (!entry->freq.trimmed().isEmpty()) putField(entry, "FREQ", entry->freq.trimmed());
    if (!entry->name.trimmed().isEmpty()) putField(entry, "NAME", entry->name.trimmed());
    if (!entry->qth.trimmed().isEmpty()) putField(entry, "QTH", entry->qth.trimmed());
    if (!entry->country.trimmed().isEmpty()) putField(entry, "COUNTRY", entry->country.trimmed());
    if (!entry->operatorCall.trimmed().isEmpty()) putField(entry, "OPERATOR", entry->operatorCall.trimmed().toUpper());
    if (!entry->stationCallsign.trimmed().isEmpty()) putField(entry, "STATION_CALLSIGN", entry->stationCallsign.trimmed().toUpper());

    const QDateTime utc = entry->utc.isValid() ? entry->utc.toUTC() : QDateTime::currentDateTimeUtc();
    putField(entry, "QSO_DATE", utc.date().toString("yyyyMMdd"));
    putField(entry, "TIME_ON", utc.time().toString("hhmmss"));
    if (entry->utcEnd.isValid()) {
        const QDateTime endUtc = entry->utcEnd.toUTC();
        putField(entry, "QSO_DATE_OFF", endUtc.date().toString("yyyyMMdd"));
        putField(entry, "TIME_OFF", endUtc.time().toString("hhmmss"));
    }
}

LogbookEntry entryFromParsed(const ParsedRecord &parsed)
{
    LogbookEntry entry;
    entry.adifFields = parsed.fields;
    entry.adifFieldOrder = parsed.order;

    entry.callsign = AdifLogbook::normalizeCallsign(valueFromMap(entry, "CALL"));
    entry.rstSent = firstValue(entry, {"RST_SENT", "SRX_STRING"});
    entry.rstReceived = firstValue(entry, {"RST_RCVD", "STX_STRING"});
    entry.band = valueFromMap(entry, "BAND");
    entry.mode = valueFromMap(entry, "MODE").toUpper();
    entry.grid = firstValue(entry, {"GRIDSQUARE", "VUCC_GRIDS"}).toUpper();
    entry.comment = firstValue(entry, {"COMMENT", "NOTES"});
    entry.freq = valueFromMap(entry, "FREQ");
    entry.name = valueFromMap(entry, "NAME");
    entry.qth = valueFromMap(entry, "QTH");
    entry.country = valueFromMap(entry, "COUNTRY");
    entry.operatorCall = valueFromMap(entry, "OPERATOR").toUpper();
    entry.stationCallsign = valueFromMap(entry, "STATION_CALLSIGN").toUpper();
    entry.utc = utcFromAdif(valueFromMap(entry, "QSO_DATE"), valueFromMap(entry, "TIME_ON"));
    entry.utcEnd = utcFromAdif(valueFromMap(entry, "QSO_DATE_OFF"), valueFromMap(entry, "TIME_OFF"));

    return entry;
}

QVector<LogbookEntry> parseAdifBytes(const QByteArray &bytes, QString *headerText = nullptr)
{
    QVector<LogbookEntry> result;
    const int eoh = indexOfEohBytes(bytes);
    if (headerText != nullptr) {
        if (eoh >= 0) {
            *headerText = QString::fromUtf8(bytes.left(eoh + 5)).trimmed() + QStringLiteral("\n");
        } else {
            *headerText = QStringLiteral("Generated by MadModem\n<PROGRAMID:8>MadModem <EOH>\n");
        }
    }

    const QByteArray body = (eoh >= 0) ? bytes.mid(eoh + 5) : bytes;
    const QVector<QByteArray> records = splitAdifRecordsBytes(body);
    for (const QByteArray &rawRecord : records) {
        const ParsedRecord parsed = parseRecordFieldsBytes(rawRecord);
        if (parsed.fields.isEmpty()) {
            continue;
        }
        // Preserve records even when CALL is missing.  They may be non-standard
        // or ancillary ADIF records; dropping them would break round-trip safety.
        result.append(entryFromParsed(parsed));
    }
    return result;
}

QByteArray headerBytesForFile(const QString &headerText)
{
    const QString header = headerText.trimmed().isEmpty()
                           ? QStringLiteral("Generated by MadModem\n<PROGRAMID:8>MadModem <EOH>\n")
                           : (headerText.endsWith('\n') ? headerText : headerText + '\n');
    return header.toUtf8();
}

} // namespace

QString AdifLogbook::defaultPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath("logbook.adi");
}

AdifLogbook::AdifLogbook(const QString &fileName)
    : m_fileName(fileName)
{
}

QString AdifLogbook::fileName() const
{
    return m_fileName;
}

void AdifLogbook::setFileName(const QString &fileName)
{
    m_fileName = fileName;
}

bool AdifLogbook::load(QString *errorMessage)
{
    QFile file(m_fileName);
    if (!file.exists()) {
        m_headerText = QStringLiteral("Generated by MadModem\n<PROGRAMID:8>MadModem <EOH>\n");
        m_records.clear();
        rebuildCallsignIndex();
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    const QByteArray bytes = file.readAll();
    m_records = parseAdifBytes(bytes, &m_headerText);
    rebuildCallsignIndex();
    return true;
}

bool AdifLogbook::save(QString *errorMessage) const
{
    QDir dir(QFileInfo(m_fileName).absolutePath());
    if (!dir.exists() && !dir.mkpath(".")) {
        if (errorMessage != nullptr) {
            *errorMessage = "Cannot create logbook directory.";
        }
        return false;
    }

    QSaveFile file(m_fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    file.write(headerBytesForFile(m_headerText));
    for (const LogbookEntry &entry : m_records) {
        file.write(entryToAdif(entry).toUtf8());
        file.write("\n");
    }

    if (file.error() != QFile::NoError) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    if (!file.commit()) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    return true;
}

bool AdifLogbook::append(const LogbookEntry &entry, QString *errorMessage)
{
    if (normalizeCallsign(entry.callsign).isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Callsign is empty.";
        }
        return false;
    }

    LogbookEntry normalized = entry;
    normalized.callsign = normalizeCallsign(entry.callsign);
    if (!normalized.utc.isValid()) {
        normalized.utc = QDateTime::currentDateTimeUtc();
    }
    normalized.utc = normalized.utc.toUTC();
    syncCommonFieldsToAdif(&normalized);

    m_records.append(normalized);
    rebuildCallsignIndex();

    // QSO logging must be append-only for normal operation.  Rewriting a 60+ MB
    // ADIF file just because one QSO was added is slow and, if a parser bug or
    // crash occurs, dangerous.  Full rewrites are reserved for explicit export,
    // import-merge, or delete operations.
    QFileInfo info(m_fileName);
    QDir dir(info.absolutePath());
    if (!dir.exists() && !dir.mkpath(".")) {
        if (errorMessage != nullptr) {
            *errorMessage = "Cannot create logbook directory.";
        }
        return false;
    }

    const bool needsHeader = !info.exists() || info.size() == 0;
    QFile file(m_fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    if (needsHeader) {
        file.write(headerBytesForFile(m_headerText));
    } else {
        file.write("\n");
    }
    file.write(entryToAdif(normalized).toUtf8());
    file.write("\n");
    if (file.error() != QFile::NoError) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    return true;
}

namespace {

bool sameLogbookEntryForDelete(const LogbookEntry &a, const LogbookEntry &b)
{
    return AdifLogbook::normalizeCallsign(a.callsign) == AdifLogbook::normalizeCallsign(b.callsign) &&
           a.band.trimmed().compare(b.band.trimmed(), Qt::CaseInsensitive) == 0 &&
           a.mode.trimmed().compare(b.mode.trimmed(), Qt::CaseInsensitive) == 0 &&
           a.rstSent.trimmed().compare(b.rstSent.trimmed(), Qt::CaseInsensitive) == 0 &&
           a.rstReceived.trimmed().compare(b.rstReceived.trimmed(), Qt::CaseInsensitive) == 0 &&
           a.grid.trimmed().compare(b.grid.trimmed(), Qt::CaseInsensitive) == 0 &&
           a.utc.toUTC() == b.utc.toUTC();
}

} // namespace

int AdifLogbook::removeEntries(const QVector<LogbookEntry> &entries, QString *errorMessage)
{
    if (entries.isEmpty()) {
        return 0;
    }

    int removed = 0;
    QVector<LogbookEntry> remaining = m_records;
    for (const LogbookEntry &wanted : entries) {
        for (int i = 0; i < remaining.size(); ++i) {
            if (sameLogbookEntryForDelete(remaining.at(i), wanted)) {
                remaining.removeAt(i);
                ++removed;
                break;
            }
        }
    }

    if (removed <= 0) {
        return 0;
    }

    m_records = remaining;
    rebuildCallsignIndex();
    if (!save(errorMessage)) {
        return -1;
    }
    return removed;
}

bool AdifLogbook::importAdif(const QString &fileName, int *importedCount, QString *errorMessage)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    const QVector<LogbookEntry> imported = parseAdifBytes(file.readAll());
    int added = 0;
    for (const LogbookEntry &entry : imported) {
        m_records.append(entry);
        if (!normalizeCallsign(entry.callsign).isEmpty()) {
            ++added;
        }
    }

    if (importedCount != nullptr) {
        *importedCount = added;
    }

    rebuildCallsignIndex();
    return save(errorMessage);
}

bool AdifLogbook::exportAdif(const QString &fileName, QString *errorMessage) const
{
    return exportRecordsAdif(fileName, m_records, errorMessage);
}

bool AdifLogbook::exportRecordsAdif(const QString &fileName,
                                    const QVector<LogbookEntry> &records,
                                    QString *errorMessage) const
{
    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    file.write(recordsToAdif(records, "Exported by MadModem").toUtf8());
    if (file.error() != QFile::NoError) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    if (!file.commit()) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    return true;
}

QVector<LogbookEntry> AdifLogbook::records() const
{
    return m_records;
}

QStringList AdifLogbook::allAdifFieldNames() const
{
    QStringList fields;
    QSet<QString> seen;
    for (const LogbookEntry &entry : m_records) {
        QStringList ordered = entry.adifFieldOrder;
        for (auto it = entry.adifFields.constBegin(); it != entry.adifFields.constEnd(); ++it) {
            if (!ordered.contains(it.key())) {
                ordered.append(it.key());
            }
        }
        for (const QString &key : ordered) {
            const QString normalized = fieldKey(key);
            if (!normalized.isEmpty() && !seen.contains(normalized)) {
                seen.insert(normalized);
                fields.append(normalized);
            }
        }
    }
    return fields;
}

QVector<LogbookEntry> AdifLogbook::filteredRecords(const QString &needle) const
{
    LogbookSearchCriteria criteria;
    criteria.anyText = needle;
    return filteredRecords(criteria);
}

QVector<LogbookEntry> AdifLogbook::filteredRecords(const LogbookSearchCriteria &criteria) const
{
    const QString any = criteria.anyText.trimmed().toUpper();
    const QString call = normalizeCallsign(criteria.callsign);
    const QString rstSent = criteria.rstSent.trimmed().toUpper();
    const QString rstReceived = criteria.rstReceived.trimmed().toUpper();
    const QString band = criteria.band.trimmed().toUpper();
    const QString mode = criteria.mode.trimmed().toUpper();
    const QString grid = criteria.grid.trimmed().toUpper();

    QVector<LogbookEntry> result;
    for (const LogbookEntry &entry : m_records) {
        const QDate qsoDate = entry.utc.toUTC().date();

        if (!criteria.fromDateUtc.isNull() && criteria.fromDateUtc.isValid() && qsoDate < criteria.fromDateUtc) {
            continue;
        }
        if (!criteria.toDateUtc.isNull() && criteria.toDateUtc.isValid() && qsoDate > criteria.toDateUtc) {
            continue;
        }

        const QString entryCall = normalizeCallsign(entry.callsign);
        const QString entryBand = entry.band.toUpper();
        const QString entryMode = entry.mode.toUpper();
        const QString entryGrid = entry.grid.toUpper();
        const QString entryComment = entry.comment.toUpper();
        const QString entryRstSent = entry.rstSent.toUpper();
        const QString entryRstReceived = entry.rstReceived.toUpper();
        const QString entryUtc = entry.utc.toUTC().toString(Qt::ISODate).toUpper();
        const QString extra = extraFieldSummary(entry, 1000).toUpper();

        if (!call.isEmpty() && !entryCall.contains(call)) continue;
        if (!rstSent.isEmpty() && !entryRstSent.contains(rstSent)) continue;
        if (!rstReceived.isEmpty() && !entryRstReceived.contains(rstReceived)) continue;
        if (!band.isEmpty() && !entryBand.contains(band)) continue;
        if (!mode.isEmpty() && !entryMode.contains(mode)) continue;
        if (!grid.isEmpty() && !entryGrid.contains(grid)) continue;

        if (!any.isEmpty()) {
            const bool anyMatch = entryCall.contains(any) ||
                                  entryBand.contains(any) ||
                                  entryMode.contains(any) ||
                                  entryGrid.contains(any) ||
                                  entryComment.contains(any) ||
                                  entryRstSent.contains(any) ||
                                  entryRstReceived.contains(any) ||
                                  entryUtc.contains(any) ||
                                  entry.freq.toUpper().contains(any) ||
                                  entry.name.toUpper().contains(any) ||
                                  entry.qth.toUpper().contains(any) ||
                                  entry.country.toUpper().contains(any) ||
                                  entry.operatorCall.toUpper().contains(any) ||
                                  entry.stationCallsign.toUpper().contains(any) ||
                                  extra.contains(any);
            if (!anyMatch) {
                continue;
            }
        }

        result.append(entry);
    }
    return result;
}

bool AdifLogbook::containsCallsign(const QString &callsign) const
{
    return m_callsignIndex.contains(normalizeCallsign(callsign));
}

int AdifLogbook::count() const
{
    return m_records.size();
}

QString AdifLogbook::normalizeCallsign(const QString &callsign)
{
    QString normalized = callsign.trimmed().toUpper();
    normalized.remove(QRegularExpression("[^A-Z0-9/]"));
    return normalized;
}

QString AdifLogbook::entryToAdif(const LogbookEntry &entry)
{
    LogbookEntry copy = entry;
    if (!copy.utc.isValid()) {
        copy.utc = QDateTime::currentDateTimeUtc();
    }
    syncCommonFieldsToAdif(&copy);

    QString line;
    QSet<QString> emitted;
    auto emitField = [&](const QString &key) {
        const QString k = fieldKey(key);
        if (k.isEmpty() || emitted.contains(k)) {
            return;
        }
        const QString value = copy.adifFields.value(k);
        if (!value.trimmed().isEmpty()) {
            line += adifTag(k, value);
            emitted.insert(k);
        }
    };

    for (const QString &key : copy.adifFieldOrder) {
        emitField(key);
    }
    for (auto it = copy.adifFields.constBegin(); it != copy.adifFields.constEnd(); ++it) {
        emitField(it.key());
    }
    line += "<EOR>";
    return line;
}

QString AdifLogbook::recordsToAdif(const QVector<LogbookEntry> &records, const QString &headerText)
{
    QString text;
    text += (headerText.trimmed().isEmpty() ? QStringLiteral("Generated by MadModem") : headerText.trimmed());
    text += '\n';
    text += "<PROGRAMID:8>MadModem <EOH>\n";
    for (const LogbookEntry &entry : records) {
        text += entryToAdif(entry);
        text += '\n';
    }
    return text;
}

QVector<LogbookEntry> AdifLogbook::parseAdif(const QString &text)
{
    return parseAdifBytes(text.toUtf8());
}

QString AdifLogbook::extraFieldSummary(const LogbookEntry &entry, int maxFields)
{
    static const QSet<QString> primary = {
        QStringLiteral("CALL"), QStringLiteral("RST_SENT"), QStringLiteral("RST_RCVD"),
        QStringLiteral("BAND"), QStringLiteral("MODE"), QStringLiteral("GRIDSQUARE"),
        QStringLiteral("COMMENT"), QStringLiteral("NOTES"), QStringLiteral("QSO_DATE"),
        QStringLiteral("TIME_ON"), QStringLiteral("QSO_DATE_OFF"), QStringLiteral("TIME_OFF"),
        QStringLiteral("FREQ"), QStringLiteral("NAME"), QStringLiteral("QTH"),
        QStringLiteral("COUNTRY"), QStringLiteral("OPERATOR"), QStringLiteral("STATION_CALLSIGN")
    };

    QStringList keys = entry.adifFieldOrder;
    for (auto it = entry.adifFields.constBegin(); it != entry.adifFields.constEnd(); ++it) {
        if (!keys.contains(it.key())) {
            keys.append(it.key());
        }
    }

    QStringList parts;
    for (const QString &key : keys) {
        if (primary.contains(key)) {
            continue;
        }
        const QString value = entry.adifFields.value(key).trimmed();
        if (value.isEmpty()) {
            continue;
        }
        parts.append(QStringLiteral("%1=%2").arg(key, value));
        if (parts.size() >= maxFields) {
            break;
        }
    }
    return parts.join(QStringLiteral("; "));
}

void AdifLogbook::rebuildCallsignIndex()
{
    m_callsignIndex.clear();
    for (const LogbookEntry &entry : m_records) {
        const QString call = normalizeCallsign(entry.callsign);
        if (!call.isEmpty()) {
            m_callsignIndex.insert(call);
        }
    }
}
