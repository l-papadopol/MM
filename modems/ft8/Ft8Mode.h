#ifndef FT8MODE_H
#define FT8MODE_H

#include "../../dsp/FrequencyMarker.h"

#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief Metadata for the interoperable Franke-Taylor digital modes supported by MadModem.
 *
 * v1.12 intentionally focuses on real on-air modes FT8 and FT4.  FT4 is not an
 * FT8 timer tweak: it has a different 4-FSK symbol structure, four 4-symbol
 * Costas sync blocks, shorter symbol timing and its own candidate/metric path.
 */
class Ft8Mode
{
public:
    struct Profile
    {
        QString modeName;
        QString shortLabel;
        QString adifMode;
        int slotMs = 15000;
        int cycleMs = 30000;
        int signalMs = 12640;
        bool interoperableCoreAvailable = false;
        bool experimental = false;
        QString note;
    };

    static QString modeName();
    static QStringList allModeNames();
    static bool isFamilyMode(const QString &modeName);
    static Profile profileForMode(const QString &modeName);
    static QVector<FrequencyMarker> frequencyMarkers(int rxHz, int txHz,
                                                     const QString &modeName = QString());
};

#endif // FT8MODE_H
