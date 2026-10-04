#include "Ft8Mode.h"

#include <QColor>

QString Ft8Mode::modeName()
{
    return QStringLiteral("FT8");
}

QStringList Ft8Mode::allModeNames()
{
    // v1.12: keep the operator-facing focus on the two real interoperable cores.
    return QStringList()
           << QStringLiteral("FT8")
           << QStringLiteral("FT4");
}

bool Ft8Mode::isFamilyMode(const QString &modeName)
{
    return allModeNames().contains(modeName.trimmed().toUpper());
}

Ft8Mode::Profile Ft8Mode::profileForMode(const QString &modeName)
{
    const QString key = modeName.trimmed().toUpper();

    Profile p;
    p.modeName = key.isEmpty() ? QStringLiteral("FT8") : key;
    p.shortLabel = p.modeName;
    p.adifMode = p.modeName;
    p.slotMs = 15000;
    p.cycleMs = 30000;
    p.signalMs = 12640;
    p.interoperableCoreAvailable = false;
    p.experimental = false;

    if (key == QStringLiteral("FT4")) {
        p.modeName = QStringLiteral("FT4");
        p.shortLabel = QStringLiteral("FT4");
        p.adifMode = QStringLiteral("FT4");
        p.slotMs = 7500;
        p.cycleMs = 15000;
        p.signalMs = 5040;
        p.interoperableCoreAvailable = true;
        p.note = QStringLiteral("Interoperable FT4 TX/RX core active.");
        return p;
    }

    p.modeName = QStringLiteral("FT8");
    p.shortLabel = QStringLiteral("FT8");
    p.adifMode = QStringLiteral("FT8");
    p.slotMs = 15000;
    p.cycleMs = 30000;
    p.signalMs = 12640;
    p.interoperableCoreAvailable = true;
    p.note = QStringLiteral("Interoperable FT8 TX/RX core active.");
    return p;
}

QVector<FrequencyMarker> Ft8Mode::frequencyMarkers(int rxHz, int txHz, const QString &modeName)
{
    const Profile p = profileForMode(modeName.isEmpty() ? QStringLiteral("FT8") : modeName);
    QVector<FrequencyMarker> markers;

    FrequencyMarker rx;
    rx.frequencyHz = static_cast<double>(rxHz);
    rx.label = p.shortLabel + QStringLiteral(" RX focus");
    rx.color = QColor(0, 190, 70);
    markers.append(rx);

    FrequencyMarker tx;
    tx.frequencyHz = static_cast<double>(txHz);
    tx.label = p.shortLabel + QStringLiteral(" TX marker");
    tx.color = QColor(220, 0, 0);
    markers.append(tx);

    return markers;
}
