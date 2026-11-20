#include "LocatorSources.h"

/*
 * Locator lookup (IC8TEM, port of MSHV-IC8TEM src/HvTxW/hvlocsrc.*). See LocatorSources.h.
 * Copyright (C) 2026 Costantino Cerrotta, IC8TEM. GNU GPL v3 or later.
 */

#include "../MadModemVersion.h"
// The IC8TEM edition defines these in MadModemVersion.h; defaults for a tree without them.
#ifndef MADMODEM_EDITION
#define MADMODEM_EDITION ""
#endif
#ifndef MADMODEM_UPDATE_REPOSITORY
#define MADMODEM_UPDATE_REPOSITORY "l-papadopol/MM"
#endif
#include "../settings/AppSettings.h"
#include "../utils/InstallationPaths.h"
#include "../utils/RuntimeI18n.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDataStream>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSettings>
#include <QSslSocket>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cstring>

namespace {

constexpr int kRefreshDays = 14;
constexpr int kWebDays = 30;   // an answer with a locator is kept 30 days
constexpr int kWebMax = 5000;
const char *const kWebUrl = "https://api.g7vrd.co.uk/v1/locators/by-callsign/";
const char *const kUserAgent = "MadModem/" MADMODEM_VERSION_STRING " " MADMODEM_EDITION " (+https://github.com/" MADMODEM_UPDATE_REPOSITORY ")";

const char *const kUrl[LocatorSources::Count] = {
    "",
    "https://downloads.sourceforge.net/project/wsjt-x-improved/Additional%20Files/CALL3.TXT",
    "https://www.bigskyspaces.com/w7gj/CALL3.TXT",
    "https://downloads.sourceforge.net/project/jtdx/grid_data.bin",
    "https://api.g7vrd.co.uk/v1/locators/by-callsign/",
};
const char *const kFile[LocatorSources::Count] = {
    "", "CALL3_WSJT-X_Improved.TXT", "CALL3_EME_W7GJ.TXT", "grid_data.bin", "g7vrd_cache.txt",
};

QString uiText(const char *key, const char *fallback)
{
    return MadModemI18n::translate(QString::fromUtf8(key), QString::fromUtf8(fallback));
}

bool inRange(QChar c, char lo, char hi)
{
    return c.unicode() >= static_cast<ushort>(static_cast<uchar>(lo)) && c.unicode() <= static_cast<ushort>(static_cast<uchar>(hi));
}

// "fn42ab12" -> "FN42AB", "FN42" -> "FN42", else ""
QString normalizeGrid(const QString &raw)
{
    const QString g = raw.trimmed().toUpper();
    if (g.size() >= 6 && LocatorSources::isGrid(g.left(6))) return g.left(6);
    if (g.size() >= 4 && LocatorSources::isGrid(g.left(4))) return g.left(4);
    return QString();
}

bool looksLikeCall(const QString &c)
{
    const int n = c.size();
    if (n < 3 || n > 12) return false;
    if (c.at(0) == QLatin1Char('/') || c.at(n - 1) == QLatin1Char('/') || c.contains(QStringLiteral("//"))) return false;
    if (c.at(0) == QLatin1Char('Q') || c.at(0) == QLatin1Char('0')) return false;
    bool digit = false, letter = false;
    for (QChar ch : c) {
        if (inRange(ch, '0', '9')) digit = true;
        else if (inRange(ch, 'A', 'Z')) letter = true;
        else if (ch != QLatin1Char('/')) return false;
    }
    return digit && letter;
}

// contest serial between the call and the grid ("570007")
bool numberLike(const QString &w)
{
    if (w.isEmpty()) return false;
    for (int i = 0; i < w.size(); ++i) {
        const QChar c = w.at(i);
        if (!inRange(c, '0', '9') && !(i == 0 && (c == QLatin1Char('+') || c == QLatin1Char('-')))) return false;
    }
    return true;
}

quint32 encodeGrid(const QString &g)
{
    const quint32 f1 = g.at(0).unicode() - 'A', f2 = g.at(1).unicode() - 'A';
    const quint32 s1 = g.at(2).unicode() - '0', s2 = g.at(3).unicode() - '0';
    quint32 u1 = 0, u2 = 0;
    if (g.size() >= 6) {
        u1 = g.at(4).unicode() - 'A' + 1;
        u2 = g.at(5).unicode() - 'A' + 1;
    }
    return (((((f1 * 18 + f2) * 10 + s1) * 10 + s2) * 25 + u1) * 25 + u2) + 1;
}

QString decodeGrid(quint32 c)
{
    if (c == 0) return QString();
    c -= 1;
    const quint32 u2 = c % 25; c /= 25;
    const quint32 u1 = c % 25; c /= 25;
    const quint32 s2 = c % 10; c /= 10;
    const quint32 s1 = c % 10; c /= 10;
    const quint32 f2 = c % 18; c /= 18;
    const quint32 f1 = c;
    if (f1 > 17) return QString();
    QString g;
    g += QChar('A' + f1);
    g += QChar('A' + f2);
    g += QChar('0' + s1);
    g += QChar('0' + s2);
    if (u1 && u2) {
        g += QChar('A' + u1 - 1);
        g += QChar('A' + u2 - 1);
    }
    return g;
}

void callKey(const QString &call, char *key)
{
    std::memset(key, 0, 12);
    const QByteArray a = call.toLatin1();
    std::memcpy(key, a.constData(), static_cast<std::size_t>(qMin(12, a.size())));
}

bool recordLess(const LocatorRecord &a, const LocatorRecord &b)
{
    return std::memcmp(a.call, b.call, 12) < 0;
}

void addRecord(QVector<LocatorRecord> &v, const QString &call, const QString &grid)
{
    if (call.size() > 12) return;
    LocatorRecord r;
    callKey(call, r.call);
    r.grid = encodeGrid(grid);
    v.append(r);
}

// sorted by call, one record for each call (the first one)
void finishTable(QVector<LocatorRecord> &v)
{
    std::stable_sort(v.begin(), v.end(), recordLess);
    int out = 0;
    for (int i = 0; i < v.size(); ++i) {
        if (out > 0 && std::memcmp(v.at(out - 1).call, v.at(i).call, 12) == 0) continue;
        v[out++] = v.at(i);
    }
    v.resize(out);
    v.squeeze();
}

// CALL3.TXT: "CALL,LOCATOR,EME flag,...", "//" comments, last record "ZZZZZZ,..."
bool parseCall3(const QByteArray &data, LocatorTable &t, QString &error)
{
    if (data.size() < 100 || data.trimmed().startsWith('<')) {
        error = QStringLiteral("not a CALL3.TXT file");
        return false;
    }
    t.records.reserve(data.size() / 24);
    int pos = 0, lines = 0;
    const int n = data.size();
    while (pos < n) {
        int e = data.indexOf('\n', pos);
        if (e < 0) e = n;
        const QByteArray line = data.mid(pos, e - pos).trimmed();
        pos = e + 1;
        ++lines;
        if (line.isEmpty()) continue;
        if (line.startsWith("//")) {
            if (t.info.isEmpty() && lines < 8) {
                const int r = line.indexOf("Revised");
                if (r >= 0) t.info = QString::fromLatin1(line.mid(r)).simplified();
            }
            continue;
        }
        if (line.startsWith("ZZZZ")) continue;
        const int c1 = line.indexOf(',');
        if (c1 <= 0) continue;
        const int c2 = line.indexOf(',', c1 + 1);
        const QString call = QString::fromLatin1(line.left(c1)).trimmed().toUpper();
        const QString grid = normalizeGrid(QString::fromLatin1(c2 < 0 ? line.mid(c1 + 1) : line.mid(c1 + 1, c2 - c1 - 1)));
        if (grid.isEmpty() || !looksLikeCall(call)) continue;
        addRecord(t.records, call, grid);
    }
    finishTable(t.records);
    if (t.records.size() < 300) {
        error = QStringLiteral("too few valid lines (%1)").arg(t.records.size());
        return false;
    }
    return true;
}

// grid_data.bin (JTDX): qCompress( QDataStream BigEndian: quint32 rows, rows x (QString call, QString grid) )
bool parseGridData(const QByteArray &raw, LocatorTable &t, QString &error)
{
    if (raw.size() <= 8 || static_cast<unsigned char>(raw.at(4)) != 0x78) {
        error = QStringLiteral("not a grid_data.bin file");
        return false;
    }
    QByteArray d = qUncompress(raw);
    if (d.isEmpty()) {
        error = QStringLiteral("cannot decompress");
        return false;
    }
    QDataStream in(d);
    in.setByteOrder(QDataStream::BigEndian);
    quint32 rows = 0;
    in >> rows;
    if (in.status() != QDataStream::Ok || rows == 0 || rows > 2000000) {
        error = QStringLiteral("bad number of rows (%1)").arg(rows);
        return false;
    }
    t.records.reserve(static_cast<int>(rows));
    for (quint32 i = 0; i < rows; ++i) {
        QString k, g;
        in >> k >> g;
        if (in.status() != QDataStream::Ok) {
            error = QStringLiteral("truncated at row %1").arg(i);
            return false;
        }
        k = k.trimmed().toUpper();
        k.remove(QLatin1Char('<'));
        k.remove(QLatin1Char('>'));
        g = normalizeGrid(g);
        if (g.isEmpty() || !looksLikeCall(k)) continue;
        addRecord(t.records, k, g);
    }
    d.clear();
    finishTable(t.records);
    if (t.records.size() < 1000) {
        error = QStringLiteral("too few valid rows (%1)").arg(t.records.size());
        return false;
    }
    return true;
}

QString dataFolder()
{
    const QString exe = QCoreApplication::applicationDirPath();
    const QString base = MadModemInstallation::isManaged(exe)
                             ? MadModemInstallation::writableLocation(QStandardPaths::AppDataLocation)
                             : exe;
    return QDir(base).filePath(QStringLiteral("locsources"));
}

} // namespace

