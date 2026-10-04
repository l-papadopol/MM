#pragma once
#include "AdifLogbook.h"
#include <algorithm>

namespace GenericCabrillo {
struct Options {
    QString contestId;
    QString categoryOperator = "SINGLE-OP";
    QString categoryBand = "ALL";
    QString categoryPower = "LOW";
    QString categoryAssisted = "NON-ASSISTED";
    QString sentExchangeField = "RST_SENT";
    QString receivedExchangeField = "RST_RCVD";
    QString name;
    QString email;
};
inline QString clean(QString s) { return s.replace('\r',' ').replace('\n',' ').simplified(); }
inline QString value(const LogbookEntry &e, const QString &field, bool sent) {
    const QString k=field.trimmed().toUpper();
    if(k=="RST_SENT") return e.rstSent;
    if(k=="RST_RCVD") return e.rstReceived;
    if(k=="CALL") return e.callsign;
    if(k=="STATION_CALLSIGN") return e.stationCallsign;
    if(k=="SERIAL_SENT") return e.adifFields.value("STX");
    if(k=="SERIAL_RCVD") return e.adifFields.value("SRX");
    if(k=="EXCH_SENT") return e.adifFields.value("STX_STRING");
    if(k=="EXCH_RCVD") return e.adifFields.value("SRX_STRING");
    Q_UNUSED(sent)
    return e.adifFields.value(k);
}
inline QByteArray cabrillo(QVector<LogbookEntry> records,const Options&o,QString*error) {
    auto fail=[&](const QString&s){if(error)*error=s;return QByteArray();};
    if(records.isEmpty()) return fail("No QSOs selected");
    const QString contest=clean(o.contestId).toUpper(); if(contest.isEmpty()) return fail("CONTEST is required");
    QString own=records.first().stationCallsign.trimmed().toUpper(); if(own.isEmpty()) return fail("STATION_CALLSIGN is missing");
    for(const auto&e:records) if(e.stationCallsign.trimmed().toUpper()!=own) return fail("Selected QSOs contain more than one station callsign");
    std::sort(records.begin(),records.end(),[](const LogbookEntry&a,const LogbookEntry&b){return a.utc<b.utc;});
    QString out="START-OF-LOG: 3.0\nCREATED-BY: MadModem 0.5.9-beta\nCONTEST: "+contest+"\nCALLSIGN: "+own+
      "\nCATEGORY-OPERATOR: "+o.categoryOperator+"\nCATEGORY-BAND: "+o.categoryBand+"\nCATEGORY-POWER: "+o.categoryPower+
      "\nCATEGORY-ASSISTED: "+o.categoryAssisted+"\nNAME: "+clean(o.name)+"\nEMAIL: "+clean(o.email)+"\n";
    for(const auto&e:records){
        if(!e.utc.isValid()||e.callsign.trimmed().isEmpty()) return fail("A selected QSO has no valid UTC time or callsign");
        bool ok=false; double mhz=e.freq.toDouble(&ok); if(!ok) return fail(e.callsign+": frequency missing");
        QString mode=e.mode.trimmed().toUpper(); if(mode=="RTTY") mode="RY"; else if(mode=="SSB"||mode=="USB"||mode=="LSB") mode="PH"; else if(mode.isEmpty()) mode="RY";
        QString sx=clean(value(e,o.sentExchangeField,true)); QString rx=clean(value(e,o.receivedExchangeField,false));
        if(sx.isEmpty()) return fail(e.callsign+": sent exchange field "+o.sentExchangeField+" is empty");
        if(rx.isEmpty()) return fail(e.callsign+": received exchange field "+o.receivedExchangeField+" is empty");
        out += QString("QSO: %1 %2 %3 %4 %5 %6 %7 %8 %9\n")
            .arg(qRound64(mhz*1000.0),5,10,QLatin1Char(' ')).arg(mode,-2).arg(e.utc.toUTC().toString("yyyy-MM-dd"))
            .arg(e.utc.toUTC().toString("HHmm")).arg(own,-13).arg(sx,-12).arg(e.callsign.trimmed().toUpper(),-13).arg(rx,-12).arg(QString());
    }
    out += "END-OF-LOG:\n"; return out.toUtf8();
}
}
