#include "CtyCountryFile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <algorithm>
#include <cmath>

const CtyCountryFile &CtyCountryFile::instance()
{
    static const CtyCountryFile inst;
    return inst;
}

CtyCountryFile::CtyCountryFile()
{
    const QString exePath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("cty.csv"));
    loadFromFile(exePath);
}

QString CtyCountryFile::normalizeCallsignForDxcc(const QString &callsign)
{
    QString c = callsign.trimmed().toUpper();
    c.remove(QRegularExpression(QStringLiteral("[^A-Z0-9/]+")));
    while (c.contains(QStringLiteral("//"))) {
        c.replace(QStringLiteral("//"), QStringLiteral("/"));
    }
    if (c.startsWith('/')) c.remove(0, 1);
    if (c.endsWith('/')) c.chop(1);
    return c;
}

QString CtyCountryFile::cleanPrefixToken(const QString &token)
{
    QString t = token.trimmed().toUpper();
    if (t.isEmpty()) {
        return QString();
    }

    // cty.csv tokens may contain CQ/ITU/zone overrides such as W(5)[8].  For
    // entity matching we only need the callsign/prefix part before metadata.
    const int paren = t.indexOf('(');
    const int bracket = t.indexOf('[');
    int cut = -1;
    if (paren >= 0) cut = paren;
    if (bracket >= 0 && (cut < 0 || bracket < cut)) cut = bracket;
    if (cut >= 0) t = t.left(cut);

    t.remove(QRegularExpression(QStringLiteral("[^=A-Z0-9/]+")));
    return t;
}

bool CtyCountryFile::looksLikeMaidenheadGrid(const QString &grid, int minLength)
{
    const QString g = grid.trimmed().toUpper();
    if (g.size() < minLength || g.size() < 4) {
        return false;
    }
    if (g.at(0) < QLatin1Char('A') || g.at(0) > QLatin1Char('R') ||
        g.at(1) < QLatin1Char('A') || g.at(1) > QLatin1Char('R') ||
        !g.at(2).isDigit() || !g.at(3).isDigit()) {
        return false;
    }
    if (g.size() >= 6) {
        if (g.at(4) < QLatin1Char('A') || g.at(4) > QLatin1Char('X') ||
            g.at(5) < QLatin1Char('A') || g.at(5) > QLatin1Char('X')) {
            return false;
        }
    }
    return true;
}

QVector<QString> CtyCountryFile::csvFieldsPreserveTail(const QString &line, int firstFields)
{
    QVector<QString> fields;
    fields.reserve(firstFields + 1);
    int start = 0;
    for (int i = 0; i < line.size() && fields.size() < firstFields; ++i) {
        if (line.at(i) == QChar(',')) {
            fields.append(line.mid(start, i - start).trimmed());
            start = i + 1;
        }
    }
    fields.append(line.mid(start).trimmed());
    return fields;
}

bool CtyCountryFile::loadFromFile(const QString &path)
{
    m_sourcePath = path;
    QFile file(path);
    if (!file.exists()) {
        m_errorString = QStringLiteral("cty.csv not found next to executable: %1").arg(path);
        return false;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_errorString = file.errorString();
        return false;
    }

    QTextStream in(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    in.setCodec("UTF-8");
#endif

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.endsWith(';')) {
            line.chop(1);
        }

        const QVector<QString> f = csvFieldsPreserveTail(line, 9);
        if (f.size() < 10) {
            continue;
        }

        Entity e;
        e.primaryPrefix = f.value(0).trimmed().toUpper();
        e.name = f.value(1).trimmed();
        e.dxcc = f.value(2).trimmed();
        e.continent = f.value(3).trimmed().toUpper();
        e.cqZone = f.value(4).toInt();
        e.ituZone = f.value(5).toInt();
        e.latitude = f.value(6).toDouble();
        // country-files.com cty.csv stores longitude with the ham-radio
        // convention: positive west, negative east.  MadModem map geometry uses
        // the normal GIS convention: positive east.  Invert once at load time,
        // otherwise European DXCC centroids fall on the wrong side of Greenwich.
        e.longitude = -f.value(7).toDouble();
        e.utcOffset = f.value(8).toDouble();
        e.referenceGrid = gridFromLonLat(e.longitude, e.latitude, 6);

        const QStringList tokens = f.value(9).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        for (const QString &raw : tokens) {
            QString t = cleanPrefixToken(raw);
            if (t.isEmpty()) {
                continue;
            }
            if (t.startsWith('=')) {
                t.remove(0, 1);
                if (!t.isEmpty()) {
                    e.exactCalls.append(t);
                }
            } else {
                e.prefixes.append(t);
            }
        }
        if (!e.primaryPrefix.isEmpty() && !e.prefixes.contains(e.primaryPrefix)) {
            e.prefixes.prepend(e.primaryPrefix);
        }

        const int idx = m_entities.size();
        m_entities.append(e);
        for (const QString &exact : e.exactCalls) {
            m_exactIndex.insert(exact, idx);
        }
        for (const QString &prefix : e.prefixes) {
            if (!prefix.isEmpty()) {
                m_prefixIndex.append(qMakePair(prefix, idx));
            }
        }
    }

    std::sort(m_prefixIndex.begin(), m_prefixIndex.end(), [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
        if (a.first.size() != b.first.size()) return a.first.size() > b.first.size();
        return a.first < b.first;
    });

    m_loaded = !m_entities.isEmpty();
    if (!m_loaded) {
        m_errorString = QStringLiteral("cty.csv loaded but no country entities were parsed: %1").arg(path);
    }
    return m_loaded;
}