bool LocatorTable::find(const QString &call, QString &grid) const
{
    if (records.isEmpty() || call.isEmpty() || call.size() > 12) return false;
    LocatorRecord key;
    callKey(call, key.call);
    key.grid = 0;
    const auto it = std::lower_bound(records.constBegin(), records.constEnd(), key, recordLess);
    if (it == records.constEnd() || std::memcmp(it->call, key.call, 12) != 0) return false;
    grid = decodeGrid(it->grid);
    return !grid.isEmpty();
}

// ------------------------------------------------------------------ loader thread

LocatorLoader::LocatorLoader(int s, const QString &f, QObject *parent)
    : QThread(parent), source(s), file(f)
{
}

void LocatorLoader::run()
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("cannot open %1").arg(QFileInfo(file).fileName());
        return;
    }
    const QByteArray data = f.readAll();
    f.close();
    LocatorTablePtr t(new LocatorTable());
    const bool ok = source == LocatorSources::Jtdx ? parseGridData(data, *t, error) : parseCall3(data, *t, error);
    if (ok) table = t;
}

// ------------------------------------------------------------------ messages

bool LocatorSources::isGrid(const QString &g)
{
    const int n = g.size();
    if (n != 4 && n != 6) return false;
    if (!inRange(g.at(0), 'A', 'R') || !inRange(g.at(1), 'A', 'R') || !inRange(g.at(2), '0', '9') || !inRange(g.at(3), '0', '9')) return false;
    if (n == 6 && (!inRange(g.at(4), 'A', 'X') || !inRange(g.at(5), 'A', 'X'))) return false;
    return true;
}

