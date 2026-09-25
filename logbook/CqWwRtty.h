#pragma once
#include "AdifLogbook.h"
#include "../dxcc/CtyCountryFile.h"
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace CqWwRtty {
inline QString zoneKey(const QString &value) { return QString::number(value.toInt()); }
inline QString field(const LogbookEntry &e, bool sent, const QString &id) {
    return e.adifFields.value(QStringLiteral("APP_MADMODEM_RTTY_%1_%2").arg(sent ? "TX" : "RX", id)).trimmed().toUpper();
}
inline QDate startDate(int year) {
    QDate d(year, 9, 30);
    while (d.dayOfWeek() != 7) d = d.addDays(-1);
    return d.addDays(-1);
}
inline bool inPeriod(const QDateTime &utc) {
    const auto d = utc.toUTC().date();
    const auto start = startDate(d.year());
    return utc.isValid() && d >= start && d < start.addDays(2);
}
inline QString qthError(const QString &call, const QString &qth) {
    const auto dx = CtyCountryFile::instance().lookupCallsign(call);
    if (!dx.valid || call.endsWith("/MM")) return {};
    const QString prefix = dx.entity.primaryPrefix;
    const QStringList states = QStringLiteral("AL AR AZ CA CO CT DE FL GA IA ID IL IN KS KY LA MA MD ME MI MN MO MS MT NC ND NE NH NJ NM NV NY OH OK OR PA RI SC SD TN TX UT VA VT WA WI WV WY DC").split(' ');
    const QStringList provinces = QStringLiteral("AB BC MB NB NS NWT NF LB NU ON PEI QC SK YT").split(' ');
    if (prefix == "K" && !states.contains(qth)) return "State";
    if (prefix == "VE" && !provinces.contains(qth)) return "Province";
    return {};
}
inline QString validate(const LogbookEntry &e, bool exportCheck = false) {
    static const QRegularExpression callPattern("^(?=.*[A-Z])(?=.*[0-9])[A-Z0-9/]{3,20}$");
    if (!callPattern.match(e.callsign).hasMatch() || !callPattern.match(e.stationCallsign).hasMatch()) return "Callsign";
    if (e.mode != "RTTY") return "Mode";
    if (!QStringList{"80m","40m","20m","15m","10m"}.contains(e.band)) return "Band";
    for (bool sent : {true, false}) {
        const QString zone = field(e, sent, "CQZONE");
        if (!QRegularExpression("^(?:0?[1-9]|[1-3][0-9]|40)$").match(zone).hasMatch()) return sent ? "TX CQ zone" : "RX CQ zone";
        const QString rst = sent ? e.rstSent : e.rstReceived;
        if (!QRegularExpression("^[1-5][1-9][1-9]$").match(rst).hasMatch()) return "RST";
        const QString error = qthError(sent ? e.stationCallsign : e.callsign, field(e,sent,"QTH"));
        if (!error.isEmpty()) return error;
    }
    if (exportCheck && !inPeriod(e.utc)) return "QSO outside CQ WW UTC period";
    {
        const double mhz = e.freq.toDouble();
        const QMap<QString,QPair<double,double>> bands{{"80m",{3.5,4.0}},{"40m",{7.0,7.3}},{"20m",{14.0,14.35}},{"15m",{21.0,21.45}},{"10m",{28.0,29.7}}};
        const auto limits = bands.value(e.band);
        if (!(mhz >= limits.first && mhz <= limits.second)) return "Frequency missing or inconsistent with band";
    }
    return {};
}
// Country identity is specific to CQ WW; leave ADIF DXCC untouched.
inline int points(const LogbookEntry &e) {
    const auto own = CtyCountryFile::instance().lookupCallsign(e.stationCallsign);
    const auto dx = CtyCountryFile::instance().lookupCallsign(e.callsign);
    if (!own.valid || !dx.valid) return 0;
    if (!e.callsign.endsWith("/MM") && own.entity.primaryPrefix == dx.entity.primaryPrefix) return 1;
    return own.entity.continent == dx.entity.continent ? 2 : 3;
}
struct Score {
    int qsos = 0;
    qint64 points = 0;
    QSet<QString> multipliers;
};
// Input belongs to one session. Invalid rows never reserve a duplicate key.
inline Score score(const QVector<LogbookEntry> &records, int year) {
    Score result;
    QSet<QString> worked;
    for (const auto &e : records) {
        if (!validate(e).isEmpty() || !inPeriod(e.utc) || e.utc.toUTC().date().year() != year) continue;
        const auto dx = CtyCountryFile::instance().lookupCallsign(e.callsign);
        if (!dx.valid) continue;
        const QString suffix = "@" + e.band;
        const QString key = e.callsign + suffix;
        if (worked.contains(key)) continue;
        worked.insert(key);
        ++result.qsos;
        result.points += points(e);
        result.multipliers.insert("zone:" + zoneKey(field(e,false,"CQZONE")) + suffix);
        if (!e.callsign.endsWith("/MM")) {
            result.multipliers.insert("country:" + dx.entity.primaryPrefix + suffix);
            if (dx.entity.primaryPrefix == "K" || dx.entity.primaryPrefix == "VE")
                result.multipliers.insert("qth:" + field(e,false,"QTH") + suffix);
        }
    }
    return result;
}
struct CabrilloOptions {
    QString operatorCategory = "SINGLE-OP", power = "LOW", assisted = "ASSISTED", band = "ALL";
    QString name, email;
};
inline QByteArray cabrillo(QVector<LogbookEntry> records, const CabrilloOptions &o, QString *error) {
    auto fail = [&](const QString &why) { if(error) *error = why; return QByteArray(); };
    if (records.isEmpty()) return fail("No QSOs selected");
    const auto &first = records.first();
    const QString session = first.adifFields.value("APP_MADMODEM_RTTY_SESSION");
    const QString own = first.stationCallsign;
    const int year = first.utc.toUTC().date().year();
    if (session.isEmpty()) return fail("Select one contest session");
    if (!QStringList{"SINGLE-OP","CHECKLOG"}.contains(o.operatorCategory) ||
        !QStringList{"HIGH","LOW","QRP"}.contains(o.power) ||
        !QStringList{"ASSISTED","NON-ASSISTED"}.contains(o.assisted) ||
        !QStringList{"ALL","80M","40M","20M","15M","10M"}.contains(o.band)) return fail("Invalid category");
    const QString location = field(first,true,"QTH").isEmpty() ? QStringLiteral("DX") : field(first,true,"QTH");
    auto clean = [](QString v) { return v.replace('\r',' ').replace('\n',' ').simplified(); };
    QString text = "START-OF-LOG: 3.0\nCONTEST: CQ-WW-RTTY\nCALLSIGN: " + own + "\nLOCATION: " + location +
        "\nCATEGORY-OPERATOR: " + o.operatorCategory + "\nCATEGORY-POWER: " + o.power + "\nCATEGORY-ASSISTED: " + o.assisted +
        "\nCATEGORY-BAND: " + o.band + "\nCATEGORY-MODE: RTTY\nCATEGORY-STATION: FIXED\nCREATED-BY: MadModem R7\nNAME: " + clean(o.name) + "\nEMAIL: " + clean(o.email) + "\n";
    std::sort(records.begin(),records.end(),[](const LogbookEntry&a,const LogbookEntry&b){return a.utc<b.utc;});
    for (const auto &e : records) {
        if (e.adifFields.value("CONTEST_ID") != "CQ-WW-RTTY" || e.adifFields.value("APP_MADMODEM_RTTY_SESSION") != session || e.stationCallsign != own || e.utc.toUTC().date().year() != year)
            return fail("Select QSOs from one CQ WW session, station and edition");
        const QString why = validate(e,true);
        if (!why.isEmpty()) return fail(e.callsign + ": " + why);
        // Include off-category-band contacts as required by CQ WW log instructions.
        QStringList row{"QSO:",QString::number(qRound64(e.freq.toDouble()*1000.0)),"RY",e.utc.toUTC().toString("yyyy-MM-dd"),e.utc.toUTC().toString("HHmm"),own,e.rstSent,
            QString::number(field(e,true,"CQZONE").toInt()).rightJustified(2,'0'),field(e,true,"QTH").isEmpty()?QStringLiteral("DX"):field(e,true,"QTH"),e.callsign,e.rstReceived,
            QString::number(field(e,false,"CQZONE").toInt()).rightJustified(2,'0'),field(e,false,"QTH").isEmpty()?QStringLiteral("DX"):field(e,false,"QTH")};
        text += row.join(' ') + '\n';
    }
    text += "END-OF-LOG:\n";
    return text.toUtf8();
}
}
