#ifndef RADIOTELESCOPEMODE_H
#define RADIOTELESCOPEMODE_H

#include <QString>
#include <Qt>

class RadioTelescopeMode
{
public:
    static QString modeName() { return QStringLiteral("Radio Telescope"); }
    static QString shortLabel() { return QStringLiteral("Radio Telescope"); }
    static bool isMode(const QString &mode) { return mode.trimmed().compare(modeName(), Qt::CaseInsensitive) == 0; }
    static int defaultLowFrequencyHz() { return 1000; }
    static int defaultHighFrequencyHz() { return 2000; }
    static double defaultBeamWidthDeg() { return 15.0; }
    static int defaultDwellMs() { return 1500; }
};

#endif // RADIOTELESCOPEMODE_H