// "CQ [DX] CALL GRID", "QRZ CALL GRID", "CALL1 CALL2 [R] GRID", "CALL1 CALL2 R 570007 JN70AA", "CALL GRID".
// Not RR73. The grid belongs to the station that sent the message.
bool LocatorSources::parseMessage(const QString &message, QString &call, QString &grid)
{
    call.clear();
    grid.clear();
    QString m = message;
    m.replace(QLatin1Char('\n'), QLatin1Char(' '));
    m.remove(QLatin1Char('<'));
    m.remove(QLatin1Char('>'));
    m = m.simplified();
    if (m.startsWith(QStringLiteral("TU; "))) m.remove(0, 4);
    if (m.isEmpty()) return false;
    QStringList w = m.split(QLatin1Char(' '));
    while (!w.isEmpty()) { // marks of some decoders at the end
        const QString &l = w.last();
        if (l == QLatin1String("OOO") || l == QLatin1String("?") || l == QLatin1String("*") || l == QLatin1String("#") ||
            l == QLatin1String("^") || l == QLatin1String(".") ||
            (l.size() == 2 && (l.at(0) == QLatin1Char('a') || l.at(0) == QLatin1Char('q')) && l.at(1).isDigit()))
            w.removeLast();
        else
            break;
    }
    if (w.size() < 2 || w.size() > 7) return false;
    const QString g = w.last().toUpper();
    if (g == QLatin1String("RR73") || !isGrid(g)) return false;
    int i = w.size() - 2;
    while (i > 0 && (w.at(i) == QLatin1String("R") || numberLike(w.at(i)))) --i;
    const QString c = w.at(i).toUpper();
    if (!looksLikeCall(c) || isGrid(c)) return false;
    if (i >= 1) {
        const QString w0 = w.at(0).toUpper();
        const bool cq = w0 == QLatin1String("CQ") || w0 == QLatin1String("QRZ") || w0 == QLatin1String("DE");
        if (cq && i > 2) return false;
        if (!cq && !(i == 1 && looksLikeCall(w0))) return false;
    }
    call = c;
    grid = g;
    return true;
}

// ------------------------------------------------------------------ LocatorSources

LocatorSources::LocatorSources(QObject *parent)
    : QObject(parent), m_dir(dataFolder())
{
    for (int i = 0; i < Count; ++i) {
        m_on[i] = true;
        m_generation[i] = 0;
    }
    m_network = new QNetworkAccessManager(this);
    m_downloadTimer = new QTimer(this);
    m_downloadTimer->setSingleShot(true);
    connect(m_downloadTimer, &QTimer::timeout, this, &LocatorSources::downloadTimeout);
    m_webTimer = new QTimer(this);
    m_webTimer->setSingleShot(true);
    connect(m_webTimer, &QTimer::timeout, this, &LocatorSources::webTimeout);
    m_checkTimer = new QTimer(this);
    connect(m_checkTimer, &QTimer::timeout, this, &LocatorSources::checkUpdates);
    readSettings();
    readWebCache();
    for (int s = Call3; s <= Jtdx; ++s) {
        if (m_on[s] && QFile::exists(filePath(s))) load(s, filePath(s));
    }
    QTimer::singleShot(30000, this, &LocatorSources::checkUpdates); // not during the start of MadModem
    m_checkTimer->start(6 * 3600 * 1000);
}

LocatorSources::~LocatorSources()
{
    for (LocatorLoader *l : std::as_const(m_loaders)) {
        disconnect(l, nullptr, this, nullptr);
        l->wait();
        delete l;
    }
    m_loaders.clear();
    if (m_downloadReply) m_downloadReply->abort();
    if (m_webReply) m_webReply->abort();
}

QString LocatorSources::tag(int s)
{
    switch (s) {
    case Rx: return QStringLiteral("RX");
    case Call3: return QStringLiteral("C3");
    case Eme: return QStringLiteral("EME");
    case Jtdx: return QStringLiteral("JTDX");
    case Web: return QStringLiteral("WEB");
    default: return QString();
    }
}

QString LocatorSources::name(int s)
{
    switch (s) {
    case Rx: return uiText("loc_src_rx", "Decodes of the last %1 periods").arg(periods());
    case Call3: return QStringLiteral("CALL3.TXT (WSJT-X Improved)");
    case Eme: return QStringLiteral("CALL3.TXT EME (W7GJ)");
    case Jtdx: return QStringLiteral("grid_data.bin (JTDX)");
    case Web: return uiText("loc_src_web", "Online (api.g7vrd.co.uk)");
    default: return QString();
    }
}

QString LocatorSources::filePath(int s) const
{
    return QDir(m_dir).filePath(QString::fromLatin1(kFile[s]));
}

void LocatorSources::readSettings()
{
    m_loadingSettings = true;
    QSettings s(AppSettings::settingsFilePath(), QSettings::IniFormat);
    s.beginGroup(QStringLiteral("LocatorSources"));
    const char *keys[Count] = {"rx", "call3", "eme", "jtdx", "web"};
    for (int i = 0; i < Count; ++i) {
        m_on[i] = s.value(QString::fromLatin1(keys[i]), true).toBool();
    }
    m_autoUpdate = s.value(QStringLiteral("autoUpdate"), true).toBool();
    s.endGroup();
    m_loadingSettings = false;
}

