// SPDX-License-Identifier: GPL-3.0-or-later
#include "FtDecodeFilter.h"
#include "FtDecodedText.h"
#include "../../dxcc/CtyCountryFile.h"

#include <QRegularExpression>
#include <QSettings>
#include <algorithm>

namespace {
const char *const ruleKeys[] = {"continent", "country", "cqZone", "ituZone", "wap"};

bool accepts(const FtDecodeFilter::Rule &rule, bool matched)
{
    return rule.mode == FtDecodeFilter::All ||
           matched == (rule.mode == FtDecodeFilter::Include);
}

QStringList tokens(const QString &text, bool names)
{
    static const QRegularExpression words(QStringLiteral("[,;\\s]+"));
    static const QRegularExpression countries(QStringLiteral("[,;\\n]+"));
    return text.split(names ? countries : words, Qt::SkipEmptyParts);
}
}

bool FtDecodeFilter::compile(Field *invalid)
{
    static const QSet<QString> continents{"AF", "AN", "AS", "EU", "NA", "OC", "SA"};
    static const QRegularExpression zone(QStringLiteral("^(\\d{1,2})(?:-(\\d{1,2}))?$"));
    static const QRegularExpression wap(QStringLiteral("^WAP-[A-Z]{3}-[A-Z0-9]+$"));
    static const QRegularExpression separator(QStringLiteral("[;|,\\s]+"));
    m_wapReferences.clear();

    for (int n = 0; n < FieldCount; ++n) {
        Rule &rule = rules[n];
        rule.values.clear();
        if (rule.mode == All) continue;
        if (invalid) *invalid = Field(n);
        const auto values = tokens(rule.input, n == Country);
        if (values.isEmpty()) return false;
        for (const QString &input : values) {
            const QString value = input.trimmed().toUpper();
            if (n == Continent) {
                if (!continents.contains(value)) return false;
                rule.values.insert(value);
            } else if (n == Country) {
                const auto entity = CtyCountryFile::instance().lookupEntityNameOrPrefix(value);
                if (!entity.valid || entity.entity.dxcc.isEmpty()) return false;
                rule.values.insert(entity.entity.dxcc);
            } else if (n == Wap) {
                if (value != QStringLiteral("*") && !wap.match(value).hasMatch()) return false;
                rule.values.insert(value);
            } else {
                const auto match = zone.match(value);
                const int first = match.captured(1).toInt();
                const int last = match.captured(2).isEmpty() ? first : match.captured(2).toInt();
                const int maximum = n == CqZone ? 40 : 90;
                if (first < 1 || last < first || last > maximum) return false;
                for (int z = first; z <= last; ++z) rule.values.insert(QString::number(z));
            }
        }
    }

    if (rules[Wap].mode != All) {
        if (invalid) *invalid = Wap;
        const auto lines = wapDirectory.toUpper().split(QLatin1Char('\n'));
        for (const auto &line : lines) {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) continue;
            const auto parts = trimmed.split(separator, Qt::SkipEmptyParts);
            if (parts.size() != 2 || !FtDecodedText::isCallsign(parts[0]) ||
                !wap.match(parts[1]).hasMatch()) return false;
            m_wapReferences.insert(CtyCountryFile::normalizeCallsignForDxcc(parts[0]), parts[1]);
        }
        if (m_wapReferences.isEmpty()) return false;
    }
    return true;
}

bool FtDecodeFilter::active() const
{
    return traffic != AnyTraffic ||
           std::any_of(rules.begin(), rules.end(), [](const Rule &rule) { return rule.mode != All; });
}

bool FtDecodeFilter::hides(const QString &message, const QString &myCall) const
{
    if (!active()) return false;
    const auto parsed = FtDecodedText::parse(message, myCall);
    if (parsed.addressedToMe) return false;
    if ((traffic == OnlyCq && !parsed.cq) || (traffic == HideCq && parsed.cq)) return true;
    const QString call = parsed.cq ? FtDecodedText::cqersCall(parsed.text) : parsed.senderCall;
    if (!myCall.isEmpty() && FtDecodedText::callMatches(call, myCall)) return false;

    if (rules[Continent].mode != All || rules[Country].mode != All ||
        rules[CqZone].mode != All || rules[ItuZone].mode != All) {
        const auto lookup = CtyCountryFile::instance().lookupCallsign(call);
        const auto &entity = lookup.entity;
        const QString continent = entity.dxcc == QLatin1String("13") ? QStringLiteral("AN") : entity.continent;
        if (!accepts(rules[Continent], rules[Continent].values.contains(continent)) ||
            !accepts(rules[Country], rules[Country].values.contains(entity.dxcc)) ||
            !accepts(rules[CqZone], rules[CqZone].values.contains(QString::number(entity.cqZone))) ||
            !accepts(rules[ItuZone], rules[ItuZone].values.contains(QString::number(entity.ituZone)))) return true;
    }

    const Rule &wap = rules[Wap];
    if (wap.mode != All) {
        const auto references = m_wapReferences.values(CtyCountryFile::normalizeCallsignForDxcc(call));
        const bool matched = !references.isEmpty() && (wap.values.contains(QStringLiteral("*")) ||
            std::any_of(references.begin(), references.end(), [&](const QString &ref) { return wap.values.contains(ref); }));
        if (!accepts(wap, matched)) return true;
    }
    return false;
}

void FtDecodeFilter::load(QSettings &settings)
{
    settings.beginGroup(QStringLiteral("ftDecodeFilter"));
    for (int i = 0; i < FieldCount; ++i) {
        const QString prefix = QString::fromLatin1(ruleKeys[i]);
        rules[i].mode = Mode(qBound(0, settings.value(prefix + "/mode").toInt(), int(Exclude)));
        rules[i].input = settings.value(prefix + "/values").toString();
    }
    traffic = Traffic(qBound(0, settings.value(QStringLiteral("traffic")).toInt(), int(HideCq)));
    wapDirectory = settings.value(QStringLiteral("wapDirectory")).toString();
    settings.endGroup();
    if (!compile()) *this = {};
}

void FtDecodeFilter::save(QSettings &settings) const
{
    settings.beginGroup(QStringLiteral("ftDecodeFilter"));
    for (int i = 0; i < FieldCount; ++i) {
        const QString prefix = QString::fromLatin1(ruleKeys[i]);
        settings.setValue(prefix + "/mode", int(rules[i].mode));
        settings.setValue(prefix + "/values", rules[i].input);
    }
    settings.setValue(QStringLiteral("traffic"), int(traffic));
    settings.setValue(QStringLiteral("wapDirectory"), wapDirectory);
    settings.endGroup();
}
