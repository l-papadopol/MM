#ifndef RTTYCONTESTRULES_H
#define RTTYCONTESTRULES_H

#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QString>
#include <QStringList>

struct RttyContestFieldRule
{
    QString id;
    QString label;
    QString type;
    QString source;
    QString regex;
    bool required = true;
    QJsonObject when;
};

struct RttyContestMacroRule
{
    QString label;
    QString text;
};

struct RttyContestPeriodRule
{
    QString id;
    QString startUtc;
    QString endUtc;
};

struct RttyContestMultiplierRule
{
    QString id;
    QString label;
    QString source;
    QString scope = QStringLiteral("band");
    QString aggregate = QStringLiteral("mults");
    QJsonObject options;
    QJsonObject when;
    QJsonObject bandWeights;
};

struct RttyContestScoreRule
{
    bool supported = true;
    QString formula = QStringLiteral("POINTS * MULTS");
    QList<QJsonObject> qsoPointRules;
    QList<RttyContestMultiplierRule> multipliers;
    QString note;
};

struct RttyContestProfile
{
    QString id;
    QString name;
    QString cabrilloId;
    QString status = QStringLiteral("active");
    QStringList bands;
    QString dupeScope = QStringLiteral("band");
    QList<RttyContestPeriodRule> periods;

    bool serialEnabled = false;
    int serialStart = 1;
    int serialWidth = 3;
    QString serialScope = QStringLiteral("contest");

    QList<RttyContestFieldRule> sentFields;
    QList<RttyContestFieldRule> receivedFields;
    QList<RttyContestMacroRule> macros;
    RttyContestScoreRule scoring;

    QString sourceUrl;
    QString sourceNote;
};

class RttyContestRules
{
public:
    bool load(const QString &path, QString *errorMessage = nullptr);

    QString sourcePath() const { return m_sourcePath; }
    QString updatedUtc() const { return m_updatedUtc; }
    int schemaVersion() const { return m_schemaVersion; }
    const QList<RttyContestProfile> &profiles() const { return m_profiles; }
    const RttyContestProfile *profileById(const QString &id) const;

private:
    static QList<RttyContestFieldRule> parseFields(const QJsonArray &array);
    static QList<RttyContestMacroRule> parseMacros(const QJsonArray &array);
    static RttyContestScoreRule parseScoring(const QJsonObject &object);

    QString m_sourcePath;
    QString m_updatedUtc;
    int m_schemaVersion = 0;
    QList<RttyContestProfile> m_profiles;
};

#endif // RTTYCONTESTRULES_H