void LocatorSources::saveSettings() const
{
    if (m_loadingSettings) return;
    QSettings s(AppSettings::settingsFilePath(), QSettings::IniFormat);
    s.beginGroup(QStringLiteral("LocatorSources"));
    const char *keys[Count] = {"rx", "call3", "eme", "jtdx", "web"};
    for (int i = 0; i < Count; ++i) {
        s.setValue(QString::fromLatin1(keys[i]), m_on[i]);
    }
    s.setValue(QStringLiteral("autoUpdate"), m_autoUpdate);
    s.endGroup();
}

void LocatorSources::setOn(int s, bool on)
{
    if (s < 0 || s >= Count || m_on[s] == on) return;
    m_on[s] = on;
    if (s >= Call3 && s <= Jtdx) {
        if (!on) {
            m_tables[s].clear();
            m_generation[s]++;
            m_downloadQueue.removeAll(s);
        } else {
            if (QFile::exists(filePath(s))) load(s, filePath(s));
            QTimer::singleShot(500, this, &LocatorSources::checkUpdates);
        }
    }
    if (s == Rx && !on) m_rx.clear();
    saveSettings();
    emit changed();
}

void LocatorSources::setAutoUpdate(bool on)
{
    if (m_autoUpdate == on) return;
    m_autoUpdate = on;
    saveSettings();
    if (on) QTimer::singleShot(500, this, &LocatorSources::checkUpdates);
    emit changed();
}

// ------------------------------------------------------------------ RX

void LocatorSources::addRx(const QString &message)
{
    if (!m_on[Rx]) return;
    QString call, grid;
    if (!parseMessage(message, call, grid)) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto it = m_rx.find(call);
    if (it != m_rx.end()) {
        if (!(grid.size() == 4 && it->grid.size() == 6 && it->grid.startsWith(grid))) it->grid = grid; // keep 6 characters
        it->ms = now;
    } else {
        RxEntry e;
        e.grid = grid;
        e.ms = now;
        m_rx.insert(call, e);
    }
    if (++m_rxAdds >= 256) {
        m_rxAdds = 0;
        const qint64 keep = static_cast<qint64>(qMax(m_rxPeriod, 15.0) * periods() * 2000.0);
        for (it = m_rx.begin(); it != m_rx.end();) {
            if (now - it->ms > keep) it = m_rx.erase(it);
            else ++it;
        }
    }
}

// ------------------------------------------------------------------ lookups

QString LocatorSources::findOne(int s, const QString &call0, double periodSeconds) const
{
    if (s < 0 || s >= Count || !m_on[s]) return QString();
    const QString call = call0.trimmed().toUpper();
    if (call.size() < 3) return QString();
    if (s == Rx) {
        if (periodSeconds > 0.5) m_rxPeriod = periodSeconds;
        const auto it = m_rx.constFind(call);
        if (it == m_rx.constEnd()) return QString();
        if (QDateTime::currentMSecsSinceEpoch() - it->ms > static_cast<qint64>(m_rxPeriod * periods() * 1000.0)) return QString();
        return it->grid;
    }
    if (s == Web) { // answers already received (kept 30 days)
        const auto it = m_web.constFind(call);
        if (it == m_web.constEnd()) return QString();
        if (QDateTime::currentMSecsSinceEpoch() / 1000 - it->t > static_cast<qint64>(kWebDays) * 86400) return QString();
        return it->grid;
    }
    QString g;
    if (m_tables[s] && m_tables[s]->find(call, g)) return g;
    return QString();
}

QString LocatorSources::findLocal(const QString &call, double periodSeconds, int &source) const
{
    source = -1;
    QString best;
    for (int s = Rx; s < Count; ++s) {
        const QString g = findOne(s, call, periodSeconds);
        if (g.isEmpty()) continue;
        if (best.isEmpty()) {
            best = g;
            source = s;
            if (g.size() == 6) break;
            continue;
        }
        if (g.size() == 6 && g.startsWith(best)) {
            best = g;
            source = s;
            break;
        }
    }
    return best;
}

QString LocatorSources::tooltip(const QString &call0, double periodSeconds) const
{
    const QString call = call0.trimmed().toUpper();
    QString t = uiText("loc_tooltip_title", "Locator of %1:").arg(call);
    for (int s = Rx; s < Count; ++s) {
        QString v;
        if (!m_on[s]) {
            v = uiText("loc_off", "off");
        } else {
            v = findOne(s, call, periodSeconds);
            if (v.isEmpty()) {
                if (s == Web && !m_web.contains(call)) v = uiText("loc_not_asked", "not asked");
                else if (s >= Call3 && s <= Jtdx && !m_tables[s]) v = uiText("loc_not_loaded", "list not loaded");
                else v = QStringLiteral("-");
            }
        }
        t += QStringLiteral("\n") + tag(s).leftJustified(5, QLatin1Char(' ')) + name(s) + QStringLiteral(":  ") + v;
    }
    return t;
}

QString LocatorSources::status(int s) const
{
    if (s == Rx) return uiText("loc_status_rx", "Calls heard with a locator: %1").arg(m_rx.size());
    if (s == Web) {
        QString t = uiText("loc_status_web", "Answers kept: %1    asked since the start: %2").arg(m_web.size()).arg(m_webAsked);
        if (!m_webError.isEmpty()) t += QStringLiteral("\n") + m_webError;
        return t;
    }
    if (s < Call3 || s > Jtdx) return QString();
    QString t = m_tables[s] ? uiText("loc_status_calls", "Calls: %1").arg(m_tables[s]->records.size())
                            : uiText("loc_status_not_loaded", "List not loaded");
    const QFileInfo fi(filePath(s));
    if (fi.exists()) t += QStringLiteral("    ") + uiText("loc_status_downloaded", "downloaded: %1").arg(fi.lastModified().toString(QStringLiteral("yyyy-MM-dd")));
    if (m_tables[s] && !m_tables[s]->info.isEmpty()) t += QStringLiteral("    (") + m_tables[s]->info + QLatin1Char(')');
    if (m_downloading == s) t += QStringLiteral("    ") + uiText("loc_status_downloading", "downloading...");
    else if (m_downloadQueue.contains(s)) t += QStringLiteral("    ") + uiText("loc_status_waiting", "waiting to download");
    else if (!m_error[s].isEmpty()) t += QStringLiteral("\n") + m_error[s];
    return t;
}

