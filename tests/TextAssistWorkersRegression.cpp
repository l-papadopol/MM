#include "runtime/LogbookIndexWorker.h"
#include "runtime/TextAssistWorker.h"
#include "logbook/AdifLogbook.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>

#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    auto check = [&](bool result, const char *name) {
        std::cout << (result ? "PASS " : "FAIL ") << name << '\n';
        ok &= result;
    };

    QTemporaryDir dir;
    check(dir.isValid(), "temporary directory");
    const QString logPath = dir.filePath(QStringLiteral("assist.adi"));
    AdifLogbook logbook(logPath);
    check(logbook.load(), "load empty logbook");

    const QString sessionKey = QStringLiteral("APP_MADMODEM_RTTY_SESSION");
    const QString ruleKey = QStringLiteral("APP_MADMODEM_RTTY_RULE");

    LogbookEntry first;
    first.callsign = QStringLiteral("K1ABC");
    first.band = QStringLiteral("20m");
    first.mode = QStringLiteral("RTTY");
    first.grid = QStringLiteral("FN31AA");
    first.utc = QDateTime::fromString(QStringLiteral("2026-09-27T12:00:00Z"), Qt::ISODate);
    first.adifFields.insert(QStringLiteral("DXCC"), QStringLiteral("291"));
    first.adifFields.insert(sessionKey, QStringLiteral("session-1"));
    first.adifFields.insert(ruleKey, QStringLiteral("cqww-rtty"));
    first.adifFields.insert(QStringLiteral("CONTEST_ID"), QStringLiteral("CQ-WW-RTTY"));
    check(logbook.append(first), "append indexed contest QSO");

    LogbookIndexWorker::ContestConfig config;
    config.active = true;
    config.mode = QStringLiteral("RTTY");
    config.sessionId = QStringLiteral("session-1");
    config.ruleId = QStringLiteral("cqww-rtty");
    config.cabrilloId = QStringLiteral("CQ-WW-RTTY");
    config.sessionAdifKey = sessionKey;
    config.ruleAdifKey = ruleKey;
    config.dupeScope = QStringLiteral("band");

    LogbookIndexWorker index;
    QStringList lastWorked;
    QObject::connect(&index, &LogbookIndexWorker::lookupReady,
                     [&](quint64, const QString &, const QStringList &worked) {
                         lastWorked = worked;
                     });
    index.rebuildFromFile(logPath, config);

    index.lookup(1, QStringLiteral("RTTY"), {QStringLiteral("K1ABC")}, false, QString(), QString());
    check(lastWorked.contains(QStringLiteral("K1ABC")), "global worked-before hash lookup");

    lastWorked.clear();
    index.lookup(2, QStringLiteral("RTTY"), {QStringLiteral("K1ABC")}, true,
                 QStringLiteral("20m"), QString());
    check(lastWorked.contains(QStringLiteral("K1ABC")), "contest dupe on matching band");

    lastWorked.clear();
    index.lookup(3, QStringLiteral("RTTY"), {QStringLiteral("K1ABC")}, true,
                 QStringLiteral("40m"), QString());
    check(lastWorked.isEmpty(), "contest dupe scope rejects another band");

    LogbookEntry second = first;
    second.callsign = QStringLiteral("DL1XYZ");
    second.band = QStringLiteral("20m");
    second.utc = first.utc.addSecs(60);
    index.addEntry(second);
    lastWorked.clear();
    index.lookup(4, QStringLiteral("RTTY"), {QStringLiteral("DL1XYZ")}, true,
                 QStringLiteral("20m"), QString());
    check(lastWorked.contains(QStringLiteral("DL1XYZ")), "incremental index update");

    QVariantList ftResults;
    QObject::connect(&index, &LogbookIndexWorker::ftLookupReady,
                     [&](quint64, const QString &, const QVariantList &results) { ftResults = results; });
    QVariantMap ftQuery;
    ftQuery.insert(QStringLiteral("cacheKey"), QStringLiteral("ft-k1abc"));
    ftQuery.insert(QStringLiteral("generation"), 1);
    ftQuery.insert(QStringLiteral("call"), QStringLiteral("K1ABC"));
    ftQuery.insert(QStringLiteral("band"), QStringLiteral("20m"));
    ftQuery.insert(QStringLiteral("mode"), QStringLiteral("RTTY"));
    ftQuery.insert(QStringLiteral("dxcc"), QStringLiteral("291"));
    ftQuery.insert(QStringLiteral("grid4"), QStringLiteral("FN31"));
    ftQuery.insert(QStringLiteral("recentHours"), 24 * 365);
    ftQuery.insert(QStringLiteral("recentBandModeMinutes"), 24 * 60 * 30);
    ftQuery.insert(QStringLiteral("referenceUtcMs"), first.utc.toMSecsSinceEpoch());
    index.lookupFt(5, QStringLiteral("FT_CACHE"), QVariantList{ftQuery});
    check(ftResults.size() == 1, "FT batch lookup returns one result");
    const QVariantMap ftStatus = ftResults.value(0).toMap();
    check(ftStatus.value(QStringLiteral("worked")).toBool(), "FT worked-call index");
    check(ftStatus.value(QStringLiteral("recentWorked")).toBool(), "FT recent-call index");
    check(ftStatus.value(QStringLiteral("latestCallUtc")).toDateTime() == first.utc,
          "FT cache receives timestamp for time-dependent policy refresh");
    check(ftStatus.value(QStringLiteral("recentBandMode")).toBool(), "FT recent call-band-mode index");
    check(ftStatus.value(QStringLiteral("countryWorkedAny")).toBool(), "FT DXCC any-band index");
    check(ftStatus.value(QStringLiteral("countryWorkedBand")).toBool(), "FT DXCC band index");
    check(ftStatus.value(QStringLiteral("countryWorkedMode")).toBool(), "FT DXCC mode index");
    check(ftStatus.value(QStringLiteral("gridWorkedAny")).toBool(), "FT grid any-band index");
    check(ftStatus.value(QStringLiteral("gridWorkedBand")).toBool(), "FT grid band index");
    check(ftStatus.value(QStringLiteral("gridWorkedMode")).toBool(), "FT grid mode index");

    TextAssistWorker assist;
    QVariantList annotations;
    QVariantMap candidate;
    QVariantList heard;
    QObject::connect(&assist, &TextAssistWorker::analysisReady,
                     [&](quint64, const QString &, const QVariantList &a,
                         const QVariantMap &c, const QVariantList &h) {
                         annotations = a;
                         candidate = c;
                         heard = h;
                     });

    RttyContestProfile profile;
    profile.id = QStringLiteral("cqww-rtty");
    RttyContestFieldRule rst;
    rst.id = QStringLiteral("RST");
    rst.type = QStringLiteral("rst");
    rst.regex = QStringLiteral("^[1-5][1-9][1-9]$");
    RttyContestFieldRule serial;
    serial.id = QStringLiteral("SERIAL");
    serial.type = QStringLiteral("serial");
    serial.regex = QStringLiteral("^\\d{1,6}$");
    profile.receivedFields = {rst, serial};

    assist.analyze(10, QStringLiteral("RTTY"),
                   QStringLiteral("CQ K1ABC IZ6NNH 599 123 JN61AA"),
                   100, QStringLiteral("RTTY"), QStringLiteral("IZ6NNH"), true, profile);

    bool sawCall = false;
    bool absoluteOffsetOk = false;
    for (const QVariant &item : annotations) {
        const QVariantMap a = item.toMap();
        if (a.value(QStringLiteral("kind")).toString() == QStringLiteral("call") &&
            a.value(QStringLiteral("value")).toString() == QStringLiteral("K1ABC")) {
            sawCall = true;
            absoluteOffsetOk = a.value(QStringLiteral("start")).toInt() >= 100;
        }
    }
    check(sawCall, "text worker recognizes callsign");
    check(absoluteOffsetOk, "text worker preserves document offsets");
    check(candidate.value(QStringLiteral("dxCall")).toString() == QStringLiteral("K1ABC"),
          "contest candidate selects correspondent");
    const QVariantMap fields = candidate.value(QStringLiteral("fields")).toMap();
    check(fields.value(QStringLiteral("RST")).toString() == QStringLiteral("599"),
          "contest parser extracts RST");
    check(fields.value(QStringLiteral("SERIAL")).toString() == QStringLiteral("123"),
          "contest parser extracts serial after RST");

    return ok ? 0 : 1;
}
