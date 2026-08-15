#ifndef CTYCOUNTRYFILE_H
#define CTYCOUNTRYFILE_H

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief Lightweight loader for AD1C/K1EA country-files.com cty.csv.
 *
 * The file is intentionally loaded from the executable directory, so users can
 * update DXCC/prefix data by replacing cty.csv without rebuilding MadModem.
 */
class CtyCountryFile
{
public:
    struct Entity
    {
        QString primaryPrefix;
        QString name;
        QString dxcc;
        QString continent;
        int cqZone = 0;
        int ituZone = 0;
        double latitude = 0.0;
        double longitude = 0.0;
        double utcOffset = 0.0;
        QString referenceGrid;
        QStringList prefixes;
        QStringList exactCalls;
    };

    struct LookupResult
    {
        bool valid = false;
        Entity entity;
        QString matchedToken;
        bool exactMatch = false;
    };

    static const CtyCountryFile &instance();

    bool isLoaded() const { return m_loaded; }
    QString sourcePath() const { return m_sourcePath; }
    QString errorString() const { return m_errorString; }
    int entityCount() const { return m_entities.size(); }

    LookupResult lookupCallsign(const QString &callsign) const;
    LookupResult lookupEntityNameOrPrefix(const QString &query) const;

    /**
     * Returns the best map/grid reference MadModem can derive from the
     * available data.  Exact 6/8-character station locators are preserved.
     * Empty or coarse 4-character locators are refined with the cty.csv
     * DXCC centroid only when that reference is consistent with the decoded
     * 4-character square, so the San Marino fix applies to every small DXCC
     * entity without moving ordinary QSOs to an unrelated country centroid.
     */
    QString refinedGridForCallsign(const QString &callsign,
                                   const QString &grid,
                                   int precision = 6) const;
    static QString normalizeCallsignForDxcc(const QString &callsign);
    static QString gridFromLonLat(double longitude, double latitude, int precision = 4);

private:
    CtyCountryFile();

    bool loadFromFile(const QString &path);
    static QString cleanPrefixToken(const QString &token);
    static bool looksLikeMaidenheadGrid(const QString &grid, int minLength = 4);
    static QVector<QString> csvFieldsPreserveTail(const QString &line, int firstFields);

    QVector<Entity> m_entities;
    QHash<QString, int> m_exactIndex;
    QVector<QPair<QString, int>> m_prefixIndex;
    bool m_loaded = false;
    QString m_sourcePath;
    QString m_errorString;
};

#endif // CTYCOUNTRYFILE_H