// ------------------------------------------------------------------ lists: load

void LocatorSources::load(int s, const QString &file)
{
    auto *l = new LocatorLoader(s, file, nullptr);
    l->generation = ++m_generation[s];
    m_loaders.append(l);
    connect(l, &QThread::finished, this, &LocatorSources::loaderFinished);
    l->start(QThread::LowPriority);
}

void LocatorSources::loaderFinished()
{
    auto *l = qobject_cast<LocatorLoader *>(sender());
    if (!l || !m_loaders.contains(l)) return;
    m_loaders.removeAll(l);
    const int s = l->source;
    const bool downloaded = l->file.endsWith(QStringLiteral(".tmp"));
    if (l->table) {
        if (downloaded) {
            QFile::remove(filePath(s));
            if (!QFile::rename(l->file, filePath(s))) {
                QFile::copy(l->file, filePath(s));
                QFile::remove(l->file);
            }
        }
        const bool used = m_on[s] && l->generation == m_generation[s];
        if (used) m_tables[s] = l->table;
        m_error[s].clear();
        if (used) QTimer::singleShot(0, this, &LocatorSources::listLoaded);
    } else if (downloaded) {
        QFile::remove(l->file);
        m_error[s] = uiText("loc_bad_download", "The downloaded file is not valid: %1").arg(l->error);
    } else {
        m_error[s] = l->error;
    }
    l->deleteLater();
    if (downloaded && m_downloading == s) nextDownload();
    emit changed();
}

// ------------------------------------------------------------------ lists: download

void LocatorSources::updateNow()
{
    m_force = true;
    checkUpdates();
}

void LocatorSources::checkUpdates()
{
    for (int s = Call3; s <= Jtdx; ++s) {
        if (!m_on[s] || s == m_downloading || m_downloadQueue.contains(s)) continue;
        const QFileInfo fi(filePath(s));
        const bool old = !fi.exists() || fi.lastModified().daysTo(QDateTime::currentDateTime()) >= kRefreshDays;
        if (m_force || (m_autoUpdate && old)) m_downloadQueue.append(s);
    }
    m_force = false;
    if (m_downloading < 0) nextDownload();
    else emit changed();
}

void LocatorSources::nextDownload()
{
    m_downloading = -1;
    while (!m_downloadQueue.isEmpty()) {
        const int s = m_downloadQueue.takeFirst();
        if (!m_on[s]) continue;
        m_downloading = s;
        break;
    }
    if (m_downloading < 0) {
        emit changed();
        return;
    }
    QDir().mkpath(m_dir);
    m_error[m_downloading].clear();
    emit changed();
    const QString url = QString::fromLatin1(kUrl[m_downloading]);
    m_downloadTimer->start(300000);
    if (QSslSocket::supportsSsl()) {
        QNetworkRequest request{QUrl(url)};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setMaximumRedirectsAllowed(10);
        request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
        m_downloadReply = m_network->get(request);
        connect(m_downloadReply, &QNetworkReply::finished, this, &LocatorSources::downloadFinished);
        return;
    }
    const QString curl = QStandardPaths::findExecutable(QStringLiteral("curl")); // Qt without OpenSSL: curl of the system
    if (curl.isEmpty()) {
        downloadDone(false, QByteArray(), uiText("loc_no_https", "HTTPS is not available (no OpenSSL and no curl)"));
        return;
    }
    m_downloadProcess = new QProcess(this);
    const QStringList args = {QStringLiteral("-L"), QStringLiteral("-s"), QStringLiteral("-S"), QStringLiteral("-f"),
                              QStringLiteral("--max-time"), QStringLiteral("280"), QStringLiteral("-A"), QString::fromLatin1(kUserAgent),
                              QStringLiteral("-o"), filePath(m_downloading) + QStringLiteral(".dl"), url};
    connect(m_downloadProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &LocatorSources::curlFinished);
    m_downloadProcess->start(curl, args);
}

void LocatorSources::downloadFinished()
{
    auto *r = qobject_cast<QNetworkReply *>(sender());
    if (!r) return;
    r->deleteLater();
    if (r != m_downloadReply) return;
    m_downloadReply = nullptr;
    m_downloadTimer->stop();
    if (r->error() != QNetworkReply::NoError) {
        downloadDone(false, QByteArray(), r->errorString());
        return;
    }
    downloadDone(true, r->readAll(), QString());
}

void LocatorSources::curlFinished(int code, QProcess::ExitStatus status)
{
    auto *p = qobject_cast<QProcess *>(sender());
    if (!p) return;
    p->deleteLater();
    if (p != m_downloadProcess) return;
    m_downloadProcess = nullptr;
    m_downloadTimer->stop();
    const QString dl = filePath(m_downloading) + QStringLiteral(".dl");
    if (status != QProcess::NormalExit || code != 0) {
        const QString err = QString::fromLocal8Bit(p->readAllStandardError()).trimmed();
        QFile::remove(dl);
        downloadDone(false, QByteArray(), err.isEmpty() ? QStringLiteral("curl %1").arg(code) : err);
        return;
    }
    QFile f(dl);
    QByteArray data;
    if (f.open(QIODevice::ReadOnly)) {
        data = f.readAll();
        f.close();
    }
    QFile::remove(dl);
    downloadDone(true, data, QString());
}

