#include "ContestExchange.h"
#include "../dxcc/CtyCountryFile.h"
#include <QJsonArray>
#include <QRegularExpression>

namespace ContestExport {
namespace {
// Export evaluates geography from the logged station, never today's settings.
// Unknown conditions are errors rather than silently choosing a DX serial.
bool condition(const QJsonObject& when, const LogbookEntry& e, bool* known) {
    bool result=true;
    for(auto it=when.begin();it!=when.end();++it) {
        QString key=it.key(); const bool negate=key.startsWith("not_");
        if(negate) key=key.mid(4);
        bool match=false;
        if(key=="any" || key=="all") {
            match=key=="all";
            for(const auto& part:it.value().toArray()) {
                const bool value=condition(part.toObject(),e,known);
                match=key=="all" ? match && value : match || value;
            }
        } else if(key=="not") match=!condition(it.value().toObject(),e,known);
        else {
            const bool own=key.startsWith("own_");
            if(!own && !key.startsWith("dx_")) { *known=false; return false; }
            const QString call=own?e.stationCallsign:e.callsign;
            const auto entity=CtyCountryFile::instance().lookupCallsign(call);
            if(!entity.valid) { *known=false; return false; }
            key=key.mid(own?4:3);
            if(key=="primary_prefix_in" || key=="continent_in") {
                const QString value=key=="primary_prefix_in"?entity.entity.primaryPrefix:entity.entity.continent;
                for(const auto& item:it.value().toArray()) if(item.toString().compare(value,Qt::CaseInsensitive)==0) match=true;
            } else if(key=="country_name_regex" || key=="call_regex") {
                const auto regex=QRegularExpression(it.value().toString(),QRegularExpression::CaseInsensitiveOption);
                if(!regex.isValid()) { *known=false; return false; }
                match=regex.match(key=="call_regex"?call:entity.entity.name).hasMatch();
            } else { *known=false; return false; }
        }
        result=result && (negate?!match:match);
    }
    return result;
}
QString recorded(const LogbookEntry& e, bool sent, const QString& id) {
    const QString family=e.mode.trimmed().toUpper()=="CW"?"CW":"RTTY";
    auto get=[&](const QString& key) { return e.adifFields.value(key).simplified().toUpper(); };
    const QString own=get("APP_MADMODEM_"+family+(sent?"_TX_":"_RX_")+id);
    if(!own.isEmpty()) return own;
    if(id=="RST") return (sent?e.rstSent:e.rstReceived).trimmed();
    if(id=="SERIAL") return get(sent?"STX":"SRX");
    if(id=="ZONE" || id=="CQZONE") return get(sent?"MY_CQ_ZONE":"CQZ");
    if(id=="GRID") {
        const QString grid=get(sent?"MY_GRIDSQUARE":"GRIDSQUARE");
        return !grid.isEmpty()?grid:(!sent?e.grid.trimmed().toUpper():QString());
    }
    if(id=="NAME") return sent?get("MY_NAME"):(!e.name.isEmpty()?e.name.simplified().toUpper():get("NAME"));
    if(id=="AGE") return get(sent?"MY_AGE":"AGE");
    if(id=="ZONEHQ") return get(sent?"MY_ITU_ZONE":"ITUZ");
    if(id=="STATE" || id=="PROVINCE" || id=="QTH" || id=="LOCATION") {
        const QString state=get(sent?"MY_STATE":"STATE");
        if(!state.isEmpty() || id!="LOCATION") return state;
        return sent?get("MY_COUNTRY"):(!e.country.isEmpty()?e.country.simplified().toUpper():get("COUNTRY"));
    }
    return {};
}
bool valid(const QString& value, const QJsonObject& field) {
    if(value.isEmpty()) return !field.value("required").toBool(true);
    QString expression=field.value("regex").toString();
    const QString type=field.value("type").toString();
    if(expression.isEmpty()) {
        if(type=="rst") expression="^[1-5][1-9][1-9]$";
        else if(type=="serial") expression="^[0-9]{1,6}$";
        else if(type=="zone") expression="^(?:0?[1-9]|[1-3][0-9]|40)$";
        else if(type=="time") expression="^(?:[01][0-9]|2[0-3])[0-5][0-9]$";
        else if(type=="age") expression="^[0-9]{1,3}$";
        else if(type=="locator") expression="^[A-R]{2}[0-9]{2}(?:[A-X]{2})?$";
    }
    if(type=="serial" && value.toInt()<=0) return false;
    if(expression.isEmpty()) return true;
    const QRegularExpression regex(expression,QRegularExpression::CaseInsensitiveOption);
    // ADIF/imported CQ zones may be zero-padded even when the profile's
    // interactive-input regex expects an unpadded number. Keep the recorded
    // representation in Cabrillo while checking its numeric value.
    const QString checked=type=="zone" && QRegularExpression("^[0-9]{1,2}$").match(value).hasMatch()
        ? QString::number(value.toInt()) : value;
    return regex.isValid() && regex.match(checked).hasMatch();
}
}
QString automaticExchange(const LogbookEntry& e, const Identity& profile, bool sent, QString* error) {
    if(error) error->clear();
    auto fail=[&](const QString& value) { if(error)*error=(sent?QStringLiteral("TX "):QStringLiteral("RX "))+value; return QString(); };
    const QString raw=e.adifFields.value(sent?"STX_STRING":"SRX_STRING").simplified().toUpper();
    const auto definitions=profile.definition.value("exchange").toObject().value(sent?"sent":"received").toArray();
    if(definitions.isEmpty()) {
        if(!raw.isEmpty()) return raw;
        const QString serial=recorded(e,sent,"SERIAL");
        if(!serial.isEmpty()) return (recorded(e,sent,"RST")+' '+serial).simplified();
        return fail("exchange missing: use recorded STX_STRING/SRX_STRING or select an ADIF field");
    }
    QVector<QJsonObject> fields;
    for(const auto& definition:definitions) {
        const auto field=definition.toObject(); bool known=true;
        const bool applies=condition(field.value("when").toObject(),e,&known);
        if(!known) return fail("exchange condition cannot be resolved for logged callsigns");
        if(applies) fields.push_back(field);
    }
    // Complete exchange strings are accepted with or without RST. Multiword
    // trailing text (HNY, country) stays intact. Calls used on air by Sprint
    // belong to Cabrillo's callsign columns, not to its exchange columns.
    QStringList tokens=raw.split(' ',Qt::SkipEmptyParts);
    bool hasCalls=false;
    for(const auto& field:fields) if(field.value("type").toString()=="call") hasCalls=true;
    if(hasCalls) {
        tokens.removeAll(e.callsign.trimmed().toUpper());
        tokens.removeAll(e.stationCallsign.trimmed().toUpper());
    }
    int exchangeFields=0;
    for(const auto& field:fields) if(field.value("type").toString()!="call") ++exchangeFields;
    QMap<QString,QString> parsed;
    int cursor=0;
    for(int i=0;i<fields.size();++i) {
        const auto& field=fields[i]; const QString id=field.value("id").toString().toUpper();
        if(id=="MYCALL" || id=="DXCALL") {
            const QString call=id=="MYCALL"?(sent?e.stationCallsign:e.callsign):(sent?e.callsign:e.stationCallsign);
            if(cursor<tokens.size() && tokens[cursor].compare(call,Qt::CaseInsensitive)==0) ++cursor;
            continue;
        }
        if(cursor>=tokens.size()) continue;
        if(id=="RST" && (tokens.size()<exchangeFields ||
            !QRegularExpression("^[1-5][1-9][1-9]$").match(tokens[cursor]).hasMatch())) continue;
        QString value=tokens[cursor++];
        if(i==fields.size()-1 && (field.value("type").toString()=="text" || field.value("type").toString()=="name")) {
            while(cursor<tokens.size()) value+=' '+tokens[cursor++];
        }
        parsed.insert(id,value);
    }
    QStringList output;
    bool usedParsed=false;
    for(const auto& field:fields) {
        const QString id=field.value("id").toString().toUpper();
        if(id=="MYCALL" || id=="DXCALL") continue;
        QString value=recorded(e,sent,id);
        if(value.isEmpty()) { value=parsed.value(id); usedParsed=usedParsed || !value.isEmpty(); }
        if(value.isEmpty() && id=="EXCHANGE" && profile.id=="ARRL-DX-CW") {
            const auto entity=CtyCountryFile::instance().lookupCallsign(sent?e.stationCallsign:e.callsign);
            if(entity.valid) value=e.adifFields.value(entity.entity.primaryPrefix=="K" || entity.entity.primaryPrefix=="VE"
                ? (sent?"MY_STATE":"STATE") : (sent?"TX_PWR":"RX_PWR")).simplified().toUpper();
        }
        const int width=profile.definition.value("export").toObject().value("field_widths").toObject().value(id).toInt();
        if(field.value("type").toString()=="locator" && width==4 && value.size()>=4) value=value.left(4);
        if(!valid(value,field)) return fail(id+": missing or invalid recorded value");
        if(!value.isEmpty()) output.push_back(value);
    }
    if(usedParsed && cursor<tokens.size()) return fail("exchange contains unmatched fields");
    if(output.isEmpty()) return fail("exchange is empty");
    return output.join(' ');
}
}
