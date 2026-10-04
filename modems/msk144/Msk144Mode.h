#ifndef MSK144MODE_H
#define MSK144MODE_H

#include <QString>
#include <Qt>

class Msk144Mode
{
public:
    static QString modeName() { return QStringLiteral("MSK144"); }
    static QString shortLabel() { return QStringLiteral("MSK144"); }
    static QString adifMode() { return QStringLiteral("MSK144"); }
    static bool isMode(const QString &mode) { return mode.trimmed().compare(modeName(), Qt::CaseInsensitive) == 0; }
    static int defaultRxFrequencyHz() { return 1500; }
    static int defaultTxFrequencyHz() { return 1500; }
    static int defaultPeriodSeconds() { return 15; }
};

#endif // MSK144MODE_H