void LocatorSources::downloadTimeout()
{
    if (m_downloadReply) {
        QNetworkReply *r = m_downloadReply;
        m_downloadReply = nullptr;
        r->abort();
    }
    if (m_downloadProcess) {
        QProcess *p = m_downloadProcess;
        m_downloadProcess = nullptr;
        p->kill();
        p->deleteLater();
    }
    if (m_downloading >= 0) downloadDone(false, QByteArray(), uiText("loc_timeout", "no answer (timeout)"));
}

void LocatorSources::downloadDone(bool ok, const QByteArray &data, const QString &error)
{
    const int s = m_downloading;
    if (s < 0) return;
    if (!ok || data.isEmpty()) {
        m_error[s] = uiText("loc_download_failed", "Download failed: %1").arg(error.isEmpty() ? uiText("loc_empty_answer", "empty answer") : error);
        nextDownload();
        return;
    }
    const QString tmp = filePath(s) + QStringLiteral(".tmp");
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size()) {
        f.close();
        QFile::remove(tmp);
        m_error[s] = uiText("loc_cannot_write", "Cannot write %1").arg(tmp);
        nextDownload();
        return;
    }
    f.close();
    load(s, tmp); // checked in the background, then it replaces the old list (nextDownload when done)
}

// ------------------------------------------------------------------ online (G7VRD)

void LocatorSources::requestOnline(const QString &call0)
{
    const QString call = call0.trimmed().toUpper();
    if (!m_on[Web] || !looksLikeCall(call) || call.contains(QLatin1Char('/'))) {
        emit onlineResult(call, QString());
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch() / 1000;
    const auto it = m_web.constFind(call);
    if (it != m_web.constEnd()) {
        const qint64 age = now - it->t;
        if ((!it->grid.isEmpty() && age < static_cast<qint64>(kWebDays) * 86400) || (it->grid.isEmpty() && age < 86400)) {
            emit onlineResult(call, it->grid);
            return;
        }
    }
    if (now < m_webBackoff) {
        emit onlineResult(call, QString());
        return;
    }
    if (!m_webCurrent.isEmpty()) {
        m_webNext = call; // only the last one waits
        return;
    }
    webStart(call);
}

void LocatorSources::webStart(const QString &call)
{
    m_webCurrent = call;
    m_webAsked++;
    const QString url = QString::fromLatin1(kWebUrl) + call;
    m_webTimer->start(10000);
    if (QSslSocket::supportsSsl()) {
        QNetworkRequest request{QUrl(url)};
        request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
        request.setRawHeader("Accept", "application/json");
        m_webReply = m_network->get(request);
        connect(m_webReply, &QNetworkReply::finished, this, &LocatorSources::webFinished);
        return;
    }
    const QString curl = QStandardPaths::findExecutable(QStringLiteral("curl"));
    if (curl.isEmpty()) {
        m_webTimer->stop();
        webDone(QByteArray(), 0, uiText("loc_no_https", "HTTPS is not available (no OpenSSL and no curl)"));
        return;
    }
    m_webProcess = new QProcess(this);
    const QStringList args = {QStringLiteral("-s"), QStringLiteral("-S"), QStringLiteral("--max-time"), QStringLiteral("9"),
                              QStringLiteral("-A"), QString::fromLatin1(kUserAgent), QStringLiteral("-H"),
                              QStringLiteral("Accept: application/json"), QStringLiteral("-w"), QStringLiteral("\n%{http_code}"), url};
    connect(m_webProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &LocatorSources::webCurlFinished);
    m_webProcess->start(curl, args);
}

void LocatorSources::webFinished()
{
    auto *r = qobject_cast<QNetworkReply *>(sender());
    if (!r) return;
    r->deleteLater();
    if (r != m_webReply) return;
    m_webReply = nullptr;
    m_webTimer->stop();
    const int http = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray d = r->readAll();
    webDone(d, http, r->error() == QNetworkReply::NoError ? QString() : r->errorString());
}

void LocatorSources::webCurlFinished(int code, QProcess::ExitStatus status)
{
    auto *p = qobject_cast<QProcess *>(sender());
    if (!p) return;
    p->deleteLater();
    if (p != m_webProcess) return;
    m_webProcess = nullptr;
    m_webTimer->stop();
    const QByteArray out = p->readAllStandardOutput();
    const int nl = out.lastIndexOf('\n');
    const int http = nl >= 0 ? out.mid(nl + 1).trimmed().toInt() : 0;
    const QByteArray body = nl >= 0 ? out.left(nl) : out;
    QString err;
    if (status != QProcess::NormalExit || code != 0) {
        err = QString::fromLocal8Bit(p->readAllStandardError()).trimmed();
        if (err.isEmpty()) err = QStringLiteral("curl %1").arg(code);
    }
    webDone(body, http, err);
}

void LocatorSources::webTimeout()
{
    if (m_webReply) {
        QNetworkReply *r = m_webReply;
        m_webReply = nullptr;
        r->abort();
    }
    if (m_webProcess) {
        QProcess *p = m_webProcess;
        m_webProcess = nullptr;
        p->kill();
        p->deleteLater();
    }
    if (!m_webCurrent.isEmpty()) webDone(QByteArray(), 0, uiText("loc_timeout", "no answer (timeout)"));
}

void LocatorSources::webDone(const QByteArray &data, int http, const QString &error)
{
    const QString call = m_webCurrent;
    m_webCurrent.clear();
    const qint64 now = QDateTime::currentMSecsSinceEpoch() / 1000;
    QString grid;
    bool ok = false;
    if (http == 200 && error.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            ok = true;
            const QJsonObject all = doc.object().value(QStringLiteral("locators")).toObject();
            grid = normalizeGrid(all.value(call).toObject().value(QStringLiteral("locator")).toString());
        }
    }
    if (ok) {
        m_webError.clear();
        WebEntry e;
        e.grid = grid;
        e.t = now;
        m_web.insert(call, e);
        saveWebCache();
    } else {
        m_webError = uiText("loc_web_failed", "Online lookup failed: %1")
                         .arg((http > 0 ? QStringLiteral("HTTP %1  ").arg(http) : QString()) + error);
        m_webBackoff = now + (http == 429 ? 900 : 120); // the service asks to slow down: 15 minutes
    }
    emit onlineResult(call, grid);
    emit changed();
    if (!m_webNext.isEmpty()) {
        const QString next = m_webNext;
        m_webNext.clear();
        requestOnline(next);
    }
}