CtyCountryFile::LookupResult CtyCountryFile::lookupEntityNameOrPrefix(const QString &query) const
{
    LookupResult result;
    const QString q = query.trimmed();
    if (q.isEmpty() || m_entities.isEmpty()) {
        return result;
    }

    QString norm = q.toUpper();
    norm.replace(QChar('_'), QChar(' '));
    norm.replace(QChar('-'), QChar(' '));
    norm = norm.simplified();

    auto canonicalAlias = [](const QString &value) -> QString {
        const QString v = value.trimmed().toUpper().simplified();
        if (v == QLatin1String("GERMANY") || v == QLatin1String("DEUTSCHLAND") || v == QLatin1String("DL")) {
            return QStringLiteral("FED. REP. OF GERMANY");
        }
        if (v == QLatin1String("USA") || v == QLatin1String("UNITED STATES") || v == QLatin1String("UNITED STATES OF AMERICA")) {
            return QStringLiteral("UNITED STATES");
        }
        if (v == QLatin1String("UK") || v == QLatin1String("UNITED KINGDOM") || v == QLatin1String("GREAT BRITAIN")) {
            return QStringLiteral("ENGLAND");
        }
        if (v == QLatin1String("SAN MARINO")) {
            return QStringLiteral("SAN MARINO");
        }
        if (v == QLatin1String("CZECH REPUBLIC") || v == QLatin1String("CZECHIA")) {
            return QStringLiteral("CZECH REPUBLIC");
        }
        if (v == QLatin1String("IVORY COAST") || v == QLatin1String("COTE D IVOIRE") || v == QLatin1String("CÔTE D'IVOIRE")) {
            return QStringLiteral("COTE D'IVOIRE");
        }
        return v;
    };

    const QString aliasNorm = canonicalAlias(norm);
    auto use = [&](int idx, const QString &token, bool exact) {
        if (idx >= 0 && idx < m_entities.size()) {
            result.valid = true;
            result.entity = m_entities.at(idx);
            result.matchedToken = token;
            result.exactMatch = exact;
        }
    };

    // Exact country/DXCC/name matching must happen before prefix matching.
    // Otherwise free text such as "Germany" can be misread as a G* callsign/prefix.
    for (int i = 0; i < m_entities.size(); ++i) {
        const Entity &e = m_entities.at(i);
        const QString nameNorm = canonicalAlias(e.name);
        if (nameNorm == aliasNorm ||
            e.name.compare(q, Qt::CaseInsensitive) == 0 ||
            e.dxcc.compare(q, Qt::CaseInsensitive) == 0 ||
            e.primaryPrefix.compare(norm, Qt::CaseInsensitive) == 0) {
            use(i, e.name, true);
            return result;
        }
    }

    // If the input looks like a human country name, try contains matching before callsign lookup.
    const bool looksLikeCountryText = q.contains(QRegularExpression(QStringLiteral("[\s_.-]"))) || q.size() > 3;
    if (looksLikeCountryText) {
        for (int i = 0; i < m_entities.size(); ++i) {
            const Entity &e = m_entities.at(i);
            if (e.name.contains(q, Qt::CaseInsensitive) || aliasNorm.contains(canonicalAlias(e.name)) || canonicalAlias(e.name).contains(aliasNorm)) {
                use(i, e.name, false);
                return result;
            }
        }
    }

    // Finally allow ordinary prefix/callsign matching, so EA, DL, K, I, or real
    // callsigns resolve through the same DXCC rules as the decode table.
    result = lookupCallsign(q);
    if (result.valid) {
        return result;
    }

    for (int i = 0; i < m_entities.size(); ++i) {
        const Entity &e = m_entities.at(i);
        if (e.name.contains(q, Qt::CaseInsensitive)) {
            use(i, e.name, false);
            return result;
        }
    }
    return result;
}

