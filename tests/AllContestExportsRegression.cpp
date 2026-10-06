#include "logbook/ContestExport.h"
#include <QCoreApplication>
#include <QDir>
#include <iostream>
#include <stdexcept>
using namespace ContestExport;
void check(bool value,const QString& message) {
    if(!value) throw std::runtime_error(message.toStdString());
    std::cout<<"PASS "<<message.toStdString()<<'\n';
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        const QDir dir(QCoreApplication::applicationDirPath());
        const auto profiles=loadProfiles({dir.filePath("rtty_rules"),dir.filePath("cw_rules")});
        // Independent expected exchange order for each shipped profile.
        const QMap<QString,QPair<QString,QString>> expected{
            {"sartg_new_year",{"599 001 LUCIAN BUON ANNO","599 002 BOB HAPPY NEW YEAR"}},
            {"arrl_rtty_roundup",{"599 001","599 CA"}},
            {"bartg_sprint",{"001","002"}}, {"mexico_rtty",{"599 001","599 002"}},
            {"cq_wpx_rtty",{"599 001","599 002"}}, {"naqp_rtty",{"LUCIAN","BOB CA"}},
            {"yb_dx_rtty",{"599 001","599 002"}}, {"na_sprint_rtty",{"001 LUCIAN DX","002 BOB CA"}},
            {"bartg_hf",{"599 001 1200","599 002 1201"}}, {"ea_rtty",{"599 001","599 002"}},
            {"sp_dx_rtty",{"599 001","599 002"}}, {"bartg_sprint75",{"001","002"}},
            {"ari_dx_rtty",{"599 RN","599 002"}}, {"volta_rtty",{"599 001 15","599 002 03"}},
            {"sartg_ww",{"599 001","599 002"}}, {"rttyops_scry",{"599 2000","599 1995"}},
            {"russian_ww_rtty",{"599 001","599 002"}}, {"cq_ww_rtty",{"599 15","599 03 CA"}},
            {"urc_dx_rtty",{"599 ITA","599 USA"}}, {"makrothen",{"JN63","CM87"}},
            {"jarl_ww_rtty",{"599 40","599 45"}}, {"waedc_rtty",{"599 001","599 002"}},
            {"ok_dx_rtty",{"599 15","599 03"}}, {"weekly_rtty_test",{"LUCIAN DX","BOB CA"}},
            {"arrl_rookie_roundup_rtty",{"LUCIAN 20 DX","BOB 21 CA"}},
            {"ig_ry_ww_rtty",{"599 2000","599 1995"}}, {"darc_rtty_sprint",{"599 001","599 002"}},
            {"r3a_cup_digi_rtty",{"599 001","599 002"}}, {"paccdigi_rtty",{"599 001","599 002"}},
            {"open_ukraine_rtty",{"599 001","599 002"}}, {"ari_40_80_cw",{"599 RN","599 CA"}},
            {"ari_international_cw",{"599 RN","599 002"}}, {"cq_ww_cw",{"599 15","599 03"}},
            {"cq_wpx_cw",{"599 001","599 002"}}, {"arrl_dx_cw",{"599 100","599 CA"}},
            {"iaru_hf_cw",{"599 28","599 06"}}, {"wae_dx_cw",{"599 001","599 002"}},
            {"marconi_144_cw",{"599 001 JN63HX","599 002 CM87UX"}}
        };
        const QMap<QString,QPair<QString,QString>> values{
            {"RST",{"599","599"}}, {"SERIAL",{"001","002"}}, {"CQZONE",{"15","03"}}, {"ZONE",{"15","03"}},
            {"NAME",{"LUCIAN","BOB"}}, {"HNY",{"BUON ANNO","HAPPY NEW YEAR"}}, {"LOCATION",{"DX","CA"}},
            {"STATE",{"RN","CA"}}, {"PROVINCE",{"RN","CA"}}, {"POVIAT",{"AAA","BBB"}},
            {"OBLAST",{"AA","BB"}}, {"QTH",{"DX","CA"}}, {"TERRITORY",{"ITA","USA"}},
            {"GRID",{"JN63HX","CM87UX"}}, {"AGE",{"40","45"}}, {"YEAR",{"2000","1995"}},
            {"DOK",{"A01","A02"}}, {"AREA",{"AA","BB"}}, {"EXCHANGE",{"100","CA"}},
            {"ZONEHQ",{"28","06"}}, {"TIME",{"1200","1201"}}
        };
        QMap<QString,LogbookEntry> examples;
        int covered=0;
        for(const QString& fileName:{QStringLiteral("rtty_rules"),QStringLiteral("cw_rules")}) {
            QFile file(dir.filePath(fileName)); check(file.open(QIODevice::ReadOnly),"load "+fileName);
            const auto definitions=QJsonDocument::fromJson(file.readAll()).object().value("profiles").toArray();
            const QString mode=fileName=="cw_rules"?"CW":"RTTY";
            for(const auto& value:definitions) {
                const auto definition=value.toObject(); const auto key=definition.value("id").toString();
                check(expected.contains(key),"expected fixture exists: "+key);
                const auto profile=profileFor(profiles,definition.value("cabrillo_id").toString(),mode);
                check(profile.definition.value("id").toString()==key,"mode-specific profile resolved: "+key);
                LogbookEntry e; e.callsign="K6ABC"; e.stationCallsign="IZ6NNH"; e.mode=mode;
                e.rstSent=e.rstReceived="599"; e.band="20m"; e.freq="14.085";
                if(key=="ari_40_80_cw" || key=="r3a_cup_digi_rtty") {e.band="40m";e.freq="7.040";}
                if(key=="marconi_144_cw") {e.band="2m";e.freq="144.050";}
                const auto bands=definition.value("bands").toArray();
                if(!bands.isEmpty() && !bands.contains(e.band)) {
                    e.band=bands.first().toString();
                    const QMap<QString,QString> frequencies{{"160m","1.830"},{"80m","3.550"},{"40m","7.040"},{"20m","14.085"},{"15m","21.085"},{"10m","28.085"},{"2m","144.050"}};
                    e.freq=frequencies.value(e.band);
                }
                e.utc=QDateTime(QDate(2026,4,11),QTime(12,0),Qt::UTC);
                if(profile.id=="CQ-WW-RTTY") e.utc=QDateTime(CqWwRtty::startDate(2026),QTime(12,0),Qt::UTC);
                const auto periods=definition.value("periods").toArray();
                if(!periods.isEmpty()) e.utc=QDateTime::fromString(periods.first().toObject().value("start_utc").toString(),Qt::ISODate);
                e.adifFields["CONTEST_ID"]=profile.id;
                e.adifFields["APP_MADMODEM_"+mode+"_RULE"]=key;
                e.adifFields["APP_MADMODEM_"+mode+"_SESSION"]="first";
                for(auto it=values.begin();it!=values.end();++it) {
                    e.adifFields["APP_MADMODEM_"+mode+"_TX_"+it.key()]=it.value().first;
                    e.adifFields["APP_MADMODEM_"+mode+"_RX_"+it.key()]=it.value().second;
                }
                if(key=="arrl_rookie_roundup_rtty") {
                    e.adifFields["APP_MADMODEM_RTTY_TX_YEAR"]="20";e.adifFields["APP_MADMODEM_RTTY_RX_YEAR"]="21";
                }
                QString error;
                const auto tx=automaticExchange(e,profile,true,&error),rx=automaticExchange(e,profile,false,&error);
                check(tx==expected[key].first && rx==expected[key].second && error.isEmpty(),"exact internal exchanges: "+key+" ["+tx+" / "+rx+"]");
                auto restarted=e; restarted.utc=e.utc.addSecs(60); restarted.adifFields["APP_MADMODEM_"+mode+"_SESSION"]="restarted";
                const auto bytes=cabrillo({restarted,e},profiles,{},true,&error);
                check(!bytes.isEmpty() && bytes.count("QSO:")==2,"Cabrillo across sessions: "+key+" "+error);
                auto raw=e; raw.adifFields.clear(); raw.adifFields["CONTEST_ID"]=profile.id;
                raw.adifFields["STX_STRING"]=tx; raw.adifFields["SRX_STRING"]=rx;
                check(automaticExchange(raw,profile,true,&error)==tx && automaticExchange(raw,profile,false,&error)==rx,
                      "ADIF exchange strings: "+key+" "+error);
                auto prior=e; prior.utc=QDateTime(e.utc.date().addYears(-1),e.utc.time(),Qt::UTC);
                if(profile.id=="CQ-WW-RTTY") prior.utc=QDateTime(CqWwRtty::startDate(2025),QTime(12,0),Qt::UTC);
                check(discover({e,restarted,prior},profiles).editions.size()==2,"separate years: "+key);
                examples.insert(key,e); ++covered;
            }
        }
        check(covered==38,"all 30 RTTY and 8 CW profiles covered");
        auto wpx=examples["cq_wpx_rtty"]; wpx.adifFields={{"CONTEST_ID","CQ-WPX-RTTY"},{"STX","001"},{"SRX","002"}};
        QString error;
        check(!cabrillo({wpx},profiles,{},true,&error).isEmpty(),"WPX uses standard STX/SRX plus RST");
        auto reportLikeSerial=wpx; reportLikeSerial.adifFields.remove("SRX");
        reportLikeSerial.adifFields["SRX_STRING"]="599";
        check(automaticExchange(reportLikeSerial,profileFor(profiles,"CQ-WPX-RTTY","RTTY"),false,&error)=="599 599",
              "exchange-only serial 599 is not consumed as an RST");
        auto untagged=wpx; untagged.adifFields.remove("CONTEST_ID"); untagged.utc=untagged.utc.addSecs(30);
        auto merged=discover({wpx,untagged},profiles);
        check(merged.editions.size()==1 && merged.editions[0].records.size()==2 && merged.editions[0].inferred.count(true)==1,"non-CQ untagged QSO recognized from observed edition and exchange");
        auto other=wpx;other.adifFields["CONTEST_ID"]="SARTG-WW-RTTY";
        auto ambiguous=discover({wpx,other,untagged},profiles);
        check(ambiguous.ambiguous==1 && ambiguous.editions.size()==2,"overlapping serial contests remain ambiguous, not duplicated");
        auto mixedCw=examples["ari_international_cw"],mixedRtty=examples["ari_dx_rtty"];
        const auto mixed=cabrillo({mixedCw,mixedRtty},profiles,{},true,&error);
        check(mixed.contains("CATEGORY-MODE: MIXED") && mixed.count("QSO:")==2,"ARI-DX CW/RTTY profiles coexist in one mixed-mode entry");
        auto sprint=examples["na_sprint_rtty"];sprint.adifFields.clear();
        sprint.adifFields["STX_STRING"]="K6ABC IZ6NNH 001 LUCIAN DX";
        sprint.adifFields["SRX_STRING"]="IZ6NNH 002 BOB CA K6ABC";
        check(automaticExchange(sprint,profiles["NA_SPRINT_RTTY"],false,&error)=="002 BOB CA","Sprint does not duplicate leading/trailing calls in exchange columns");
        auto scry=examples["rttyops_scry"],scry2=scry;scry2.utc=QDateTime(QDate(2026,8,30),QTime(13,0),Qt::UTC);
        check(discover({scry,scry2},profiles).editions.size()==2,"independent SCRY periods remain separate");
        auto tour=examples["r3a_cup_digi_rtty"],tour2=tour;tour2.utc=tour.utc.addSecs(7200);
        check(discover({tour,tour2},profiles).editions.size()==1,"R3A tours belong to the same edition");
        auto weekly=examples["weekly_rtty_test"],week2=weekly;week2.utc=weekly.utc.addDays(7);
        check(discover({weekly,week2},profiles).editions.size()==2,"weekly editions separated despite same station/session");
        auto missing=wpx;missing.adifFields.remove("SRX");
        check(cabrillo({missing},profiles,{},true,&error).isEmpty(),"missing exchange blocks all-contest writer");
    } catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n';return 1; }
    return 0;
}