void LocatorSources::readWebCache()
{
    QFile f(filePath(Web));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch() / 1000;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QStringList l = in.readLine().trimmed().split(QLatin1Char(','));
        if (l.count() < 3) continue;
        WebEntry e;
        e.grid = normalizeGrid(l.at(1));
        e.t = l.at(2).toLongLong();
        if (now - e.t > static_cast<qint64>(kWebDays) * 86400) continue;
        if (looksLikeCall(l.at(0))) m_web.insert(l.at(0), e);
    }
}

void LocatorSources::saveWebCache()
{
    if (m_web.size() > kWebMax) { // drop the oldest answers
        QList<qint64> ts;
        for (auto it = m_web.constBegin(); it != m_web.constEnd(); ++it) ts.append(it->t);
        std::sort(ts.begin(), ts.end());
        const qint64 limit = ts.at(ts.size() - kWebMax);
        for (auto it = m_web.begin(); it != m_web.end();) {
            if (it->t < limit) it = m_web.erase(it);
            else ++it;
        }
    }
    QDir().mkpath(m_dir);
    QFile f(filePath(Web));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);
    for (auto it = m_web.constBegin(); it != m_web.constEnd(); ++it) {
        out << it.key() << ',' << it->grid << ',' << it->t << '\n';
    }
}

// ------------------------------------------------------------------ binder

LocatorLookupBinder::LocatorLookupBinder(LocatorSources *sources, QLineEdit *callEdit, QLineEdit *gridEdit,
                                         std::function<double()> periodSeconds, QObject *parent)
    : QObject(parent), m_sources(sources), m_call(callEdit), m_grid(gridEdit), m_period(std::move(periodSeconds))
{
    if (!m_call || !m_grid || !m_sources) return;
    m_badge = new QLabel(m_grid);
    m_badge->setObjectName(QStringLiteral("locatorSourceBadge"));
    m_badge->setStyleSheet(QStringLiteral("color:#7fd7ff;font-size:9pt;font-weight:bold;background:transparent;"));
    m_badge->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_badge->hide();
    m_grid->installEventFilter(this);
    m_lastCall = m_call->text().trimmed().toUpper();
    m_lastGrid = m_grid->text().trimmed().toUpper();
    // MadModem sets the DX call with its signals blocked, so the binder looks at it.
    m_pollTimer.setInterval(400);
    connect(&m_pollTimer, &QTimer::timeout, this, &LocatorLookupBinder::poll);
    m_pollTimer.start();
    m_webTimer.setSingleShot(true);
    m_webTimer.setInterval(1200);
    connect(&m_webTimer, &QTimer::timeout, this, [this]() {
        if (m_call && m_grid && m_grid->text().trimmed().isEmpty() && !m_lastCall.isEmpty()) {
            m_badge->setText(QStringLiteral("WEB..."));
            placeBadge();
            m_badge->show();
            m_sources->requestOnline(m_lastCall);
        }
    });
    connect(m_sources, &LocatorSources::onlineResult, this, [this](const QString &call, const QString &grid) {
        if (!m_call || !m_grid || call != m_lastCall) return;
        if (!grid.isEmpty() && m_grid->text().trimmed().isEmpty()) {
            setGrid(grid, LocatorSources::tag(LocatorSources::Web));
        } else if (m_badge->text() == QLatin1String("WEB...")) {
            m_badge->hide();
        }
        refreshTooltip();
    });
    connect(m_sources, &LocatorSources::listLoaded, this, [this]() { lookup(false); });
}

bool LocatorLookupBinder::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_grid && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        placeBadge();
    }
    return QObject::eventFilter(watched, event);
}

void LocatorLookupBinder::placeBadge()
{
    if (!m_grid || !m_badge) return;
    m_badge->adjustSize();
    const int w = m_badge->width();
    m_badge->move(m_grid->width() - w - 8, (m_grid->height() - m_badge->height()) / 2);
    m_grid->setTextMargins(0, 0, m_badge->isVisible() ? w + 6 : 0, 0);
}

void LocatorLookupBinder::poll()
{
    if (!m_call || !m_grid) return;
    const QString call = m_call->text().trimmed().toUpper();
    const QString grid = m_grid->text().trimmed().toUpper();
    if (call != m_lastCall) {
        // The grid belongs to the new call when it changed together with the call
        // (a decode was clicked) or was typed; it is stale when it still holds the
        // locator seen with the previous call.
        const bool stale = !grid.isEmpty() && grid == m_lastGrid;
        m_lastCall = call;
        m_autoGrid.clear();
        m_webTimer.stop();
        if (stale) {
            m_grid->setText(QString());
        }
        m_badge->hide();
        placeBadge();
        lookup(true);
    } else if (grid != m_lastGrid && grid != m_autoGrid) {
        // edited by the operator or by a decode: no source label
        m_badge->hide();
        placeBadge();
        refreshTooltip();
    } else if (grid.isEmpty() && !call.isEmpty()) {
        lookup(false); // a new decode or list may know it now
    }
    m_lastGrid = m_grid->text().trimmed().toUpper();
}

