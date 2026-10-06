#pragma once
#include "CqWwRtty.h"
#include "ContestExchange.h"
#include "GenericCabrillo.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace ContestExport {
inline QString normalizedId(QString value) {
    value = value.trimmed().toUpper();
    QString compact = value;
    compact.remove(QRegularExpression("[^A-Z0-9]"));
    if (compact == "CQWWRTTY" || compact == "CQWWRTTYDX") return "CQ-WW-RTTY";
    return value;
}
inline Profiles loadProfiles(const QStringList& paths) {
    Profiles profiles;
    for (const auto& path : paths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) continue;
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        for (const auto& value : root.value("profiles").toArray()) {
            const auto object = value.toObject();
            const QString mode=QFileInfo(path).fileName().startsWith("cw_") ? "CW" : "RTTY";
            const Identity item{normalizedId(object.value("cabrillo_id").toString()), object.value("name").toString(), mode, object};
            if (item.id.isEmpty()) continue;
            profiles.insert(object.value("id").toString().toUpper(), item);
            profiles.insert(mode+"|"+item.id, item);
            if (!profiles.contains(item.id)) profiles.insert(item.id, item);
        }
    }
    if (!profiles.contains("CQ-WW-RTTY")) {
        const Identity cq{"CQ-WW-RTTY", "CQ WW RTTY", "RTTY", {}};
        profiles.insert("CQ_WW_RTTY",cq); profiles.insert("CQ-WW-RTTY",cq);
    }
    return profiles;
}
inline Identity profileFor(const Profiles& profiles, const QString& id, const QString& mode) {
    const QString normalized=normalizedId(id);
    const QString key=mode.trimmed().toUpper()+"|"+normalized;
    if(profiles.contains(key)) return profiles.value(key);
    const auto direct=profiles.constFind(normalized);
    if(direct!=profiles.constEnd() && (direct->mode.isEmpty() || direct->mode.compare(mode,Qt::CaseInsensitive)==0)) return *direct;
    return {normalized,normalized,mode,{}};
}
struct Window { QDateTime start,end; QString id; };
inline Window windowFor(const Identity& profile, const QDateTime& utc, bool* scheduled=nullptr) {
    if(scheduled) *scheduled=false;
    const int year=utc.toUTC().date().year();
    const auto settings=profile.definition.value("export").toObject();
    const auto calendar=settings.value("calendar").toObject();
    // Legacy CQ logs remain usable even if the external rules are unavailable.
    const int month=calendar.value("month").toInt(profile.id=="CQ-WW-RTTY"?9:0);
    if(month && (calendar.value("last_full_weekend").toBool() || profile.id=="CQ-WW-RTTY")) {
        QDate last(year,month,1); last=last.addMonths(1).addDays(-1);
        while(last.dayOfWeek()!=7) last=last.addDays(-1);
        const QDateTime start(last.addDays(-1),QTime(0,0),Qt::UTC);
        const auto end=start.addSecs(qint64(calendar.value("duration_hours").toInt(48))*3600-1);
        if(scheduled) *scheduled=true;
        return utc>=start && utc<=end ? Window{start,end,start.toString(Qt::ISODate)} : Window{};
    }
    Window combined;
    bool included=false;
    for(const auto& value:profile.definition.value("periods").toArray()) {
        const auto period=value.toObject();
        const auto start=QDateTime::fromString(period.value("start_utc").toString(),Qt::ISODate).toUTC();
        const auto end=QDateTime::fromString(period.value("end_utc").toString(),Qt::ISODate).toUTC();
        if(!start.isValid() || !end.isValid() || end<start || (start.date().year()!=year && end.date().year()!=year)) continue;
        if(scheduled) *scheduled=true;
        if(utc>=start && utc<=end) {
            included=true;
            if(settings.value("separate_periods").toBool()) return {start,end,period.value("id").toString()+start.toString(Qt::ISODate)};
        }
        if(!combined.start.isValid() || start<combined.start) combined.start=start;
        if(!combined.end.isValid() || end>combined.end) combined.end=end;
    }
    combined.id=combined.start.toString(Qt::ISODate);
    return included?combined:Window{};
}
struct Edition {
    QString contestId, name, station;
    QDate firstDate, lastDate;
    Window window;
    QSet<QString> modes;
    QVector<LogbookEntry> records;
    QVector<bool> inferred;
};
struct Catalog {
    QVector<Edition> editions;
    int invalidDates=0, outsidePeriod=0, ambiguous=0;
};
inline Catalog discover(const QVector<LogbookEntry>& records, const Profiles& profiles = {}) {
    struct Candidate { LogbookEntry entry; Identity identity; Window window; bool inferred; };
    QVector<Candidate> candidates;
    QVector<LogbookEntry> unassigned;
    Catalog result;
    auto fitsExchange=[](const LogbookEntry& e,const Identity& profile) {
        if(profile.definition.isEmpty()) {
            return profile.id=="CQ-WW-RTTY" && e.mode.trimmed().compare("RTTY",Qt::CaseInsensitive)==0 &&
                !CqWwRtty::field(e,true,"CQZONE").isEmpty() && !CqWwRtty::field(e,false,"CQZONE").isEmpty();
        }
        QString error;
        if(automaticExchange(e,profile,true,&error).isEmpty() || !error.isEmpty()) return false;
        return !automaticExchange(e,profile,false,&error).isEmpty() && error.isEmpty();
    };
    for(const auto& record:records) {
        const QString mode=record.mode.trimmed().toUpper();
        QString id=normalizedId(record.adifFields.value("CONTEST_ID"));
        const QString rule=record.adifFields.value(mode=="CW"?"APP_MADMODEM_CW_RULE":"APP_MADMODEM_RTTY_RULE").trimmed().toUpper();
        if(profiles.contains(rule)) {
            if(!id.isEmpty() && id!=profiles.value(rule).id) { ++result.ambiguous; continue; }
            id=profiles.value(rule).id;
        } else if(id.isEmpty() && !rule.isEmpty()) id=normalizedId(rule);
        if(!record.utc.isValid()) { if(!id.isEmpty()) ++result.invalidDates; continue; }
        if(id.isEmpty()) { unassigned.push_back(record); continue; }
        auto identity=profileFor(profiles,id,mode);
        bool scheduled=false; const auto window=windowFor(identity,record.utc,&scheduled);
        if(scheduled && !window.start.isValid()) { ++result.outsidePeriod; continue; }
        candidates.push_back({record,identity,window,false});
    }
    // Infer without a tag only when an actual schedule and exchange agree.
    QVector<LogbookEntry> stillUnassigned;
    for(const auto& record:unassigned) {
        QMap<QString,Identity> matches;
        for(const auto& profile:profiles) {
            if(profile.mode.compare(record.mode,Qt::CaseInsensitive)!=0) continue;
            if(windowFor(profile,record.utc).start.isValid() && fitsExchange(record,profile)) matches.insert(profile.id,profile);
        }
        if(profiles.isEmpty() && CqWwRtty::inPeriod(record.utc)) {
            Identity cq{"CQ-WW-RTTY","CQ WW RTTY","RTTY",{}};
            if(fitsExchange(record,cq)) matches.insert(cq.id,cq);
        }
        if(matches.size()==1) {
            const auto identity=matches.first();
            candidates.push_back({record,identity,windowFor(identity,record.utc),true});
        } else if(matches.size()>1) ++result.ambiguous;
        else stillUnassigned.push_back(record);
    }
    std::sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b){return a.entry.utc<b.entry.utc;});
    QMap<QString,int> latest;
    for(const auto& candidate:candidates) {
        const auto& record=candidate.entry; const auto date=record.utc.toUTC().date();
        const QString station=record.stationCallsign.trimmed().toUpper();
        const QString key=candidate.identity.id+'|'+station+'|'+candidate.window.id;
        int index=latest.value(key,-1);
        const bool scheduled=candidate.window.start.isValid();
        if(index<0 || (!scheduled && (result.editions[index].lastDate.daysTo(date)>=7 || result.editions[index].firstDate.year()!=date.year()))) {
            Edition edition; edition.contestId=candidate.identity.id; edition.name=candidate.identity.name; edition.station=station;
            edition.window=candidate.window;
            edition.firstDate=scheduled?candidate.window.start.date():date;
            edition.lastDate=scheduled?candidate.window.end.date():date;
            result.editions.push_back(edition); index=result.editions.size()-1; latest.insert(key,index);
        }
        auto& edition=result.editions[index];
        if(!scheduled) edition.lastDate=date;
        edition.modes.insert(record.mode.trimmed().toUpper());
        if(edition.modes.size()>1) edition.name=edition.contestId;
        edition.records.push_back(record); edition.inferred.push_back(candidate.inferred);
    }
    // With no calendar, tagged QSOs provide the edition's observed date range.
    // Reuse that evidence for every profile; never guess between two contests.
    for(const auto& record:stillUnassigned) {
        QVector<int> matches;
        for(int i=0;i<result.editions.size();++i) {
            const auto& edition=result.editions[i];
            if(record.stationCallsign.trimmed().toUpper()!=edition.station) continue;
            if(!edition.modes.contains(record.mode.trimmed().toUpper())) continue;
            if(edition.window.start.isValid()) {
                if(record.utc<edition.window.start || record.utc>edition.window.end) continue;
            } else if(record.utc.toUTC().date()<edition.firstDate || record.utc.toUTC().date()>edition.lastDate) continue;
            const auto identity=profileFor(profiles,edition.contestId,record.mode);
            bool scheduled=false; const auto window=windowFor(identity,record.utc,&scheduled);
            if(scheduled && !window.start.isValid()) continue;
            if(!identity.definition.isEmpty() && fitsExchange(record,identity)) matches.push_back(i);
        }
        if(matches.size()==1) {
            auto& edition=result.editions[matches.first()]; edition.records.push_back(record); edition.inferred.push_back(true);
        } else if(matches.size()>1) ++result.ambiguous;
    }
    // Keep each inference marker beside its QSO after chronological sorting.
    for(auto& edition:result.editions) {
        QVector<int> order; for(int i=0;i<edition.records.size();++i) order.push_back(i);
        std::stable_sort(order.begin(),order.end(),[&](int a,int b){return edition.records[a].utc<edition.records[b].utc;});
        const auto oldRecords=edition.records; const auto oldInferred=edition.inferred;
        for(int i=0;i<order.size();++i) { edition.records[i]=oldRecords[order[i]]; edition.inferred[i]=oldInferred[order[i]]; }
    }
    std::stable_sort(result.editions.begin(),result.editions.end(),[](const Edition& a,const Edition& b){return a.firstDate>b.firstDate;});
    return result;
}
// Export works on copies. No inferred identity or normalized value is written
// back to the operator's ADIF log.
inline LogbookEntry prepared(LogbookEntry entry, const QString& contestId) {
    entry.callsign = entry.callsign.trimmed().toUpper();
    entry.stationCallsign = entry.stationCallsign.trimmed().toUpper();
    entry.mode = entry.mode.trimmed().toUpper();
    entry.band = entry.band.trimmed().toLower();
    entry.rstSent = entry.rstSent.trimmed();
    entry.rstReceived = entry.rstReceived.trimmed();
    entry.adifFields.insert("CONTEST_ID", contestId);
    return entry;
}
inline QByteArray cabrillo(const QVector<LogbookEntry>& records, const Profiles& profiles,
                          GenericCabrillo::Options options, bool automatic, QString* error) {
    const auto editions=discover(records,profiles);
    if(editions.editions.size()!=1 || editions.invalidDates || editions.outsidePeriod || editions.ambiguous ||
       editions.editions.first().records.size()!=records.size()) {
        if(error) *error="Select QSOs from one contest edition and station";
        return {};
    }
    const QString id=editions.editions.first().contestId;
    if(id=="CQ-WW-RTTY") {
        CqWwRtty::CabrilloOptions cq;
        cq.operatorCategory=options.categoryOperator; cq.power=options.categoryPower;
        cq.assisted=options.categoryAssisted; cq.band=options.categoryBand;
        cq.name=options.name; cq.email=options.email;
        return CqWwRtty::cabrillo(records,cq,error);
    }
    options.contestId=id;
    for(const auto& entry:records) {
        const auto profile=profileFor(profiles,id,entry.mode);
        const auto bands=profile.definition.value("bands").toArray();
        bool allowed=bands.isEmpty();
        for(const auto& band:bands) if(band.toString().compare(entry.band,Qt::CaseInsensitive)==0) allowed=true;
        if(!allowed) { if(error)*error=entry.callsign+": band not allowed by contest profile"; return {}; }
        if(profile.definition.isEmpty() && profiles.contains(id) && !profiles.value(id).mode.isEmpty() &&
           profiles.value(id).mode.compare(entry.mode,Qt::CaseInsensitive)!=0) {
            if(error)*error=entry.callsign+": mode does not match contest profile"; return {};
        }
    }
    if(automatic) options.exchangeResolver=[&profiles,id](const LogbookEntry& entry,bool sent,QString* why) {
        return automaticExchange(entry,profileFor(profiles,id,entry.mode),sent,why);
    };
    return GenericCabrillo::cabrillo(records,options,error);
}

}
