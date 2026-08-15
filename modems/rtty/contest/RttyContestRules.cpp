#include "RttyContestRules.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

namespace {
QString normalizedId(const QString &value)
{
    return value.trimmed().toLower();
}
}

QList<RttyContestFieldRule> RttyContestRules::parseFields(const QJsonArray &array)
{
    QList<RttyContestFieldRule> out;
    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        RttyContestFieldRule field;
        field.id = object.value(QStringLiteral("id")).toString().trimmed().toUpper();
        field.label = object.value(QStringLiteral("label")).toString().trimmed();
        field.type = object.value(QStringLiteral("type")).toString().trimmed().toLower();
        field.source = object.value(QStringLiteral("source")).toString().trimmed();
        field.regex = object.value(QStringLiteral("regex")).toString().trimmed();
        field.required = object.value(QStringLiteral("required")).toBool(true);
        field.when = object.value(QStringLiteral("when")).toObject();
        if (!field.id.isEmpty()) {
            if (field.label.isEmpty()) {
                field.label = field.id;
            }
            out.push_back(field);
        }
    }
    return out;
}

QList<RttyContestMacroRule> RttyContestRules::parseMacros(const QJsonArray &array)
{
    QList<RttyContestMacroRule> out;
    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        RttyContestMacroRule macro;
        macro.label = object.value(QStringLiteral("label")).toString().trimmed();
        macro.text = object.value(QStringLiteral("text")).toString();
        if (!macro.label.isEmpty()) {
            out.push_back(macro);
        }
    }
    return out;
}

RttyContestScoreRule RttyContestRules::parseScoring(const QJsonObject &object)
{
    RttyContestScoreRule score;
    score.supported = object.value(QStringLiteral("supported")).toBool(true);
    score.formula = object.value(QStringLiteral("formula")).toString(QStringLiteral("POINTS * MULTS")).trimmed().toUpper();
    score.note = object.value(QStringLiteral("note")).toString().trimmed();

    const QJsonArray points = object.value(QStringLiteral("qso_points")).toArray();
    for (const QJsonValue &value : points) {
        if (value.isObject()) {
            score.qsoPointRules.push_back(value.toObject());
        }
    }

    const QJsonArray multipliers = object.value(QStringLiteral("multipliers")).toArray();
    for (const QJsonValue &value : multipliers) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        RttyContestMultiplierRule multiplier;
        multiplier.id = object.value(QStringLiteral("id")).toString().trimmed();
        multiplier.label = object.value(QStringLiteral("label")).toString().trimmed();
        multiplier.source = object.value(QStringLiteral("source")).toString().trimmed().toLower();
        multiplier.scope = object.value(QStringLiteral("scope")).toString(QStringLiteral("band")).trimmed().toLower();
        multiplier.aggregate = object.value(QStringLiteral("aggregate")).toString(QStringLiteral("mults")).trimmed().toLower();
        multiplier.options = object.value(QStringLiteral("options")).toObject();
        multiplier.when = object.value(QStringLiteral("when")).toObject();
        multiplier.bandWeights = object.value(QStringLiteral("band_weights")).toObject();
        if (!multiplier.source.isEmpty()) {
            if (multiplier.id.isEmpty()) {
                multiplier.id = multiplier.source;
            }
            if (multiplier.label.isEmpty()) {
                multiplier.label = multiplier.id;
            }
            score.multipliers.push_back(multiplier);
        }
    }
    return score;
}

bool RttyContestRules::load(const QString &path, QString *errorMessage)
{
    m_profiles.clear();
    m_sourcePath.clear();
    m_updatedUtc.clear();
    m_schemaVersion = 0;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Cannot open %1: %2").arg(path, file.errorString());
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Invalid rtty_rules JSON at offset %1: %2")
                                .arg(parseError.offset)
                                .arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = document.object();
    m_schemaVersion = root.value(QStringLiteral("schema")).toInt();
    if (m_schemaVersion != 1) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unsupported rtty_rules schema %1 (expected 1)").arg(m_schemaVersion);
        }
        return false;
    }
    m_updatedUtc = root.value(QStringLiteral("updated_utc")).toString().trimmed();

    const QJsonArray profiles = root.value(QStringLiteral("profiles")).toArray();
    for (const QJsonValue &value : profiles) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        RttyContestProfile profile;
        profile.id = normalizedId(object.value(QStringLiteral("id")).toString());
        profile.name = object.value(QStringLiteral("name")).toString().trimmed();
        profile.cabrilloId = object.value(QStringLiteral("cabrillo_id")).toString().trimmed().toUpper();
        profile.status = object.value(QStringLiteral("status")).toString(QStringLiteral("active")).trimmed().toLower();
        profile.dupeScope = object.value(QStringLiteral("dupe_scope")).toString(QStringLiteral("band")).trimmed().toLower();
        for (const QJsonValue &periodValue : object.value(QStringLiteral("periods")).toArray()) {
            if (!periodValue.isObject()) {
                continue;
            }
            const QJsonObject periodObject = periodValue.toObject();
            RttyContestPeriodRule period;
            period.id = periodObject.value(QStringLiteral("id")).toString().trimmed();
            period.startUtc = periodObject.value(QStringLiteral("start_utc")).toString().trimmed();
            period.endUtc = periodObject.value(QStringLiteral("end_utc")).toString().trimmed();
            if (!period.id.isEmpty() && !period.startUtc.isEmpty() && !period.endUtc.isEmpty()) {
                profile.periods.push_back(period);
            }
        }
        for (const QJsonValue &band : object.value(QStringLiteral("bands")).toArray()) {
            const QString value = band.toString().trimmed().toLower();
            if (!value.isEmpty()) {
                profile.bands.push_back(value);
            }
        }

        const QJsonObject serial = object.value(QStringLiteral("serial")).toObject();
        profile.serialEnabled = serial.value(QStringLiteral("enabled")).toBool(false);
        profile.serialStart = qMax(1, serial.value(QStringLiteral("start")).toInt(1));
        profile.serialWidth = qBound(1, serial.value(QStringLiteral("width")).toInt(3), 6);
        profile.serialScope = serial.value(QStringLiteral("scope")).toString(QStringLiteral("contest")).trimmed().toLower();

        const QJsonObject exchange = object.value(QStringLiteral("exchange")).toObject();
        profile.sentFields = parseFields(exchange.value(QStringLiteral("sent")).toArray());
        profile.receivedFields = parseFields(exchange.value(QStringLiteral("received")).toArray());
        profile.macros = parseMacros(object.value(QStringLiteral("macros")).toArray());
        profile.scoring = parseScoring(object.value(QStringLiteral("scoring")).toObject());

        const QJsonObject source = object.value(QStringLiteral("source")).toObject();
        profile.sourceUrl = source.value(QStringLiteral("url")).toString().trimmed();
        profile.sourceNote = source.value(QStringLiteral("note")).toString().trimmed();

        if (profile.id.isEmpty() || profile.name.isEmpty()) {
            continue;
        }
        bool duplicate = false;
        for (const RttyContestProfile &existing : m_profiles) {
            if (existing.id == profile.id) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            m_profiles.push_back(profile);
        }
    }

    if (m_profiles.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("rtty_rules contains no usable contest profiles");
        }
        return false;
    }

    m_sourcePath = path;
    return true;
}

const RttyContestProfile *RttyContestRules::profileById(const QString &id) const
{
    const QString key = normalizedId(id);
    for (const RttyContestProfile &profile : m_profiles) {
        if (profile.id == key) {
            return &profile;
        }
    }
    return nullptr;
}