CtyCountryFile::LookupResult CtyCountryFile::lookupCallsign(const QString &callsign) const
{
    LookupResult result;
    const QString call = normalizeCallsignForDxcc(callsign);
    if (call.isEmpty() || m_entities.isEmpty()) {
        return result;
    }

    auto useIndex = [&](int idx, const QString &token, bool exact) {
        if (idx >= 0 && idx < m_entities.size()) {
            result.valid = true;
            result.entity = m_entities.at(idx);
            result.matchedToken = token;
            result.exactMatch = exact;
        }
    };

    if (m_exactIndex.contains(call)) {
        useIndex(m_exactIndex.value(call), call, true);
        return result;
    }

    // Portable calls are tricky.  Try the whole call first, then try individual
    // components so EA/IZ6NNH or IZ6NNH/P still resolve sensibly.
    QStringList candidates;
    candidates << call;
    const QStringList parts = call.split('/', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (!candidates.contains(part)) {
            candidates << part;
        }
    }

    for (const QString &candidate : candidates) {
        for (const auto &p : m_prefixIndex) {
            if (candidate.startsWith(p.first)) {
                useIndex(p.second, p.first, false);
                return result;
            }
        }
    }
    return result;
}

QString CtyCountryFile::refinedGridForCallsign(const QString &callsign, const QString &grid, int precision) const
{
    const int outLen = qBound(4, precision, 8);
    QString g = grid.trimmed().toUpper();

    // A real station locator is always better than a country-file centroid.
    if (looksLikeMaidenheadGrid(g, 6)) {
        return g.left(qMin(g.size(), outLen));
    }

    const LookupResult cty = lookupCallsign(callsign);
    const QString ref = cty.valid ? cty.entity.referenceGrid.trimmed().toUpper() : QString();
    const bool refOk = looksLikeMaidenheadGrid(ref, 6);

    if (g.isEmpty() || !looksLikeMaidenheadGrid(g.left(4), 4)) {
        return refOk ? ref.left(qMin(ref.size(), outLen)) : QString();
    }

    g = g.left(4);

    // Generic small-DXCC refinement: when the only locator we have is the
    // coarse 4-character square and the cty.csv reference point is inside the
    // same square, use the cty.csv 6-character locator.  This fixes San Marino,
    // Vatican, Monaco, Gibraltar, Malta-sized entities, etc. without using a
    // special-case DXCC number and without moving a station to a different grid.
    if (refOk && ref.left(4) == g) {
        return ref.left(qMin(ref.size(), outLen));
    }

    return g;
}

QString CtyCountryFile::gridFromLonLat(double longitude, double latitude, int precision)
{
    if (!std::isfinite(longitude) || !std::isfinite(latitude) || latitude < -90.0 || latitude > 90.0) {
        return QString();
    }
    while (longitude < -180.0) longitude += 360.0;
    while (longitude >= 180.0) longitude -= 360.0;

    double lon = longitude + 180.0;
    double lat = latitude + 90.0;
    QString grid;
    grid.reserve(qMax(4, precision));

    const int a = static_cast<int>(std::floor(lon / 20.0));
    const int b = static_cast<int>(std::floor(lat / 10.0));
    grid.append(QChar('A' + qBound(0, a, 17)));
    grid.append(QChar('A' + qBound(0, b, 17)));
    lon -= a * 20.0;
    lat -= b * 10.0;

    const int c = static_cast<int>(std::floor(lon / 2.0));
    const int d = static_cast<int>(std::floor(lat / 1.0));
    grid.append(QChar('0' + qBound(0, c, 9)));
    grid.append(QChar('0' + qBound(0, d, 9)));
    lon -= c * 2.0;
    lat -= d * 1.0;

    if (precision >= 6) {
        const int e = static_cast<int>(std::floor(lon / (2.0 / 24.0)));
        const int f = static_cast<int>(std::floor(lat / (1.0 / 24.0)));
        grid.append(QChar('A' + qBound(0, e, 23)));
        grid.append(QChar('A' + qBound(0, f, 23)));
    }
    return grid.left(qMax(4, precision)).toUpper();
}
