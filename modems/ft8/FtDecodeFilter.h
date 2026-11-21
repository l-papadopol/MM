// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMultiHash>
#include <QSet>
#include <QString>
#include <array>

class QSettings;

class FtDecodeFilter
{
public:
    enum Field { Continent, Country, CqZone, ItuZone, Wap, FieldCount };
    enum Mode { All, Include, Exclude };
    enum Traffic { AnyTraffic, OnlyCq, HideCq };

    struct Rule {
        Mode mode = All;
        QString input;
        QSet<QString> values;
    };

    std::array<Rule, FieldCount> rules;
    Traffic traffic = AnyTraffic;
    QString wapDirectory;

    bool compile(Field *invalid = nullptr);
    bool active() const;
    bool hides(const QString &message, const QString &myCall) const;
    void load(QSettings &settings);
    void save(QSettings &settings) const;

private:
    QMultiHash<QString, QString> m_wapReferences;
};