void LocatorLookupBinder::lookup(bool callChanged)
{
    if (!m_call || !m_grid) return;
    const QString call = m_call->text().trimmed().toUpper();
    if (call.size() < 3) {
        refreshTooltip();
        return;
    }
    if (!m_grid->text().trimmed().isEmpty()) {
        refreshTooltip();
        return;
    }
    int source = -1;
    const QString grid = m_sources->findLocal(call, m_period ? m_period() : 15.0, source);
    if (!grid.isEmpty()) {
        setGrid(grid, LocatorSources::tag(source));
        return;
    }
    refreshTooltip();
    if (callChanged && m_sources->isOn(LocatorSources::Web)) {
        m_webTimer.start(); // 1.2 s after the last change of the call
    }
}

void LocatorLookupBinder::setGrid(const QString &grid, const QString &tag)
{
    if (!m_grid) return;
    m_grid->setText(grid);
    m_autoGrid = grid;
    m_lastGrid = grid;
    m_badge->setText(tag);
    m_badge->show();
    placeBadge();
    refreshTooltip();
}

void LocatorLookupBinder::refreshTooltip()
{
    if (!m_grid || !m_call) return;
    const QString call = m_call->text().trimmed().toUpper();
    const QString text = call.size() >= 3 ? m_sources->tooltip(call, m_period ? m_period() : 15.0) : QString();
    m_badge->setToolTip(text);
    m_grid->setToolTip(text);
}

// ------------------------------------------------------------------ dialog

LocatorSourcesDialog::LocatorSourcesDialog(LocatorSources *sources, QWidget *parent)
    : QDialog(parent), m_sources(sources)
{
    setWindowTitle(uiText("loc_title", "Locator sources"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    auto *box = new QGroupBox(uiText("loc_group", "Locator lookup for the DX call"), this);
    auto *info = new QLabel(uiText("loc_info", "When the DX call has no locator, MadModem looks in these sources, in this order, and fills the DX grid. The small label inside the DX grid field shows where the locator comes from; hold the mouse on it to see what every source says."), box);
    info->setWordWrap(true);
    auto *list = new QVBoxLayout();
    list->setSpacing(1);
    for (int i = 0; i < LocatorSources::Count; ++i) {
        m_check[i] = new QCheckBox(LocatorSources::tag(i) + QStringLiteral(":  ") + LocatorSources::name(i), box);
        m_check[i]->setChecked(sources->isOn(i));
        connect(m_check[i], &QCheckBox::toggled, this, [this, i](bool on) {
            m_sources->setOn(i, on);
            refresh();
        });
        m_status[i] = new QLabel(box);
        m_status[i]->setWordWrap(true);
        m_status[i]->setContentsMargins(24, 0, 0, 6);
        list->addWidget(m_check[i]);
        if (i == LocatorSources::Web) {
            auto *web = new QLabel(uiText("loc_web_info", "The online lookup sends the call to api.g7vrd.co.uk only when no other source has the locator; the answers are kept for 30 days."), box);
            web->setWordWrap(true);
            web->setContentsMargins(24, 0, 0, 2);
            list->addWidget(web);
        }
        list->addWidget(m_status[i]);
    }
    m_auto = new QCheckBox(uiText("loc_auto", "Download the lists and update them every 14 days"), box);
    m_auto->setChecked(sources->autoUpdate());
    connect(m_auto, &QCheckBox::toggled, this, [this](bool on) {
        m_sources->setAutoUpdate(on);
        refresh();
    });
    m_update = new QPushButton(uiText("loc_update_now", "Update Now"), box);
    connect(m_update, &QPushButton::clicked, this, [this]() {
        m_sources->updateNow();
        refresh();
    });
    auto *updateRow = new QHBoxLayout();
    updateRow->addWidget(m_auto);
    updateRow->addStretch(1);
    updateRow->addWidget(m_update);
    auto *links = new QLabel(uiText("loc_sources", "Sources:") + QStringLiteral(" "
        "<a href=\"https://sourceforge.net/projects/wsjt-x-improved/files/Additional%20Files/\">WSJT-X Improved</a>, "
        "<a href=\"https://www.bigskyspaces.com/w7gj/\">W7GJ</a>, "
        "<a href=\"https://sourceforge.net/projects/jtdx/files/\">JTDX</a>, "
        "<a href=\"https://g7vrd.co.uk/public-apis\">G7VRD</a>"), box);
    links->setTextFormat(Qt::RichText);
    links->setTextInteractionFlags(Qt::TextBrowserInteraction);
    links->setOpenExternalLinks(true);
    links->setWordWrap(true);
    auto *folder = new QLabel(uiText("loc_folder", "Folder: %1").arg(QDir::toNativeSeparators(sources->folder())), box);
    folder->setWordWrap(true);
    folder->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *boxLayout = new QVBoxLayout(box);
    boxLayout->addWidget(info);
    boxLayout->addLayout(list);
    boxLayout->addLayout(updateRow);
    boxLayout->addWidget(links);
    boxLayout->addWidget(folder);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    auto *outer = new QVBoxLayout(this);
    outer->addWidget(box);
    outer->addWidget(buttons);
    resize(620, 560);
    connect(sources, &LocatorSources::changed, this, &LocatorSourcesDialog::refresh);
    refresh();
}

void LocatorSourcesDialog::refresh()
{
    for (int i = 0; i < LocatorSources::Count; ++i) {
        m_status[i]->setText(m_sources->status(i));
        m_status[i]->setEnabled(m_sources->isOn(i));
    }
    m_update->setEnabled(!m_sources->busy());
}
